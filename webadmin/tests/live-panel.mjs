import assert from "node:assert/strict";
import { chromium, expect } from "@playwright/test";
let secret = "";
for await (const chunk of process.stdin) secret += chunk;
secret = secret.trim();
const browser = await chromium.launch({
  headless: true,
  ...(process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE
    ? { executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE }
    : {}),
});
try {
  const page = await browser.newPage({
    viewport: { width: 1440, height: 1080 },
  });
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  let credentialLeak = false;
  page.on("request", (request) => {
    if (request.headers().authorization?.includes(secret))
      credentialLeak = true;
  });
  await page.goto(process.argv[2]);
  await expect(page.getByText("API connected", { exact: true })).toBeVisible();
  await expect(page.getByText("alpha-0.6.4", { exact: true })).toHaveCount(2);
  await expect(page.locator(".metric-value").nth(1)).toHaveText("11");
  assert.ok(
    Number(await page.locator(".metric-value").first().textContent()) >= 2,
  );
  const status = await page.request.get(
    new URL("admin-api/v1/status", process.argv[2]).href,
  );
  assert.equal(status.status(), 200);
  const snapshot = await status.json();
  assert.deepEqual(snapshot.users, { online: 2, peak_online: 2, successful_logins: 2 });
  await expect(page.getByTestId("user-statistics").locator(".metric-value")).toHaveText(["2", "2", "2", "0"]);
  assert.deepEqual(snapshot.chat, { received: 5, public_dispatches: 1, private_dispatches: 2,
    rejected: 2, flood_limited: 0, errors: 0, queued: 5, unavailable: 0, refused: 0 });
  assert.ok(!(await status.text()).includes("WebChatSecret"));
  assert.ok(!(await status.text()).includes(secret));
  assert.ok(!(await page.content()).includes(secret));
  assert.equal(credentialLeak, false);
  const forbidden = await page.request.get(
    new URL("/@fs/" + process.argv[3], process.argv[2]).href,
  );
  assert.equal(forbidden.status(), 403);
  assert.ok(!(await forbidden.text()).includes(secret));
  await page.screenshot({
    path: "test-results/live-desktop.png",
    animations: "disabled", fullPage: true,
  });
  await page.getByRole("link", { name: "Chat", exact: true }).click();
  await expect(page.getByTestId("chat-details").locator(".metric-value"))
    .toHaveText(["5", "1", "2", "2", "0", "0", "5", "0", "0"]);
  assert.ok(!(await page.content()).includes("WebChatSecret"));
  await page.screenshot({ path: "test-results/live-chat-desktop.png", animations: "disabled", fullPage: true });
  await page.getByRole("link", { name: "Search", exact: true }).click();
  await expect(page.getByTestId("search-query-statistics").locator(".metric-value")).toHaveText(["1", "1", "0", "0", "0", "0", "1", "0", "0", "0"]);
  await expect(page.getByTestId("search-result-statistics").locator(".metric-value")).toHaveText(["1", "1", "0", "0", "0", "0", "1", "0", "0"]);
  await expect(page.locator('[data-setting="max_active_per_user"]')).toHaveText("8");
  console.log("SEARCH_MONITOR_READY");
  await expect(page.getByTestId("search-query-statistics").locator(".metric-value").first()).toHaveText("3", { timeout: 12000 });
  await expect(page.getByTestId("search-result-statistics").locator(".metric-value").first()).toHaveText("3");
  const rates = page.getByTestId("search-rates").locator(".metric-value");
  assert.ok(Number(await rates.nth(0).textContent()) > 0, "Live queries did not produce a positive sampled rate");
  assert.ok(Number(await rates.nth(1).textContent()) > 0, "Live results did not produce a positive sampled rate");
  await expect(page.getByTestId("search-monitoring").locator("polyline")).toHaveCount(2);
  assert.ok(!(await page.content()).includes("WebSearchSecret")); assert.ok(!JSON.stringify(snapshot).includes("WebSearchSecret"));
  await page.screenshot({ path: "test-results/live-search-desktop.png", animations: "disabled", fullPage: true });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByRole("combobox").selectOption("pl");
  await expect(
    page.getByRole("heading", { name: "Monitoring wyszukiwań" }),
  ).toBeVisible();
  assert.ok(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  );
  await page.screenshot({
    path: "test-results/live-search-mobile-pl.png",
    animations: "disabled", fullPage: true,
  });
  assert.deepEqual(errors, []);
  console.log(
    "Real C++ API → local adapter → React: live search rates, counters, secret isolation, desktop and mobile PL OK",
  );
} finally {
  await browser.close();
}
