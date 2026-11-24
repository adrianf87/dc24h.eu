import { test, expect, type Route } from "@playwright/test";

const sessionCsrf = "d".repeat(64);
const snapshot = {
  schema: 1, mode: "read-only",
  server: { version: "beta-0.6.16", running: true, uptime_seconds: 123 },
  network: { active_connections: 5, accepted_connections: 22 },
  users: { online: 4, peak_online: 7 },
  chat: { received: 10, rejected: 1 },
};

test("a successful logout in one tab clears a sibling's private metrics and actions", async ({ page, context }) => {
  await context.route("**/remote/v1/session", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify({
      schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 600,
    }),
  }));
  await context.route("**/remote/v1/summary", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify(snapshot),
  }));
  await context.route("**/remote/v1/logout", route => route.fulfill({
    status: 200, contentType: "application/json", body: '{"schema":1,"authenticated":false}',
  }));
  const sibling = await context.newPage();
  try {
    await Promise.all([page.goto("/remote.html"), sibling.goto("/remote.html")]);
    await expect(page.getByText("Active connections")).toBeVisible();
    await expect(sibling.getByText("Active connections")).toBeVisible();
    await page.getByRole("button", { name: "Sign out" }).click();
    await expect(page.getByText("Sign in to read approved hub metrics.")).toBeVisible();
    await expect(sibling.getByText("Your session expired or was revoked. Sign in again.")).toBeVisible();
    await expect(sibling.getByText("Active connections")).toHaveCount(0);
    await expect(sibling.getByRole("button", { name: "Sign out" })).toHaveCount(0);
  } finally { await sibling.close(); }
});

test("private bilingual incident timeline remains keyboard-readable on mobile without privileged writes", async ({ page }) => {
  await page.setViewportSize({ width: 375, height: 812 });
  const writes: string[] = [];
  page.on("request", request => { if (request.method() !== "GET") writes.push(request.url()); });
  await page.route("**/remote/v1/session", route => route.fulfill({ status: 200,
    contentType: "application/json", body: JSON.stringify({ schema: 1, authenticated: true,
      mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 600 }) }));
  await page.route("**/remote/v1/summary", route => route.fulfill({ status: 200,
    contentType: "application/json", body: JSON.stringify({ ...snapshot,
      observation: { schema: 1, source_age_ms: 500 }, incidents: {
        schema: 1, state: "observed", events: [
          { kind: "hub_stopped", age_seconds: 3 },
          { kind: "transport_refusals", age_seconds: 1 },
        ], alerts: [{ kind: "transport_refusals", age_seconds: 1 }],
      },
    }) }));
  await page.goto("/remote.html");
  const incidentRegion = page.getByRole("region", { name: "Private incident overview" });
  await expect(page.locator("main.remote-shell")).toHaveAttribute("lang", "en");
  await expect(incidentRegion.getByRole("heading", { name: "Recent aggregate service events" })).toBeVisible();
  await expect(incidentRegion.getByText("Transport security refusal threshold reached")).toBeVisible();
  await expect(incidentRegion.getByText("Aggregate security signals: 1")).toBeVisible();
  await expect(incidentRegion.getByText(/\b\d+ s\b/).first()).toBeVisible();
  await page.getByRole("button", { name: "Polski" }).click();
  await expect(page.locator("main.remote-shell")).toHaveAttribute("lang", "pl");
  const plRegion = page.getByRole("region", { name: "Prywatny przegląd incydentów" });
  await expect(plRegion.getByRole("heading", { name: "Ostatnie zbiorcze zdarzenia usługi" })).toBeVisible();
  await page.getByRole("button", { name: "Potwierdź lokalnie" }).focus();
  await page.keyboard.press("Enter");
  await expect(page.getByText("Potwierdzono tylko na tym ekranie; nie wysłano żadnej akcji do serwera.")).toBeVisible();
  expect(writes).toEqual([]);
  for (const title of ["Start", "Stop", "Restart", "Grant"]) {
    await expect(page.getByRole("button", { name: title, exact: true })).toHaveCount(0);
  }
});

