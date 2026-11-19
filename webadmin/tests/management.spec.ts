import { expect, test } from "@playwright/test";
import { snapshot } from "./fixtures.ts";

const catalog = {
  schema: 1,
  maintenance: true,
  restart_required: true,
  areas: [
    { key: "accounts", permission: "accounts.manage", allowed: false, proof: "MgmtSecret" },
    { key: "roles", permission: "roles.manage", allowed: false },
    { key: "bans", permission: "users.ban+blocks.manage", allowed: true },
  ],
};

test("management stays unavailable without a catalog", async ({ page }) => {
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({ json: snapshot }));
  await page.route("**/admin-api/v1/management", (route) => route.fulfill({ status: 401, json: { error: "admin_grant_required" } }));
  await page.goto("/#management");
  await expect(page.getByRole("heading", { name: "Account, role and ban management", exact: true })).toBeVisible();
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
  await expect(page.getByText("MgmtSecret")).toHaveCount(0);
  await expect(page.locator("input[type=password]")).toHaveCount(0);
  await expect(page.locator("form")).toHaveCount(0);
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByLabel("Interface language").selectOption("pl");
  await page.getByRole("button", { name: "Otwórz nawigację", exact: true }).click();
  await page.getByRole("link", { name: "Zarządzanie", exact: true }).click();
  await expect(page.getByRole("heading", { name: "Zarządzanie kontami, rolami i banami", exact: true })).toBeVisible();
  await expect(page.getByTestId("management-bans")).toContainText("Dozwolone dla tego grantu");
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});

test("a malformed management catalog stays unavailable", async ({ page }) => {
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({ json: snapshot }));
  await page.route("**/admin-api/v1/management", (route) => route.fulfill({
    json: { schema: 1, maintenance: true, restart_required: true, areas: [{ key: "accounts", permission: "accounts.manage", allowed: true }], secret: "MgmtSecret" },
  }));
  await page.goto("/#management");
  await expect(page.getByTestId("management")).toContainText("not available");
  await expect(page.getByText("MgmtSecret", { exact: true })).toHaveCount(0);
});
