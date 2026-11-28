import { expect, test } from "@playwright/test";

test("OTP-authenticated Owner sees actual read-only C++ account statistics in React", async ({ page }) => {
  let authenticated = true;
  const paths: string[] = [];
  await page.route("**/stage4/v1/**", async route => {
    const path = new URL(route.request().url()).pathname;
    paths.push(path);
    let status = 200;
    let payload: unknown = {};
    if (path.endsWith("/session")) {
      status = authenticated ? 200 : 401;
      payload = authenticated ? { schema: 1, authenticated: true, role: "owner", csrf: "a".repeat(64) } :
        { error: "unauthorized" };
    } else if (path.endsWith("/identity-summary")) {
      payload = { schema: 1, data: { schema: 1, source: "stage4-account-service",
        accounts: { total: 8, active: 7, owners: 1, administrators: 2, registered: 5 },
        verified_emails: 7 } };
    } else if (path.endsWith("/admin")) {
      expect(route.request().method()).toBe("POST");
      expect(route.request().headers()["x-dc24h-stage4-csrf"]).toBe("a".repeat(64));
      const input = route.request().postDataJSON();
      expect(input).toEqual({ operation: "accounts.list", after: "0" });
      payload = { schema: 1, data: { accounts: [{ id: "1", name: "BrowserFixture", role: "registered",
        status: "active", revision: "1", created_at: 1, updated_at: 1, last_login_at: null }],
        maintenance: true, next_after: null } };
    } else if (path.endsWith("/logout")) {
      authenticated = false;
      payload = { schema: 1, authenticated: false };
    } else {
      status = 404;
      payload = { error: "not_found" };
    }
    await route.fulfill({ status, contentType: "application/json", body: JSON.stringify(payload) });
  });
  await page.goto("/public-stage4/index.html");
  await expect(page.locator("#dashboard")).toBeVisible();
  await expect(page.getByRole("heading", { name: "Konta użytkowników" })).toBeVisible();
  await expect(page.getByText("Łącznie kont", { exact: true })).toBeVisible();
  await expect(page.locator(".api-stats")).toContainText("8");
  await expect(page.getByRole("heading", { name: "Struktura ról" })).toBeVisible();
  await expect(page.getByRole("img", { name: /87,5.*kont ze zweryfikowanym/ })).toBeVisible();
  expect(paths).toContain("/stage4/v1/identity-summary");
  expect(paths).not.toContain("/stage4/v1/telemetry");
  await page.getByRole("button", { name: "Konta", exact: true }).click();
  await page.locator(".api-panel button").filter({ hasText: /Pierwsza strona/ }).click();
  await expect(page.getByRole("button", { name: "BrowserFixture" })).toBeVisible();
  expect(paths).toContain("/stage4/v1/admin");
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByLabel("Widok", { exact: true }).selectOption("overview");
  await expect(page.getByRole("heading", { name: "Przegląd", exact: true })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(390);
  await page.getByRole("button", { name: "Wyloguj" }).click();
  await expect(page.locator("#login")).toBeVisible();
  await expect(page.locator(".api-stats")).toHaveCount(0);
});

test("Registered role never gets read-only admin API UI", async ({ page }) => {
  const paths: string[] = [];
  await page.route("**/stage4/v1/**", async route => {
    const path = new URL(route.request().url()).pathname;
    paths.push(path);
    await route.fulfill({ status: path.endsWith("/session") ? 200 : 403,
      contentType: "application/json", body: JSON.stringify(path.endsWith("/session") ?
        { schema: 1, authenticated: true, role: "registered", csrf: "b".repeat(64) } :
        { error: "forbidden" }) });
  });
  await page.goto("/public-stage4/index.html");
  await expect(page.getByRole("heading", { name: "Moje konto", exact: true })).toBeVisible();
  await expect(page.getByRole("navigation", { name: "Administracja" })).toHaveCount(0);
  expect(paths).not.toContain("/stage4/v1/identity-summary");
});

// Synthetic browser API: never send test codes or credentials to the VPS.
test("public WebAdmin waits for a mailed code after C++ password proof", async ({ page }) => {
  let authenticated = false;
  const paths: string[] = [];
  await page.route("**/stage4/v1/**", async route => {
    const path = new URL(route.request().url()).pathname;
    paths.push(path);
    const data = route.request().postDataJSON?.() as Record<string, unknown> | undefined;
    let code = 200, response: unknown;
    switch (path) {
      case "/stage4/v1/session":
        code = authenticated ? 200 : 401;
        response = authenticated ? { authenticated: true, role: "owner", csrf: "c".repeat(64) } :
          { error: "unauthorized" }; break;
      case "/stage4/v1/challenge": response = { schema: 1, csrf: "a".repeat(64) }; break;
      case "/stage4/v1/login/send":
        expect(data).toEqual({ username: "Administrator", password: "synthetic-password" });
        expect(route.request().headers()["x-dc24h-stage4-csrf"]).toBe("a".repeat(64));
        response = { schema: 1, sent: true, csrf: "b".repeat(64), expires_in: 300 }; break;
      case "/stage4/v1/login/verify":
        expect(data).toEqual({ code: "12345678" });
        expect(route.request().headers()["x-dc24h-stage4-csrf"]).toBe("b".repeat(64));
        authenticated = true;
        response = { schema: 1, authenticated: true, role: "owner", csrf: "c".repeat(64) }; break;
      case "/stage4/v1/summary": response = { schema: 1, summary: { account_service: "available" } }; break;
      case "/stage4/v1/logout": authenticated = false; response = { schema: 1, authenticated: false }; break;
      default: code = 404; response = { error: "not_found" };
    }
    await route.fulfill({ status: code, contentType: "application/json", body: JSON.stringify(response) });
  });
  await page.goto("/public-stage4/index.html");
  await expect(page.locator("#dashboard")).toBeHidden();
  await page.locator("#actor").fill("Administrator");
  await page.locator("#password").fill("synthetic-password");
  await page.getByRole("button", { name: "Wyślij kod e-mail" }).click();
  await expect(page.locator("#verification")).toBeVisible();
  await expect(page.locator("#dashboard")).toBeHidden();
  await expect(page.locator("#password")).toHaveValue("");
  await page.locator("#email-code").fill("12345678");
  await page.getByRole("button", { name: "Potwierdź kod" }).click();
  await expect(page.locator("#dashboard")).toBeVisible();
  await expect(page.locator("#role")).toHaveText("Właściciel");
  await expect(page.locator("#email-code")).toHaveValue("");
  await page.getByRole("button", { name: /Moje konto/ }).click();
  await expect(page.getByRole("dialog", { name: "Moje konto i sesje" })).toBeVisible();
  await page.getByRole("button", { name: "Odśwież stan usługi kont" }).click();
  await expect(page.locator("#result")).toHaveText("Usługa kont działa.");
  await page.keyboard.press("Escape");
  await expect(page.getByRole("dialog", { name: "Moje konto i sesje" })).toBeHidden();
  await page.getByRole("button", { name: "Wyloguj" }).click();
  await expect(page.locator("#login")).toBeVisible();
  expect(paths).toContain("/stage4/v1/login/send");
  expect(paths).toContain("/stage4/v1/login/verify");
});

test("wrong password never sends a verification email or exposes the dashboard", async ({ page }) => {
  await page.route("**/stage4/v1/**", async route => {
    const path = new URL(route.request().url()).pathname;
    await route.fulfill({ status: path.endsWith("/challenge") ? 200 : 403,
      contentType: "application/json",
      body: JSON.stringify(path.endsWith("/challenge") ? { csrf: "a".repeat(64) } : { error: "login_refused" }) });
  });
  await page.goto("/public-stage4/index.html");
  await page.locator("#actor").fill("Administrator");
  await page.locator("#password").fill("incorrect-password");
  await page.getByRole("button", { name: "Wyślij kod e-mail" }).click();
  await expect(page.locator("#dashboard")).toBeHidden();
  await expect(page.locator("#verification")).toBeHidden();
  await expect(page.locator("#password")).toHaveValue("");
  expect(await page.locator("body").innerText()).not.toContain("incorrect-password");
});

test("registration requires mailbox proof and a separate fresh login", async ({ page }) => {
  let verified = false;
  let loggedIn = false;
  await page.route("**/stage4/v1/**", async route => {
    const endpoint = new URL(route.request().url()).pathname.split("/stage4/v1/")[1];
    const body = route.request().postDataJSON?.() as Record<string, unknown> | undefined;
    let status = 200;
    let payload: object;
    switch (endpoint) {
      case "session":
        status = 401;
        payload = { error: "unauthorized" };
        break;
      case "challenge": payload = { csrf: "a".repeat(64) }; break;
      case "register/send":
        expect(body).toEqual({ username: "SyntheticUser", email: "fixture@example.test",
          password: "synthetic registration phrase" });
        payload = { csrf: "b".repeat(64) };
        break;
      case "register/verify":
        expect(body).toEqual({ code: "87654321" });
        expect(route.request().headers()["x-dc24h-stage4-csrf"]).toBe("b".repeat(64));
        verified = true;
        payload = { registered: true };
        break;
      case "login/send":
        expect(verified).toBe(true);
        expect(body?.username).toBe("SyntheticUser");
        payload = { csrf: "c".repeat(64) };
        break;
      case "login/verify":
        expect(body).toEqual({ code: "12345678" });
        expect(route.request().headers()["x-dc24h-stage4-csrf"]).toBe("c".repeat(64));
        loggedIn = true;
        payload = { authenticated: true, role: "registered", csrf: "d".repeat(64) };
        break;
      default:
        status = 404;
        payload = { error: "not_found" };
    }
    await route.fulfill({ status, contentType: "application/json", body: JSON.stringify(payload) });
  });
  await page.goto("/public-stage4/index.html");
  await page.getByRole("button", { name: "Załóż konto" }).click();
  await page.locator("#new-username").fill("SyntheticUser");
  await page.locator("#email").fill("fixture@example.test");
  await page.locator("#new-password").fill("synthetic registration phrase");
  await page.getByRole("button", { name: "Wyślij kod rejestracyjny" }).click();
  await expect(page.locator("#dashboard")).toBeHidden();
  await page.locator("#email-code").fill("87654321");
  await page.getByRole("button", { name: "Potwierdź kod" }).click();
  await expect(page.locator("#login")).toBeVisible();
  expect(loggedIn).toBe(false);
  await page.locator("#actor").fill("SyntheticUser");
  await page.locator("#password").fill("synthetic registration phrase");
  await page.getByRole("button", { name: "Wyślij kod e-mail" }).click();
  await page.locator("#email-code").fill("12345678");
  await page.getByRole("button", { name: "Potwierdź kod" }).click();
  await expect(page.locator("#role")).toHaveText("Zarejestrowany");
  await expect(page.locator("#refresh")).toBeHidden();
});