test("remote read-only screen needs a session, supports EN/PL and never offers hub-control mutations", async ({ page }) => {
  let loggedIn = false;
  await page.route("**/remote/v1/session", async (route) => {
    await route.fulfill({ status: loggedIn ? 200 : 401, contentType: "application/json",
      body: loggedIn ? JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }) :
        '{"error":"unauthorized"}' });
  });
  await page.route("**/remote/v1/challenge", async (route) => {
    await route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ schema: 1, csrf: "e".repeat(64) }) });
  });
  await page.route("**/remote/v1/login", async (route) => {
    const body = route.request().postDataJSON() as {actor_id:string;password:string;otp:string};
    expect(body.actor_id).toBe("7");
    expect(body.password).toBe("example-proof");
    expect(body.otp).toBe("123456");
    expect(route.request().headers()["x-dc24h-remote-csrf"]).toBe("e".repeat(64));
    loggedIn = true;
    await route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 600 }) });
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
  await page.getByLabel("Kod uwierzytelniający (6 cyfr)").fill("123456");
  await page.getByRole("button", { name: "Zaloguj", exact: true }).click();
  await expect(page.getByText("beta-0.6.16")).toBeVisible();
  await expect(page.getByText(/Pozostały czas sesji \(orientacyjny\):/)).toBeVisible();
  await expect(page.getByText("Aktywne połączenia")).toBeVisible();
  await expect(page.getByText("Użytkownicy online")).toBeVisible();
  await expect(page.getByRole("region", { name: "Prywatny przegląd incydentów" })).toBeVisible();
  await expect(page.getByText("Niedostępne w zatwierdzonym widoku tylko do odczytu")).toBeVisible();
  const acknowledgedRequests: string[] = [];
  page.on("request", request => {
    if (request.method() !== "GET") acknowledgedRequests.push(request.url());
  });
  await page.getByRole("button", { name: "Potwierdź lokalnie" }).focus();
  await page.keyboard.press("Enter");
  await expect(page.getByRole("status").filter({ hasText: "Potwierdzono tylko na tym ekranie" })).toBeVisible();
  await expect(page.getByRole("button", { name: "Potwierdź lokalnie" })).toBeDisabled();
  await expect(page.getByRole("button", { name: "Potwierdź lokalnie" })).toHaveAttribute("aria-pressed", "true");
  await page.getByRole("button", { name: "Odśwież" }).click();
  await expect(page.getByRole("button", { name: "Potwierdź lokalnie" })).toBeEnabled();
  await expect(page.getByRole("button", { name: "Potwierdź lokalnie" })).toHaveAttribute("aria-pressed", "false");
  await page.getByRole("button", { name: "English" }).click();
  await expect(page.getByText("Incident history is unavailable or not yet verified; this does not mean the service is healthy.")).toBeVisible();
  await page.getByRole("button", { name: "Polski" }).click();
  expect(acknowledgedRequests).toEqual([]);
  for (const title of ["Start", "Stop", "Restart", "Edit", "Restore", "Grant"]) {
    await expect(page.getByRole("button", { name: title, exact: true })).toHaveCount(0);
  }
  await page.getByRole("button", { name: "Wyloguj" }).click();
  await expect(page.getByText("Zaloguj się, aby odczytać zatwierdzone dane huba.")).toBeVisible();
  await expect(page.getByRole("region", { name: "Prywatny przegląd incydentów" })).toHaveCount(0);
});

