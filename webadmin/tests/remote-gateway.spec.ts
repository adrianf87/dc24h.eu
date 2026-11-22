import { test, expect } from "@playwright/test";

const sessionCsrf = "d".repeat(64);
const snapshot = {
  schema: 1, mode: "read-only",
  server: { version: "beta-0.2.4", running: true, uptime_seconds: 123 },
  network: { active_connections: 5, accepted_connections: 22 },
  users: { online: 4, peak_online: 7 },
  chat: { received: 10, rejected: 1 },
};

test("remote read-only screen needs a session, supports EN/PL and never offers mutations", async ({ page }) => {
  let loggedIn = false;
  await page.route("**/remote/v1/session", async (route) => {
    await route.fulfill({ status: loggedIn ? 200 : 401, contentType: "application/json",
      body: loggedIn ? JSON.stringify({ schema: 1, authenticated: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }) :
        '{"error":"unauthorized"}' });
  });
  await page.route("**/remote/v1/challenge", async (route) => {
    await route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ schema: 1, csrf: "e".repeat(64) }) });
  });
  await page.route("**/remote/v1/login", async (route) => {
    const body = route.request().postDataJSON() as {actor_id:string;password:string};
    expect(body.actor_id).toBe("7");
    expect(body.password).toBe("example-proof");
    expect(route.request().headers()["x-dc24h-remote-csrf"]).toBe("e".repeat(64));
    loggedIn = true;
    await route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ schema: 1, authenticated: true, mode: "read-only", csrf: sessionCsrf, expires_in: 600 }) });
  });
  await page.route("**/remote/v1/summary", async (route) => {
    await route.fulfill({ status: 200, contentType: "application/json", body: JSON.stringify(snapshot) });
  });
  await page.route("**/remote/v1/logout", async (route) => {
    expect(route.request().headers()["x-dc24h-remote-csrf"]).toBe(sessionCsrf);
    loggedIn = false;
    await route.fulfill({ status: 200, contentType: "application/json", body: '{"schema":1,"authenticated":false}' });
  });
  await page.goto("/remote.html");
  await expect(page.getByText("Sign in to read approved hub metrics.")).toBeVisible();
  await page.getByRole("button", { name: "Polski" }).click();
  await expect(page.getByText("Zaloguj się, aby odczytać zatwierdzone dane huba.")).toBeVisible();
  await page.getByLabel("ID konta administratora").fill("7");
  await page.getByLabel("Hasło konta").fill("example-proof");
  await page.getByRole("button", { name: "Zaloguj" }).click();
  await expect(page.getByText("beta-0.2.4")).toBeVisible();
  await expect(page.getByText("Aktywne połączenia")).toBeVisible();
  await expect(page.getByText("Użytkownicy online")).toBeVisible();
  for (const title of ["Start", "Stop", "Restart", "Edit", "Restore", "Grant"]) {
    await expect(page.getByRole("button", { name: title, exact: true })).toHaveCount(0);
  }
  await page.getByRole("button", { name: "Wyloguj" }).click();
  await expect(page.getByText("Zaloguj się, aby odczytać zatwierdzone dane huba.")).toBeVisible();
});

test("remote view fails closed on unavailable API and does not render stale metrics", async ({ page }) => {
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }),
  }));
  await page.route("**/remote/v1/summary", async (route) => route.fulfill({
    status: 503, contentType: "application/json", body: '{"error":"unavailable"}',
  }));
  await page.goto("/remote.html");
  await expect(page.getByText(/The gateway or hub API is unavailable/)).toBeVisible();
  await expect(page.getByText("Active connections")).toHaveCount(0);
});

test("remote view clears previously displayed metrics after session expiration", async ({ page }) => {
  let expired = false;
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }),
  }));
  await page.route("**/remote/v1/summary", async (route) => route.fulfill({
    status: expired ? 401 : 200, contentType: "application/json",
    body: expired ? '{"error":"unauthorized"}' : JSON.stringify(snapshot),
  }));
  await page.goto("/remote.html");
  await expect(page.getByText("Active connections")).toBeVisible();
  expired = true;
  await page.getByRole("button", { name: "Refresh" }).click();
  await expect(page.getByText("Your session expired or was revoked. Sign in again.")).toBeVisible();
  await expect(page.getByText("Active connections")).toHaveCount(0);
  await expect(page.getByRole("button", { name: "Sign out" })).toHaveCount(0);
});
