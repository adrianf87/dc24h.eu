import { expect, test } from "@playwright/test";
import { WEBADMIN_VERSION } from "../src/lib/status";
const data = {
  api_version: "v1",
  server: {
    name: "dc24h.eu",
    version: "alpha-0.4.1",
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
  await page.getByRole("combobox").selectOption("pl");
  await expect(page.getByText("Użytkownicy online", { exact: true })).toBeVisible();
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