test("local passkey inventory exposes only this account's device actions and revokes the browser session", async ({ page }) => {
  let revoked = false;
  let name = "My phone";
  let holdSummary = false;
  let staleSummary!: Route;
  let signalStale!: () => void;
  const staleStarted = new Promise<void>(resolve => { signalStale = resolve; });
  await page.route("**/remote/v1/session", route => route.fulfill({
    status: revoked ? 401 : 200, contentType: "application/json",
    body: revoked ? '{"error":"unauthorized"}' : JSON.stringify({
      schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 600,
    }),
  }));
  await page.route("**/remote/v1/summary", route => {
    if (holdSummary) { holdSummary = false; staleSummary = route; signalStale(); return; }
    return route.fulfill({
      status: 200, contentType: "application/json", body: JSON.stringify(snapshot),
    });
  });
  await page.route("**/remote/v1/passkey/devices", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify({ schema: 1, devices: [{
      id: "p".repeat(32), name, device_type: "multiDevice", backed_up: true,
      created_at: 1000, last_used_at: null,
    }] }),
  }));
  await page.route("**/remote/v1/passkey/device/rename", async route => {
    expect(route.request().headers()["x-dc24h-remote-csrf"]).toBe(sessionCsrf);
    const body = route.request().postDataJSON() as { id: string; name: string };
    expect(body.id).toBe("p".repeat(32));
    name = body.name;
    await route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ schema: 1, renamed: true, csrf: "a".repeat(64) }) });
  });
  await page.route("**/remote/v1/passkey/device/revoke", async route => {
    // Device rename rotates the administrator session and its CSRF token.
    expect(route.request().headers()["x-dc24h-remote-csrf"]).toBe("a".repeat(64));
    revoked = true;
    await route.fulfill({ status: 200, contentType: "application/json", body: '{"schema":1,"revoked":true}' });
  });
  await page.goto("/remote.html");
  await expect(page.getByText("Passkeys / trusted devices")).toBeVisible();
  await expect(page.getByText("Active connections")).toBeVisible();
  await page.getByRole("button", { name: "Refresh devices" }).click();
  await expect(page.getByText("My phone")).toBeVisible();
  holdSummary = true;
  await page.getByRole("button", { name: "Refresh", exact: true }).click();
  await staleStarted;
  await page.getByLabel("Device name").fill("Work laptop");
  await page.getByRole("button", { name: "Rename" }).click();
  await expect(page.getByText("Work laptop")).toBeVisible();
  // The delayed read still belongs to the old session cookie, revoked by
  // the rename. Its 403 must not invalidate the successful rotated session.
  const oldResponse = page.waitForResponse(response =>
    response.url().endsWith("/remote/v1/summary") && response.status() === 403);
  await staleSummary.fulfill({ status: 403, contentType: "application/json", body: '{"error":"forbidden"}' });
  await oldResponse;
  await page.evaluate(() => new Promise<void>(resolve =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
  await expect(page.getByRole("button", { name: "Sign out" })).toBeVisible();
  await expect(page.getByText("Active connections")).toBeVisible();
  await page.getByRole("button", { name: "Revoke" }).click();
  await expect(page.getByText(/Revoke this passkey\?/)).toBeVisible();
  expect(revoked).toBe(false);
  await page.getByRole("button", { name: "Cancel" }).click();
  await expect(page.getByText(/Revoke this passkey\?/)).toHaveCount(0);
  await page.getByRole("button", { name: "Revoke" }).click();
  await page.getByRole("button", { name: "Confirm revocation" }).click();
  await expect(page.getByText("Sign in to read approved hub metrics.")).toBeVisible();
  await expect(page.getByText("The key was revoked and sessions were ended.")).toBeVisible();
});

test("device inventory outage removes stale device actions without losing read-only session", async ({ page }) => {
  let outage = false;
  await page.route("**/remote/v1/session", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify({ schema: 1,
      authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 90 }),
  }));
  await page.route("**/remote/v1/summary", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify(snapshot),
  }));
  await page.route("**/remote/v1/passkey/devices", route => route.fulfill({
    status: outage ? 503 : 200, contentType: "application/json",
    body: outage ? '{"error":"unavailable"}' : JSON.stringify({schema: 1, devices: [{
      id: "p".repeat(32), name: "Current key", device_type: "singleDevice", backed_up: false,
      created_at: 1000, last_used_at: null,
    }]}),
  }));
  await page.goto("/remote.html");
  await expect(page.getByText(/Session time remaining \(approximate\):/)).toBeVisible();
  await page.getByRole("button", { name: "Refresh devices" }).click();
  await expect(page.getByText("Current key")).toBeVisible();
  await expect(page.getByText(/Last used: Not used yet/)).toBeVisible();
  outage = true;
  await page.getByRole("button", { name: "Refresh devices" }).click();
  await expect(page.getByText("Current key")).toHaveCount(0);
  await expect(page.getByText(/Device list is unavailable/)).toBeVisible();
  await expect(page.getByRole("button", { name: "Revoke" })).toHaveCount(0);
  await expect(page.getByRole("button", { name: "Sign out" })).toBeVisible();
});

