import { searchSnapshot } from "./fixtures";
import { expect, test } from "@playwright/test";
import { WEBADMIN_VERSION } from "../src/lib/status";
import { readFile } from "node:fs/promises";
const data = {
  api_version: "v1",
  server: {
    name: "dc24h.eu",
    version: "alpha-0.6.6",
    core_running: true,
    uptime_seconds: 3661,
  },
  network: {
    active_connections: 7,
    accepted_connections: 48,
    adc_frames: 144,
    adcs_handshakes: 3,
    security_rejected: 2,
    security_closed: 1,
    rejected_connections: 5,
    closed_connections: 41,
    received_bytes: 4096,
    sent_bytes: 8192,
    peak_connections: 9,
  },
  users: { online: 4, peak_online: 6, successful_logins: 23 },
  chat: { received: 12, public_dispatches: 5, private_dispatches: 3, rejected: 2, flood_limited: 1, errors: 1, queued: 21, unavailable: 0, refused: 0 },
  database: { enabled: false },
};
test("unconfigured panel stays honest, readable and bilingual with working navigation", async ({
  page,
}) => {
  await page.goto("/");
  await expect(page.locator("a.documentation")).toHaveAttribute(
    "href",
    `https://github.com/adrianf87/dc24h.eu/blob/${WEBADMIN_VERSION}/doc/us/WEBADMIN.md`,
  );
  await expect(
    page.getByRole("heading", { name: "Your hub, in one place." }),
  ).toBeVisible();
  await expect(
    page.getByText("No observations yet", { exact: true }),
  ).toBeVisible();
  await expect(
    page.getByRole("button", { name: "Export snapshot" }),
  ).toBeDisabled();
  await expect(
    page.getByRole("button", { name: /Configuration/ }),
  ).toBeDisabled();
  await page.getByRole("switch").click();
  await expect(page.getByRole("switch")).not.toBeChecked();
  await page.getByRole("combobox").selectOption("pl");
  await expect(page.locator("a.documentation")).toHaveAttribute(
    "href",
    `https://github.com/adrianf87/dc24h.eu/blob/${WEBADMIN_VERSION}/doc/pl/WEBADMIN.md`,
  );
  await expect(
    page.getByRole("heading", { name: "Przegląd huba" }),
  ).toBeVisible();
  await page.reload();
  await expect(page.locator("html")).toHaveAttribute("lang", "pl");
  await page.getByRole("link", { name: "Baza danych", exact: true }).click();
  await expect(
    page.getByRole("heading", { name: "Stan bazy danych" }),
  ).toBeVisible();
  await page.getByRole("combobox").selectOption("us");
  await page.getByRole("link", { name: "Security", exact: true }).click();
  await expect(
    page.getByRole("heading", { name: "Security overview" }),
  ).toBeVisible();
  expect(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  ).toBe(true);
});
test("renders real API-shaped values, exports a safe snapshot and clears unavailable data", async ({
  page,
}) => {
  let online = true;
  await page.route("**/admin-api/v1/status", (route) =>
    route.fulfill(
      online
        ? { json: data }
        : { status: 502, json: { error: "api_unavailable" } },
    ),
  );
  await page.goto("/");
  await expect(page.getByText("API connected", { exact: true })).toBeVisible();
  await expect(page.locator(".metric-value").first()).toHaveText("7");
  await expect(page.locator(".metric-value").nth(1)).toHaveText("144");
  await expect(page.getByText("1h 1m", { exact: true })).toBeVisible();
  await expect(page.getByTestId("user-statistics").locator(".metric-value")).toHaveText(["4", "6", "23", "41"]);
  await expect(page.getByTestId("chat-statistics").locator(".metric-value")).toHaveText(["5", "3", "2", "1"]);
  await page.getByRole("combobox").selectOption("pl");
  await expect(page.getByText("Użytkownicy online", { exact: true })).toBeVisible();
  await expect(page.getByTestId("chat-statistics").getByText("Czat prywatny", { exact: true })).toBeVisible();
  await page.getByRole("combobox").selectOption("us");
  await page.getByRole("switch").click();
  const download = page.waitForEvent("download");
  await page.getByRole("button", { name: "Export snapshot" }).click();
  expect((await download).suggestedFilename()).toBe("dc24h-snapshot.json");
  await page.getByRole("link", { name: "Database", exact: true }).click();
  await expect(page.getByText("Disabled", { exact: true })).toBeVisible();
  await page.getByRole("link", { name: "Connections", exact: true }).click();
  await expect(page.locator(".metric-value").nth(1)).toHaveText("48");
  await expect(page.getByTestId("connection-statistics").locator(".metric-value")).toHaveText(["9", "5", "4,096", "8,192"]);
  online = false;
  await page.getByRole("button", { name: "Refresh now" }).click();
  await expect(
    page.getByText("Connection lost", { exact: true }),
  ).toBeVisible();
  await expect(page.locator(".metric-value").first()).toHaveText("—");
  await expect(page.getByTestId("connection-statistics").locator(".metric-value")).toHaveText(["—", "—", "—", "—"]);
  await expect(
    page.getByRole("button", { name: "Export snapshot" }),
  ).toBeDisabled();
});
test("rejects malformed snapshots and supports mobile navigation without overflow", async ({
  page,
}) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.route("**/admin-api/v1/status", (route) =>
    route.fulfill({
      json: { ...data, network: { ...data.network, active_connections: -1 } },
    }),
  );
  await page.goto("/");
  await expect(
    page.getByRole("heading", { name: "We couldn’t reach your hub." }),
  ).toBeVisible();
  await page.getByRole("button", { name: "Open navigation" }).click();
  await page.getByRole("link", { name: "Database", exact: true }).click();
  await expect(
    page.getByRole("heading", { name: "Database status" }),
  ).toBeVisible();
  await expect(
    page.getByRole("button", { name: "Open navigation" }),
  ).toHaveAttribute("aria-expanded", "false");
  expect(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  ).toBe(true);
});


