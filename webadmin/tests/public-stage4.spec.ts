import { expect, test } from "@playwright/test";

// Browser checks exercise the standalone Stage IV shell with intercepted,
// synthetic APIs. They do not call or authenticate against the actual VPS.
test("the public shell requires password then passkey before showing an admin session", async ({ page }) => {
  let verified = false;
  const paths: string[] = [];
  await page.addInitScript(() => {
    Object.defineProperty(navigator, "credentials", { configurable: true, value: {
      get: async () => ({ toJSON: () => ({ id: "synthetic-passkey", rawId: "c3ludGhldGlj",
        type: "public-key", response: { authenticatorData: "c3ludGhldGlj",
          signature: "c3ludGhldGlj", clientDataJSON: "c3ludGhldGlj", userHandle: null },
        clientExtensionResults: {} }) }),
    } });
  });
  await page.route("**/stage4/v1/**", async route => {
    const endpoint = new URL(route.request().url()).pathname;
    paths.push(endpoint);
    const data = route.request().postDataJSON?.() as Record<string, unknown> | undefined;
    let code = 200, response: unknown;
    switch (endpoint) {
      case "/stage4/v1/session":
        code = verified ? 200 : 401;
        response = verified ? { schema: 1, authenticated: true, role: "administrator", csrf: "s".repeat(64) } :
          { error: "unauthorized" }; break;
      case "/stage4/v1/challenge": response = { schema: 1, csrf: "a".repeat(64) }; break;
      case "/stage4/v1/login/options":
        expect(data).toEqual({ actor_id: "7", password: "synthetic-private-password" });
        expect(route.request().headers()["x-dc24h-stage4-csrf"]).toBe("a".repeat(64));
        response = { schema: 1, csrf: "b".repeat(64),
          options: { challenge: "c3ludGhldGljLWNoYWxsZW5nZS0xMjM0NTY3ODkw",
            rpId: "server-747585786.dc24h.eu", userVerification: "required" } }; break;
      case "/stage4/v1/login/finish":
        expect(data).toHaveProperty("response");
        expect(route.request().headers()["x-dc24h-stage4-csrf"]).toBe("b".repeat(64));
        verified = true;
        response = { schema: 1, authenticated: true, role: "owner", csrf: "s".repeat(64) }; break;
      case "/stage4/v1/summary": response = { schema: 1, summary: { users: { online: 2 } } }; break;
      case "/stage4/v1/logout": verified = false; response = { schema: 1, authenticated: false }; break;
      default: code = 404; response = { error: "not_found" };
    }
    await route.fulfill({ status: code, contentType: "application/json", body: JSON.stringify(response) });
  });
  await page.goto("/public-stage4/index.html");
  await expect(page.getByRole("heading", { name: "Logowanie OWNER / administrator" })).toBeVisible();
  await expect(page.locator("#dashboard")).toBeHidden();
  await page.locator("#actor").fill("7");
  await page.locator("#password").fill("synthetic-private-password");
  await page.getByRole("button", { name: "Zaloguj hasłem i passkey" }).click();
  await expect(page.locator("#dashboard")).toBeVisible();
  await expect(page.locator("#role")).toHaveText("Rola: owner");
  await expect(page.locator("#password")).toHaveValue("");
  await page.getByRole("button", { name: "Odśwież stan huba" }).click();
  await expect(page.locator("#result")).toContainText('"online": 2');
  await page.getByRole("button", { name: "Wyloguj" }).click();
  await expect(page.locator("#login")).toBeVisible();
  expect(paths).toContain("/stage4/v1/login/options");
  expect(paths).toContain("/stage4/v1/login/finish");
});

test("password-only failure never reveals dashboard, token or secret", async ({ page }) => {
  await page.route("**/stage4/v1/**", async route => {
    const path = new URL(route.request().url()).pathname;
    await route.fulfill({ status: path.endsWith("/challenge") ? 200 : 403,
      contentType: "application/json",
      body: JSON.stringify(path.endsWith("/challenge") ? { csrf: "a".repeat(64) } : { error: "login_refused" }) });
  });
  await page.goto("/public-stage4/index.html");
  await page.locator("#actor").fill("7");
  await page.locator("#password").fill("incorrect-private-password");
  await page.getByRole("button", { name: "Zaloguj hasłem i passkey" }).click();
  await expect(page.locator("#dashboard")).toBeHidden();
  await expect(page.locator("#password")).toHaveValue("");
  await expect(page.locator("#message")).toContainText("Operacja odrzucona");
  expect(await page.locator("body").innerText()).not.toContain("incorrect-private-password");
});