test("session countdown starts at the server lifetime even after delayed inspection", async ({ page }) => {
  await page.route("**/remote/v1/session", async route => {
    // The old wall-clock deadline was calculated later than the initial clock
    // state. ceil() then displayed 11 minutes for a 10-minute session.
    await new Promise(resolve => setTimeout(resolve, 100));
    await route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ schema: 1, authenticated: true, mfa: true,
        mode: "read-only", csrf: sessionCsrf, expires_in: 600 }) });
  });
  await page.route("**/remote/v1/summary", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify(snapshot),
  }));
  await page.goto("/remote.html");
  await expect(page.getByText("Session time remaining (approximate): 10 min")).toBeVisible();
});

test("refused passkey enrollment leaves no device, secret or pending privileged action in the browser", async ({ page }) => {
  let finishCalls = 0;
  await page.route("**/remote/v1/session", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify({
      schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 600,
    }),
  }));
  await page.route("**/remote/v1/summary", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify(snapshot),
  }));
  await page.route("**/remote/v1/passkey/devices", route => route.fulfill({
    status: 200, contentType: "application/json", body: '{"schema":1,"devices":[]}',
  }));
  await page.route("**/remote/v1/passkey/enroll/options", async route => {
    expect(route.request().headers()["x-dc24h-remote-csrf"]).toBe(sessionCsrf);
    expect(route.request().postDataJSON()).toEqual({});
    await route.fulfill({ status: 403, contentType: "application/json", body: '{"error":"enrollment_refused"}' });
  });
  await page.route("**/remote/v1/passkey/enroll/finish", async route => {
    finishCalls++;
    await route.fulfill({ status: 403, contentType: "application/json", body: '{"error":"forbidden"}' });
  });
  await page.goto("/remote.html");
  await expect(page.getByText("Active connections")).toBeVisible();
  await page.getByRole("button", { name: "Refresh devices" }).click();
  await expect(page.getByText("No passkeys enrolled yet.")).toBeVisible();
  await page.getByRole("button", { name: "Add passkey" }).click();
  await expect(page.getByText("Passkey operation was refused. Sign in again if recent verification is required.")).toBeVisible();
  await expect(page.getByText("No passkeys enrolled yet.")).toBeVisible();
  await expect(page.getByText("Active connections")).toBeVisible();
  await expect(page.getByRole("button", { name: "Revoke" })).toHaveCount(0);
  expect(finishCalls).toBe(0);
  await expect(page.locator("main")).not.toContainText("session=" + sessionCsrf);
});

test("a delayed device revocation response cannot sign out a newer session", async ({ page }) => {
  let oldRevoke!: Route;
  let signalStarted!: () => void;
  const revokeStarted = new Promise<void>(resolve => { signalStarted = resolve; });
  await page.route("**/remote/v1/session", route => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true,
      mode: "read-only", csrf: sessionCsrf, expires_in: 600 }),
  }));
  await page.route("**/remote/v1/summary", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify(snapshot),
  }));
  await page.route("**/remote/v1/passkey/devices", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify({ schema: 1, devices: [{
      id: "p".repeat(32), name: "Old device", device_type: "multiDevice", backed_up: true,
      created_at: 1000, last_used_at: null,
    }] }),
  }));
  await page.route("**/remote/v1/passkey/device/revoke", route => {
    oldRevoke = route;
    signalStarted();
    // The server has processed the old revoke, but its HTTP reply is delayed.
  });
  await page.route("**/remote/v1/logout", route => route.fulfill({
    status: 200, contentType: "application/json", body: '{"schema":1,"authenticated":false}',
  }));
  await page.route("**/remote/v1/challenge", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify({ schema: 1, csrf: "e".repeat(64) }),
  }));
  await page.route("**/remote/v1/login", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify({ schema: 1,
      authenticated: true, mfa: true, mode: "read-only", csrf: "f".repeat(64), expires_in: 600 }),
  }));

  await page.goto("/remote.html");
  await page.getByRole("button", { name: "Refresh devices" }).click();
  await expect(page.getByText("Old device")).toBeVisible();
  await page.getByRole("button", { name: "Revoke" }).click();
  // Stage III requires an explicit second confirmation before the mutation
  // request starts. Its delayed response must still not revoke a new login.
  await page.getByRole("button", { name: "Confirm revocation" }).click();
  await revokeStarted;
  await page.getByRole("button", { name: "Sign out" }).click();
  await expect(page.getByText("Sign in to read approved hub metrics.")).toBeVisible();
  await page.getByLabel("Administrator account ID").fill("8");
  await page.getByLabel("Account password").fill("new-proof");
  await page.getByLabel("Authenticator code (6 digits)").fill("123456");
  await page.getByRole("button", { name: "Sign in", exact: true }).click();
  await expect(page.getByText("Active connections")).toBeVisible();

  const oldResponse = page.waitForResponse(response =>
    response.url().endsWith("/remote/v1/passkey/device/revoke") && response.status() === 200);
  await oldRevoke.fulfill({ status: 200, contentType: "application/json", body: '{"schema":1,"revoked":true}' });
  await oldResponse;
  await page.evaluate(() => new Promise<void>(resolve =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
  await expect(page.getByRole("button", { name: "Sign out" })).toBeVisible();
  await expect(page.getByText("Active connections")).toBeVisible();
  await expect(page.getByText("The key was revoked and sessions were ended.")).toHaveCount(0);
});