test("chat page shows every counter, exports allowlisted data and supports mobile Polish navigation", async ({ page }, testInfo) => {
  const chat = { received: 12345, public_dispatches: 5000, private_dispatches: 6000,
    rejected: 1000, flood_limited: 300, errors: 45, queued: 9007199254740991,
    unavailable: 2, refused: 7 };
  await page.route("**/admin-api/v1/status", route => route.fulfill({ json: {
    ...data, chat: { ...chat, text: "WebChatSecret", recipient: "WebChatSecret" },
  } }));
  await page.goto("/");
  await page.getByRole("link", { name: "View all chat statistics", exact: true }).click();
  await expect(page).toHaveURL(/#chat$/);
  await expect(page.getByRole("link", { name: "Chat", exact: true })).toHaveAttribute("aria-current", "page");
  await expect(page.getByRole("heading", { name: "Chat statistics", exact: true })).toBeVisible();
  await expect(page.getByTestId("chat-message-statistics").locator(".metric-value"))
    .toHaveText(["12,345", "5,000", "6,000", "1,000", "300", "45"]);
  await expect(page.getByTestId("chat-queue-statistics").locator(".metric-value"))
    .toHaveText(["9,007,199,254,740,991", "2", "7"]);
  await expect(page.getByText(/A queued copy does not confirm delivery/)).toBeVisible();
  await expect(page.getByText("WebChatSecret")).toHaveCount(0);
  const download = page.waitForEvent("download");
  await page.getByRole("button", { name: "Export snapshot", exact: true }).click();
  const saved = await download;
  const exported = JSON.parse(await readFile((await saved.path())!, "utf8"));
  expect(exported.chat).toEqual(chat);
  expect(JSON.stringify(exported)).not.toContain("WebChatSecret");
  await page.screenshot({ path: testInfo.outputPath("chat-desktop.png"), fullPage: true });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByRole("combobox").selectOption("pl");
  await expect(page.getByRole("heading", { name: "Statystyki czatu", exact: true })).toBeVisible();
  await expect(page.getByTestId("chat-message-statistics").locator(".metric-value").first())
    .toHaveText(new Intl.NumberFormat("pl-PL").format(chat.received));
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.reload();
  await expect(page.getByRole("heading", { name: "Statystyki czatu", exact: true })).toBeVisible();
  await page.getByRole("button", { name: "Otwórz nawigację", exact: true }).click();
  await page.getByRole("link", { name: "Przegląd", exact: true }).click();
  await page.getByRole("button", { name: "Otwórz nawigację", exact: true }).click();
  await page.getByRole("link", { name: "Czat", exact: true }).click();
  await expect(page.getByRole("button", { name: "Otwórz nawigację", exact: true })).toHaveAttribute("aria-expanded", "false");
  await expect(page.locator(".sidebar")).toBeHidden();
  await page.screenshot({ path: testInfo.outputPath("chat-mobile-pl.png"), fullPage: true });
});

test("chat counters refresh, reset on server restart and clear on unavailable or malformed API", async ({ page }) => {
  let current: unknown = data;
  let status = 200;
  await page.route("**/admin-api/v1/status", route => route.fulfill({ status, json: current }));
  await page.goto("/#chat");
  const values = page.getByTestId("chat-details").locator(".metric-value");
  await expect(values).toHaveText(["12", "5", "3", "2", "1", "1", "21", "0", "0"]);
  await page.getByRole("switch").click();
  await expect(page.getByRole("switch")).not.toBeChecked();
  const zero = Object.fromEntries(Object.keys(data.chat).map(key => [key, 0]));
  current = { ...data, server: { ...data.server, uptime_seconds: 1 }, chat: zero };
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(values).toHaveText(Array(9).fill("0"));
  for (const [nextStatus, nextValue] of [
    [502, { error: "api_unavailable" }],
    [200, { ...data, chat: { ...data.chat, received: -1 } }],
    [200, { ...data, chat: undefined }],
  ] as const) {
    status = nextStatus; current = nextValue;
    await page.getByRole("button", { name: "Refresh now", exact: true }).click();
    await expect(page.getByText("Connection lost", { exact: true })).toBeVisible();
    await expect(values).toHaveText(Array(9).fill("—"));
    await expect(page.getByRole("button", { name: "Export snapshot", exact: true })).toBeDisabled();
  }
  current = data; status = 200;
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(values).toHaveText(["12", "5", "3", "2", "1", "1", "21", "0", "0"]);
  await expect(page.getByRole("button", { name: "Export snapshot", exact: true })).toBeEnabled();
});

test("unconfigured chat page displays unavailable counters and retains navigation", async ({ page }) => {
  await page.route("**/admin-api/v1/status", route => route.fulfill({ status: 503, json: { error: "api_unconfigured" } }));
  await page.goto("/#chat");
  await expect(page.getByText("Not connected", { exact: true })).toBeVisible();
  await expect(page.getByTestId("chat-details").locator(".metric-value")).toHaveText(Array(9).fill("—"));
  await expect(page.getByRole("button", { name: "Export snapshot", exact: true })).toBeDisabled();
  await page.getByRole("link", { name: "Overview", exact: true }).click();
  await expect(page.getByRole("heading", { name: "Hub overview", exact: true })).toBeVisible();
});

test("search statistics and effective configuration are bilingual and safe to export", async ({ page }, testInfo) => {
  await page.route("**/admin-api/v1/status", route => route.fulfill({ json: { ...data,
    search: { ...searchSnapshot, token: "SearchPrivate", configuration: { ...searchSnapshot.configuration, password: "SearchPrivate" } } } }));
  await page.goto("/#search");
  await expect(page.getByRole("heading", { name: "Search monitoring", exact: true })).toBeVisible();
  await expect(page.getByRole("link", { name: "Search", exact: true })).toHaveAttribute("aria-current", "page");
  await expect(page.getByTestId("search-query-statistics").locator(".metric-value")).toHaveText(Array.from({ length: 10 }, (_, i) => String(i + 1)));
  await expect(page.getByTestId("search-result-statistics").locator(".metric-value")).toHaveText(Array.from({ length: 9 }, (_, i) => String(i + 11)));
  await expect(page.locator('[data-setting="max_active_per_user"]')).toHaveText("8");
  await expect(page.getByText(/Direct client UDP is outside/)).toBeVisible();
  await expect(page.getByTestId("search-configuration").locator("input,button,select")).toHaveCount(0);
  const download = page.waitForEvent("download");
  await page.getByRole("button", { name: "Export snapshot", exact: true }).click();
  const exported = JSON.parse(await readFile((await (await download).path())!, "utf8"));
  expect(exported.search).toEqual(searchSnapshot); expect(JSON.stringify(exported)).not.toContain("SearchPrivate");
  await page.screenshot({ path: testInfo.outputPath("search-desktop.png"), fullPage: true });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByRole("combobox").selectOption("pl");
  await expect(page.getByRole("heading", { name: "Monitoring wyszukiwań", exact: true })).toBeVisible();
  await expect(page.getByRole("heading", { name: "Efektywna konfiguracja", exact: true })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.screenshot({ path: testInfo.outputPath("search-mobile-pl.png"), fullPage: true });
});
test("search refresh handles restart, old APIs and invalid data honestly", async ({ page }) => {
  let current: unknown = { ...data, search: searchSnapshot };
  await page.route("**/admin-api/v1/status", route => route.fulfill({ json: current }));
  await page.goto("/#search");
  const values = page.getByTestId("search-details").locator(".metric-value");
  await expect(values).toHaveCount(20); await expect(values.first()).toHaveText("1");
  await page.getByRole("switch").click();
  const zero = Object.fromEntries(Object.keys(searchSnapshot.statistics).map(key => [key, 0]));
  current = { ...data, server: { ...data.server, uptime_seconds: 1 }, search: { statistics: zero, configuration: { ...searchSnapshot.configuration, query_rate: 3 } } };
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(values).toHaveText(Array(20).fill("0")); await expect(page.locator('[data-setting="query_rate"]')).toHaveText("3");
  current = data;
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(values).toHaveText(Array(20).fill("—")); await expect(page.getByText(/Search information is unavailable/)).toBeVisible();
  current = { ...data, search: { ...searchSnapshot, statistics: { ...searchSnapshot.statistics, queries_received: -1 } } };
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(page.getByText("Connection lost", { exact: true })).toBeVisible();
  await expect(values).toHaveText(Array(20).fill("—")); await expect(page.locator('[data-setting="query_rate"]')).toHaveText("—");
});

test("search monitoring samples real poll deltas, pauses, clears locally and keeps export cumulative", async ({ page }, testInfo) => {
  await page.clock.install({ time: new Date("2026-11-13T12:00:00Z") });
  await page.clock.pauseAt(new Date("2026-11-13T12:00:01Z"));
  let current = { ...data, search: structuredClone(searchSnapshot) };
  let requests = 0;
  await page.route("**/admin-api/v1/status", route => { requests++; return route.fulfill({ json: current }); });
  await page.goto("/#search");
  await expect(page.getByText("API connected", { exact: true })).toBeVisible();
  const rates = page.getByTestId("search-rates").locator(".metric-value");
  await expect(rates).toHaveText(["—", "—", "—", "—"]);
  await expect(page.getByText("Waiting for two observations", { exact: true })).toHaveCount(2);
  current.search.statistics.queries_received += 10;
  current.search.statistics.results_received += 20;
  current.search.statistics.queries_rejected += 5;
  current.search.statistics.query_queued += 5;
  current.search.statistics.result_queued += 10;
  await page.clock.runFor(5000);
  await expect(rates).toHaveText(["2", "4", "1", "3"]);
  await expect(page.getByTestId("search-monitoring").locator("polyline")).toHaveCount(2);
  await page.getByText("Recent intervals", { exact: true }).click();
  await expect(page.getByTestId("search-monitoring").locator("tbody tr")).toHaveCount(1);
  await expect(page.getByTestId("search-monitoring").locator("tbody td")).toHaveText(["5", "2", "4", "1", "3"]);
  const download = page.waitForEvent("download");
  await page.getByRole("button", { name: "Export snapshot", exact: true }).click();
  const exported = JSON.parse(await readFile((await (await download).path())!, "utf8"));
  expect(exported.search).toEqual(current.search);
  expect(exported).not.toHaveProperty("searchSamples");
  await page.getByRole("switch").click();
  await expect(page.getByText(/Automatic polling paused/)).toBeVisible();
  await expect(page.getByRole("button", { name: "Refresh now", exact: true })).toBeEnabled();
  const count = requests;
  await page.clock.runFor(10000);
  expect(requests).toBe(count);
  await expect(rates).toHaveText(["2", "4", "1", "3"]);
  await page.getByRole("button", { name: "Clear local observations", exact: true }).click();
  await expect(rates).toHaveText(["—", "—", "—", "—"]);
  await expect(page.getByTestId("search-query-statistics").locator(".metric-value").first()).toHaveText("11");
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(rates).toHaveText(["—", "—", "—", "—"]);
  await page.clock.runFor(5000);
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(rates).toHaveText(["0", "0", "0", "0"]);
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByRole("combobox").selectOption("pl");
  await expect(page.getByRole("heading", { name: "Monitoring wyszukiwań", exact: true })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await expect(page.getByText(/Automatyczne odpytywanie wstrzymane/)).toBeVisible();
  await page.screenshot({ path: testInfo.outputPath("monitoring-mobile-pl.png"), fullPage: true, animations: "disabled" });
});

test("search monitor drops stale baselines after errors, missing search, resets and long gaps", async ({ page }) => {
  await page.clock.install({ time: new Date("2026-11-13T12:00:00Z") });
  await page.clock.pauseAt(new Date("2026-11-13T12:00:01Z"));
  let current: unknown = { ...data, search: structuredClone(searchSnapshot) }, status = 200;
  await page.route("**/admin-api/v1/status", route => route.fulfill({ status, json: current }));
  await page.goto("/#search");
  await expect(page.getByText("API connected", { exact: true })).toBeVisible();
  const rates = page.getByTestId("search-rates").locator(".metric-value");
  await page.clock.runFor(5000);
  await expect(rates).toHaveText(["0", "0", "0", "0"]);
  await page.getByRole("switch").click();
  for (const next of [
    { ...data, server: { ...data.server, uptime_seconds: 1 }, search: searchSnapshot },
    { ...data, search: { ...searchSnapshot, statistics: { ...searchSnapshot.statistics, query_canceled: 0 } } },
    data,
    { ...data, search: { ...searchSnapshot, statistics: { ...searchSnapshot.statistics, results_received: -1 } } },
  ]) {
    current = next;
    await page.clock.runFor(5000);
    await page.getByRole("button", { name: "Refresh now", exact: true }).click();
    await expect(rates).toHaveText(["—", "—", "—", "—"]);
  }
  status = 502;
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(page.getByText("Connection lost", { exact: true })).toBeVisible();
  status = 200; current = { ...data, search: searchSnapshot };
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(rates).toHaveText(["—", "—", "—", "—"]);
  await page.clock.runFor(5000);
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(rates).toHaveText(["0", "0", "0", "0"]);
  await page.clock.runFor(16000);
  await page.getByRole("button", { name: "Refresh now", exact: true }).click();
  await expect(rates).toHaveText(["—", "—", "—", "—"]);
});
