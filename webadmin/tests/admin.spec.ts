import {test,expect} from "@playwright/test";
import {administration} from "./admin-fixture";
test("Admin Core shows process outcomes and local-only ownership, clears stale results and supports Polish mobile",async({page})=>{
 let available=true;
 await page.route("**/admin-api/v1/admin",route=>available?route.fulfill({contentType:"application/json",body:JSON.stringify(administration)}):route.fulfill({status:503,contentType:"application/json",body:'{"error":"unavailable"}'}));
 await page.goto("/#adminCore");const panel=page.getByTestId("admin-core");
 await expect(page.getByRole("heading",{name:"Central administration",level:1})).toBeVisible();
 await expect(panel.locator("tbody tr")).toHaveCount(10);
 await expect(panel.getByText("Local owner tool only",{exact:true})).toHaveCount(2);
 await expect(panel.locator("dd").first()).toHaveText("9");
 await page.setViewportSize({width:390,height:844});await page.getByLabel("Interface language").selectOption("pl");
 await expect(page.getByRole("heading",{name:"Centralna administracja",level:1})).toBeVisible();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 available=false;await panel.getByRole("button",{name:"Odśwież konta"}).click();
 await expect(panel.locator("tbody tr")).toHaveCount(0);await expect(panel.getByRole("status")).toContainText("niedostępny");
});