test("an older device inventory cannot replace a newer refresh", async ({ page }) => {
  let delayed!: Route;
  let signalStarted!: () => void;
  const started = new Promise<void>(resolve => { signalStarted = resolve; });
  let reads = 0;
  await page.route("**/remote/v1/session", route => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true,
      mode: "read-only", csrf: sessionCsrf, expires_in: 600 }),
  }));
  await page.route("**/remote/v1/summary", route => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify(snapshot),
  }));
  const inventory = (name: string) => JSON.stringify({ schema: 1, devices: [{
    id: "p".repeat(32), name, device_type: "multiDevice", backed_up: true,
    created_at: 1000, last_used_at: null,
  }] });
  await page.route("**/remote/v1/passkey/devices", async route => {
    if (++reads === 1) { delayed = route; signalStarted(); return; }
    await route.fulfill({ status: 200, contentType: "application/json", body: inventory("New name") });
  });
  await page.goto("/remote.html");
  await page.getByRole("button", { name: "Refresh devices" }).click();
  await started;
  await page.getByRole("button", { name: "Refresh devices" }).click();
  await expect(page.getByText("New name")).toBeVisible();
  const oldResponse = page.waitForResponse(response =>
    response.url().endsWith("/remote/v1/passkey/devices") && response.status() === 200 &&
    response.request().method() === "GET");
  await delayed.fulfill({ status: 200, contentType: "application/json", body: inventory("Stale name") });
  await oldResponse;
  await page.evaluate(() => new Promise<void>(resolve =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
  await expect(page.getByText("New name")).toBeVisible();
  await expect(page.getByText("Stale name")).toHaveCount(0);
});

test("remote view fails closed on unavailable API and does not render stale metrics", async ({ page }) => {
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }),
  }));
  await page.route("**/remote/v1/summary", async (route) => route.fulfill({
    status: 503, contentType: "application/json", body: '{"error":"unavailable"}',
  }));
  await page.goto("/remote.html");
  await expect(page.getByText(/The gateway or hub API is unavailable/)).toBeVisible();
  await expect(page.getByText("Active connections")).toHaveCount(0);
});

test("stale incident counters expire within 46 seconds when summary refresh hangs", async ({ page }) => {
  await page.clock.install({ time: new Date("2026-11-24T12:00:00Z") });
  await page.route("**/remote/v1/session", route => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only",
      csrf: sessionCsrf, expires_in: 600 }),
  }));
  let reads = 0;
  await page.route("**/remote/v1/summary", route => {
    if (++reads > 1) return; // Simulate an in-flight read that never completes.
    return route.fulfill({ status: 200, contentType: "application/json", body: JSON.stringify(snapshot) });
  });
  await page.goto("/remote.html");
  await expect(page.getByRole("region", { name: "Private incident overview" })).toBeVisible();
  await expect(page.getByText("Active connections")).toBeVisible();
  await page.clock.runFor(46_000);
  await expect(page.getByRole("region", { name: "Private incident overview" })).toHaveCount(0);
  await expect(page.getByText("Active connections")).toHaveCount(0);
  await expect(page.getByText("Read expired; old counters are hidden until a verified refresh.")).toBeVisible();
  expect(reads).toBeGreaterThan(1);
});

