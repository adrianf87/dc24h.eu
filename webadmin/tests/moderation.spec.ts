import { test, expect } from "@playwright/test";
import { snapshot } from "./fixtures";
import { configuration } from "./configuration-fixture";
import { MODERATION_RULES } from "../src/lib/moderation";

test("moderation rules are read-only, bilingual and absent on older APIs", async ({ page }) => {
  let mode = "current";
  await page.route("**/admin-api/v1/status", (route) => route.fulfill({
    contentType: "application/json",
    body: JSON.stringify({
      ...snapshot,
      ...(mode === "old" ? {} : {
        configuration: {
          ...configuration,
          moderation: mode === "malformed"
            ? MODERATION_RULES.map((rule, index) => index === 0 ? { ...rule, forbid_self: false, nickname: "PrivateTarget" } : rule)
            : MODERATION_RULES.map((rule) => ({ ...rule, nickname: "PrivateTarget" })),
        },
      }),
    }),
  }));
  await page.goto("/#moderation");
  const panel = page.getByTestId("moderation-rules");
  await expect(page.getByRole("heading", { name: "Moderation rules", level: 1 })).toBeVisible();
  await expect(panel.locator("tbody tr")).toHaveCount(7);
  await expect(panel.locator("tr[data-operation='kick']")).toContainText("users.kick");
  await expect(panel.locator("input,button,select,textarea")).toHaveCount(0);
  expect(await page.content()).not.toContain("PrivateTarget");
  mode = "old";
  await page.getByRole("button", { name: "Refresh now" }).click();
  await expect(panel.getByText("This API does not publish moderation rules.", { exact: true })).toBeVisible();
  mode = "malformed";
  await page.getByRole("button", { name: "Refresh now" }).click();
  await expect(panel.getByText("This API does not publish moderation rules.", { exact: true })).toBeVisible();
  mode = "current";
  await page.getByRole("button", { name: "Refresh now" }).click();
  await expect(panel.locator("tbody tr")).toHaveCount(7);
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByLabel("Interface language").selectOption("pl");
  await expect(page.getByRole("heading", { name: "Reguły moderacji", level: 1 })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});
