import {test, expect} from "@playwright/test";
import {ACCESS_ROUTES} from "../src/lib/access";
import {PERMISSION_KEYS} from "../src/lib/roles";
import {administration} from "./admin-fixture";

const grant = "ab".repeat(32);
const session = "cd".repeat(16);
const csrf = "ef".repeat(32);
function accessBody() {
  return {
    maintenance: true, restart_required: true, expires_in: 600,
    actor: {id: "1", role: "administrator", status: "active"}, grant, session, csrf,
    permissions: PERMISSION_KEYS.map(key => ({key, granted: key !== "hub.manage", implemented: key !== "configuration.manage" && key !== "hub.manage"})),
    routes: ACCESS_ROUTES.map(([method, path, permission]) => ({method, path, permission, allowed: true})),
    token: "hidden-token",
  };
}

test("proved access stays out of web storage and marks pending permissions", async ({page}) => {
  const seen: string[] = [];
  await page.route("**/admin-api/v1/**", async route => {
    const url = route.request().url();
    if (url.includes("/admin-api/v1/admin/access")) {
      await route.fulfill({contentType: "application/json", body: JSON.stringify(accessBody())});
      return;
    }
    if (url.includes("/admin-api/v1/admin")) {
      seen.push(route.request().headers()["x-dc24h-admin-grant"] ?? "");
      seen.push(route.request().headers()["x-dc24h-csrf"] ?? "");
      await route.fulfill({contentType: "application/json", body: JSON.stringify(administration)});
      return;
    }
    await route.fallback();
  });
  await page.goto("/#adminCore");
  await page.getByLabel("Active account ID").fill("1");
  await page.getByLabel("Account password").fill("Secret access proof!");
  await page.getByRole("button", {name: "Prove access", exact: true}).click();
  await expect(page.getByTestId("admin-access-result")).toContainText("Access proved for 10 minutes");
  await expect(page.getByLabel("Account password")).toHaveValue("");
  await expect(page.getByTestId("admin-access-pending")).toContainText("configuration.manage");
  await expect(page.getByTestId("admin-access-pending")).toContainText("Pending");
  await expect(page.getByTestId("admin-core")).not.toContainText(grant);
  await expect(page.getByTestId("admin-core")).not.toContainText(csrf);
  await expect(page.getByTestId("admin-core")).not.toContainText(session);
  expect(seen.at(-2)).toBe(grant);
  expect(seen.at(-1)).toBe("");
  const stored = await page.evaluate(() => JSON.stringify({...localStorage, ...sessionStorage}));
  expect(stored).not.toContain(grant);
  expect(stored).not.toContain(csrf);
  expect(stored).not.toContain(session);
  expect(await page.evaluate(() => document.body.innerText)).not.toContain("Secret access proof");
  await page.setViewportSize({width: 390, height: 844});
  await page.getByLabel("Interface language").selectOption("pl");
  await expect(page.getByRole("heading", {name: "Centralna administracja", level: 1})).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});