test("source-age budget expires early even with a fresh browser receipt", async ({ page }) => {
  await page.clock.install({ time: new Date("2026-11-24T12:00:00Z") });
  await page.route("**/remote/v1/session", route => route.fulfill({ status: 200,
    contentType: "application/json", body: JSON.stringify({ schema: 1, authenticated: true,
      mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 600 }) }));
  let reads = 0;
  await page.route("**/remote/v1/summary", route => {
    if (++reads > 1) return;
    return route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ ...snapshot, observation: { schema: 1, source_age_ms: 10_000 },
        incidents: { schema: 1, state: "unknown", events: [], alerts: [] } }) });
  });
  await page.goto("/remote.html");
  await expect(page.getByText("Incident history is unavailable or not yet verified; this does not mean the service is healthy.")).toBeVisible();
  await expect(page.getByText("Active connections")).toBeVisible();
  await page.clock.runFor(36_000);
  await expect(page.getByText("Active connections")).toHaveCount(0);
  await expect(page.getByText("Read expired; old counters are hidden until a verified refresh.")).toBeVisible();
  expect(reads).toBeGreaterThan(1);
});

test("remote view clears previously displayed metrics after session expiration", async ({ page }) => {
  let expired = false;
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }),
  }));
  await page.route("**/remote/v1/summary", async (route) => route.fulfill({
    status: expired ? 401 : 200, contentType: "application/json",
    body: expired ? '{"error":"unauthorized"}' : JSON.stringify(snapshot),
  }));
  await page.goto("/remote.html");
  await expect(page.getByText("Active connections")).toBeVisible();
  expired = true;
  await page.getByRole("button", { name: "Refresh", exact: true }).click();
  await expect(page.getByText("Your session expired or was revoked. Sign in again.")).toBeVisible();
  await expect(page.getByText("Active connections")).toHaveCount(0);
  await expect(page.getByRole("button", { name: "Sign out" })).toHaveCount(0);
});

test("a delayed denial from the previous session cannot expire a new login", async ({ page }) => {
  let firstRequest!: Route;
  let signalStarted!: () => void;
  const firstStarted = new Promise<void>((resolve) => { signalStarted = resolve; });
  let summaries = 0;
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }),
  }));
  await page.route("**/remote/v1/challenge", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, csrf: "e".repeat(64) }),
  }));
  await page.route("**/remote/v1/login", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: "f".repeat(64), expires_in: 600 }),
  }));
  await page.route("**/remote/v1/summary", async (route) => {
    summaries++;
    if (summaries === 1) {
      firstRequest = route;
      signalStarted();
      return; // Leave the old session request in flight.
    }
    await route.fulfill(summaries === 2
      ? { status: 401, contentType: "application/json", body: '{"error":"unauthorized"}' }
      : { status: 200, contentType: "application/json", body: JSON.stringify(snapshot) });
  });

  await page.goto("/remote.html");
  await firstStarted;
  await page.getByRole("button", { name: "Refresh", exact: true }).click();
  await expect(page.getByText("Your session expired or was revoked. Sign in again.")).toBeVisible();
  await page.getByLabel("Administrator account ID").fill("7");
  await page.getByLabel("Account password").fill("correct");
  await page.getByLabel("Authenticator code (6 digits)").fill("123456");
  await page.getByRole("button", { name: "Sign in", exact: true }).click();
  await expect(page.getByText("Active connections")).toBeVisible();

  const oldResponse = page.waitForResponse((response) =>
    response.url().includes("/remote/v1/summary") && response.status() === 401);
  await firstRequest.fulfill({ status: 401, contentType: "application/json", body: '{"error":"unauthorized"}' });
  await oldResponse;
  await page.evaluate(() => new Promise<void>((resolve) =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
  await expect(page.getByRole("button", { name: "Sign out" })).toBeVisible();
  await expect(page.getByText("Active connections")).toBeVisible();
});

test("a failed login challenge clears the entered password", async ({ page }) => {
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 401, contentType: "application/json", body: '{"error":"unauthorized"}',
  }));
  await page.route("**/remote/v1/challenge", async (route) => route.fulfill({
    status: 503, contentType: "application/json", body: '{"error":"unavailable"}',
  }));
  await page.goto("/remote.html");
  await page.getByLabel("Administrator account ID").fill("7");
  await page.getByLabel("Account password").fill("sensitive-proof");
  await page.getByLabel("Authenticator code (6 digits)").fill("123456");
  await page.getByRole("button", { name: "Sign in", exact: true }).click();
  await expect(page.getByText(/The gateway or hub API is unavailable/)).toBeVisible();
  await expect(page.getByLabel("Account password")).toHaveValue("");
  await expect(page.getByLabel("Authenticator code (6 digits)")).toHaveValue("");
});

