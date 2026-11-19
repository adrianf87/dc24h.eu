import {test,expect} from "@playwright/test";
const event={id:"4",occurred_at:10,action:"accounts.create",actor_id:"1",target_kind:"account",target_id:"2",outcome:"succeeded"};
test("audit history loads a proven page and clears the password in Polish mobile",async({page})=>{
 const bodies:string[]=[];
 await page.route("**/admin-api/v1/audit",async route=>{
  bodies.push(route.request().postData()??"");
  await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,restart_required:true,events:[{...event,proof:"hidden-proof"}],next_after:null})});
 });
 await page.goto("/#audit");
 await expect(page.getByRole("heading",{name:"Administrative history",level:1})).toBeVisible();
 await page.getByLabel("Active account ID with audit.read").fill("1");
 await page.getByLabel("Account password").fill("Secret audit proof!");
 await page.getByRole("button",{name:"Show history",exact:true}).click();
 const panel=page.getByTestId("audit");
 await expect(panel.getByRole("status")).toContainText("History loaded");
 await expect(panel.getByRole("cell",{name:"accounts.create"})).toBeVisible();
 await expect(page.getByLabel("Account password")).toHaveValue("");
 expect(new URLSearchParams(bodies[0]).get("after")).toBe("0");
 expect(panel).not.toContainText("hidden-proof");
 await page.setViewportSize({width:390,height:844});
 await page.getByLabel("Interface language").selectOption("pl");
 await expect(page.getByRole("heading",{name:"Historia działań administracyjnych",level:1})).toBeVisible();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 expect(await page.evaluate(()=>JSON.stringify({...localStorage,...sessionStorage}))).not.toContain("Secret audit proof");
});
test("an unavailable audit service shows no fabricated history",async({page})=>{
 await page.route("**/admin-api/v1/audit",async route=>{
  await route.fulfill({status:503,contentType:"application/json",body:'{"error":"api_unavailable"}'});
 });
 await page.goto("/#audit");
 const panel=page.getByTestId("audit");
 await page.getByLabel("Active account ID with audit.read").fill("1");
 await page.getByLabel("Account password").fill("Secret audit proof!");
 await page.getByRole("button",{name:"Show history",exact:true}).click();
 await expect(panel.getByRole("status")).toHaveText("Audit history is unavailable.");
 await expect(panel.getByRole("cell",{name:"accounts.create"})).toHaveCount(0);
 await expect(page.getByLabel("Account password")).toHaveValue("");
});
