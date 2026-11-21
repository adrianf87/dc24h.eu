import { expect, test } from "@playwright/test";
import { snapshot } from "./fixtures.ts";

const catalog = {
  schema: 2,
  maintenance: true,
  restart_required: true,
  areas: [
    { key: "accounts", permission: "accounts.manage", allowed: false, implemented: true, proof: "MgmtSecret" },
    { key: "roles", permission: "roles.manage", allowed: false, implemented: true },
    { key: "bans", permission: "users.ban+blocks.manage", allowed: true, implemented: true },
    { key: "configuration", permission: "configuration.manage", allowed: true, implemented: false },
    { key: "hub", permission: "hub.manage", allowed: false, implemented: true },
  ],
};

test("management stays unavailable without a catalog", async ({ page }) => {
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({ json: snapshot }));
  await page.route("**/admin-api/v1/management", (route) => route.fulfill({ status: 401, json: { error: "admin_grant_required" } }));
  await page.goto("/#management");
  await expect(page.getByRole("heading", { name: "Account, role, ban, configuration and hub management", exact: true })).toBeVisible();
  await expect(page.getByTestId("management")).toContainText("not available");
  await expect(page.getByTestId("management-accounts")).toHaveCount(0);
  await expect(page.locator("form")).toHaveCount(0);
});

test("management shows grant areas, links and Polish navigation", async ({ page }) => {
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({ json: snapshot }));
  await page.route("**/admin-api/v1/management", (route) => route.fulfill({ json: catalog }));
  await page.goto("/#management");
  await expect(page.getByRole("link", { name: "Management", exact: true })).toHaveAttribute("aria-current", "page");
  const accounts = page.getByTestId("management-accounts");
  await expect(accounts).toContainText("Not allowed for this grant");
  await expect(accounts).toContainText("accounts.manage");
  await expect(accounts.getByRole("link", { name: "Open accounts", exact: true })).toHaveAttribute("href", "#accounts");
  await expect(page.getByTestId("management-roles")).toContainText("Not allowed for this grant");
  await expect(page.getByTestId("management-roles").getByRole("link", { name: "Open the role catalog", exact: true })).toHaveAttribute("href", "#roles");
  const bans = page.getByTestId("management-bans");
  await expect(bans).toContainText("Allowed for this grant");
  await expect(bans).toContainText("users.ban+blocks.manage");
  await expect(bans.getByRole("link", { name: "Open bans", exact: true })).toHaveAttribute("href", "#bans");
  const configuration = page.getByTestId("management-configuration");
  // The broader management catalog is still unimplemented; the narrow
  // configuration editor can be opened under a configuration.manage grant.
  await expect(configuration).toContainText("configuration.manage");
  await expect(configuration).toContainText("Allowed for this grant");
  await expect(configuration.getByRole("link", { name: "Open configuration editor", exact: true })).toHaveAttribute("href", "#configuration");
  const hub = page.getByTestId("management-hub");
  await expect(hub).toContainText("Not implemented");
  await expect(hub).toContainText("hub.manage");
  await expect(hub).toContainText("Not allowed for this grant");
  await expect(hub.getByRole("link")).toHaveCount(0);
  await expect(page.getByTestId("management").locator("form, input, button")).toHaveCount(0);
  await expect(page.getByText("MgmtSecret")).toHaveCount(0);
  await expect(page.locator("input[type=password]")).toHaveCount(0);
  await expect(page.locator("form")).toHaveCount(0);
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByLabel("Interface language").selectOption("pl");
  await page.getByRole("button", { name: "Otwórz nawigację", exact: true }).click();
  await page.getByRole("link", { name: "Zarządzanie", exact: true }).click();
  await expect(page.getByRole("heading", { name: "Zarządzanie kontami, rolami, banami, konfiguracją i hubem", exact: true })).toBeVisible();
  await expect(page.getByTestId("management-configuration").getByRole("link", { name: "Otwórz edytor konfiguracji", exact: true })).toHaveAttribute("href", "#configuration");
  await expect(page.getByTestId("management-hub")).toContainText("Niezaimplementowane");
  await expect(page.getByTestId("management-bans")).toContainText("Dozwolone dla tego grantu");
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});

test("a malformed management catalog stays unavailable", async ({ page }) => {
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({ json: snapshot }));
  await page.route("**/admin-api/v1/management", (route) => route.fulfill({
    json: { schema: 2, maintenance: true, restart_required: true, areas: [{ key: "accounts", permission: "accounts.manage", allowed: true, implemented: true }], secret: "MgmtSecret" },
  }));
  await page.goto("/#management");
  await expect(page.getByTestId("management")).toContainText("not available");
  await expect(page.getByText("MgmtSecret", { exact: true })).toHaveCount(0);
});