test("an old summary cannot overwrite metrics after logout and a new login", async ({ page }) => {
  let oldRequest!: Route;
  let signalStarted!: () => void;
  const firstStarted = new Promise<void>((resolve) => { signalStarted = resolve; });
  let reads = 0;
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }),
  }));
  await page.route("**/remote/v1/logout", async (route) => route.fulfill({
    status: 200, contentType: "application/json", body: '{"schema":1,"authenticated":false}',
  }));
  await page.route("**/remote/v1/challenge", async (route) => route.fulfill({
    status: 200, contentType: "application/json", body: JSON.stringify({ schema: 1, csrf: "e".repeat(64) }),
  }));
  await page.route("**/remote/v1/login", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: "f".repeat(64), expires_in: 600 }),
  }));
  await page.route("**/remote/v1/summary", async (route) => {
    reads++;
    if (reads === 1) {
      oldRequest = route;
      signalStarted();
      return;
    }
    await route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ ...snapshot, network: { active_connections: 99, accepted_connections: 100 } }) });
  });
  await page.goto("/remote.html");
  await firstStarted;
  await page.getByRole("button", { name: "Sign out" }).click();
  await expect(page.getByText("Sign in to read approved hub metrics.")).toBeVisible();
  await page.getByLabel("Administrator account ID").fill("7");
  await page.getByLabel("Account password").fill("new-proof");
  await page.getByLabel("Authenticator code (6 digits)").fill("123456");
  await page.getByRole("button", { name: "Sign in", exact: true }).click();
  await expect(page.getByText("99", { exact: true })).toBeVisible();

  const staleResponse = page.waitForResponse((response) =>
    response.url().includes("/remote/v1/summary") && response.status() === 200);
  await oldRequest.fulfill({ status: 200, contentType: "application/json", body: JSON.stringify(snapshot) });
  await staleResponse;
  await page.evaluate(() => new Promise<void>((resolve) =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
  await expect(page.getByText("99", { exact: true })).toBeVisible();
  await expect(page.getByText("5", { exact: true })).toHaveCount(0);
});

test("an older read in the same session cannot replace fresher metrics", async ({ page }) => {
  let oldRequest!: Route;
  let signalStarted!: () => void;
  const firstStarted = new Promise<void>((resolve) => { signalStarted = resolve; });
  let reads = 0;
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }),
  }));
  await page.route("**/remote/v1/summary", async (route) => {
    reads++;
    if (reads === 1) { oldRequest = route; signalStarted(); return; }
    await route.fulfill({ status: 200, contentType: "application/json",
      body: JSON.stringify({ ...snapshot, network: { active_connections: 99, accepted_connections: 100 } }) });
  });

  await page.goto("/remote.html");
  await firstStarted;
  await page.getByRole("button", { name: "Refresh", exact: true }).click();
  await expect(page.getByText("99", { exact: true })).toBeVisible();

  const staleResponse = page.waitForResponse((response) =>
    response.url().includes("/remote/v1/summary") && response.status() === 200);
  await oldRequest.fulfill({ status: 200, contentType: "application/json", body: JSON.stringify(snapshot) });
  await staleResponse;
  await page.evaluate(() => new Promise<void>((resolve) =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
  await expect(page.getByText("99", { exact: true })).toBeVisible();
  await expect(page.getByText("5", { exact: true })).toHaveCount(0);
});

test("malformed HTTP 200 metrics fail closed without rendering unsafe nested data", async ({ page }) => {
  await page.route("**/remote/v1/session", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ schema: 1, authenticated: true, mfa: true, mode: "read-only", csrf: sessionCsrf, expires_in: 120 }),
  }));
  await page.route("**/remote/v1/summary", async (route) => route.fulfill({
    status: 200, contentType: "application/json",
    body: JSON.stringify({ ...snapshot, server: { ...snapshot.server, version: { nested: "private" } } }),
  }));
  await page.goto("/remote.html");
  await expect(page.getByText(/The gateway or hub API is unavailable/)).toBeVisible();
  await expect(page.getByText("Active connections")).toHaveCount(0);
});
