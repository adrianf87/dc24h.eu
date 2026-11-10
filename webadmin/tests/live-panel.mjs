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
  await expect(page.getByText("alpha-0.3.12", { exact: true })).toHaveCount(2);
  await expect(page.locator(".metric-value").nth(1)).toHaveText("1");
  assert.ok(
    Number(await page.locator(".metric-value").first().textContent()) >= 1,
  );
  const status = await page.request.get(
    new URL("admin-api/v1/status", process.argv[2]).href,
  );
  assert.equal(status.status(), 200);
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
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByRole("combobox").selectOption("pl");
  await expect(
    page.getByRole("heading", { name: "Przegląd huba" }),
  ).toBeVisible();
  assert.ok(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  );
  await page.screenshot({
    path: "test-results/live-mobile-pl.png",
    fullPage: true,
  });
  assert.deepEqual(errors, []);
  console.log(
    "Real C++ API → local adapter → React: counters, secret isolation, desktop and mobile PL OK",
  );
} finally {
  await browser.close();
}
