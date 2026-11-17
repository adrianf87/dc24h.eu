import {test,expect} from "@playwright/test";
import {snapshot} from "./fixtures";
test("kick counters show command instructions, preserve precision and support Polish mobile",async({page})=>{
 await page.route("**/admin-api/v1/status",route=>route.fulfill({contentType:"application/json",body:JSON.stringify({...snapshot,kick:{attempts:"9223372036854775807",succeeded:"3",refused:"1",errors:"0",notice_failures:"1"}})}));
 await page.goto("/#kick");const panel=page.getByTestId("kick-statistics");
 await expect(page.getByRole("heading",{name:"Kick users",level:1})).toBeVisible();
 await expect(panel.locator("dd").first()).toHaveText("9,223,372,036,854,775,807");
 await expect(panel.getByText("+kick <nickname> [reason]",{exact:true})).toBeVisible();
 await page.setViewportSize({width:390,height:844});await page.getByLabel("Interface language").selectOption("pl");
 await expect(page.getByRole("heading",{name:"Wyrzucanie użytkowników",level:1})).toBeVisible();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
});
