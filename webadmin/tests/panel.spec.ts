import { expect, test } from "@playwright/test";
import { WEBADMIN_VERSION } from "../src/lib/status";
import { readFile } from "node:fs/promises";
const data = {
  api_version: "v1",
  server: {
    name: "dc24h.eu",
    version: "alpha-0.5.2",
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
