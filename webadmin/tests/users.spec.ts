import { expect, test } from "@playwright/test";
import { snapshot } from "./fixtures.ts";
import { configuration } from "./configuration-fixture.ts";
import { rolePolicy } from "./roles-fixture.ts";

const directory = {
  schema: 1,
  truncated: false,
  users: [{
    session: "7",
    sid: "ABCD",
    nickname: "Quoted Nick",
    transport: "adc",
    role: "guest",
    public_muted: false,
    private_muted: true,
    password: "UserSecret",
  }],
};

test("users stay unavailable when the directory is absent", async ({ page }) => {
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({ json: snapshot }));
  await page.route("**/admin-api/v1/users", (route) => route.fulfill({ status: 404, json: { error: "not_found" } }));
  await page.goto("/#users");
  await expect(page.getByRole("heading", { name: "Online users", exact: true })).toBeVisible();
  await expect(page.getByTestId("hub-users")).toContainText("not available");
  await expect(page.getByText("Quoted Nick")).toHaveCount(0);
});

test("users show public fields, moderation decisions and Polish navigation", async ({ page }) => {
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({
    json: { ...snapshot, configuration: { ...configuration, roles: rolePolicy() } },
  }));
  await page.route("**/admin-api/v1/users", (route) => route.fulfill({ json: directory }));
  await page.goto("/#users");
  await expect(page.getByRole("link", { name: "Users", exact: true })).toHaveAttribute("aria-current", "page");
  const row = page.getByTestId("hub-user-row");
  await expect(row).toContainText("Quoted Nick");
  await expect(row).toContainText("ABCD");
  await expect(row).toContainText("Guest");
  await expect(row).toContainText("+kick Quoted Nick");
  await expect(row.locator("[data-operation='kick']")).toContainText("Allowed");
  await expect(row.locator("[data-operation='mute']")).toContainText("Allowed");
  await expect(page.getByText("UserSecret")).toHaveCount(0);
  await page.getByLabel("Decision role").selectOption("guest");
  await expect(row.locator("[data-operation='kick']")).toContainText("Missing permission");
  await page.getByLabel("Decision role").selectOption("moderator");
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByLabel("Interface language").selectOption("pl");
  await page.getByRole("button", { name: "Otwórz nawigację", exact: true }).click();
  await page.getByRole("link", { name: "Użytkownicy", exact: true }).click();
  await expect(page.getByRole("heading", { name: "Użytkownicy online", exact: true })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});

test("a malformed user directory stays unavailable", async ({ page }) => {
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({ json: snapshot }));
  await page.route("**/admin-api/v1/users", (route) => route.fulfill({
    json: { schema: 1, truncated: false, users: [{ ...directory.users[0], nickname: "bad\nname" }] },
  }));
  await page.goto("/#users");
  await expect(page.getByTestId("hub-users")).toContainText("not available");
  await expect(page.getByText("bad\nname", { exact: true })).toHaveCount(0);
});
