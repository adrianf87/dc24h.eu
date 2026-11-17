import {test,expect} from "@playwright/test";
test("staff directory uses SQL-filtered cursor pages and supports guarded demotion in Polish",async({page})=>{
 let seen="";
 const staff={id:"40",name:"StaffOperator",role:"operator",status:"disabled",revision:"2",created_at:0,updated_at:0,last_login_at:null};
 await page.route("**/admin-api/v1/accounts**",async route=>{
  if(route.request().method()==="POST") {
   seen=route.request().postData()??"";
   await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,account:{...staff,role:"registered",revision:"3"}})});
  }else {
   expect(route.request().url()).toContain("?staff_after=0");
   await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,staff_only:true,accounts:[staff],next_after:null})});
  }
 });
 await page.goto("/#operators");
 await expect(page.getByRole("heading",{level:1,name:"Operators and hub owner"})).toBeVisible();
 await expect(page.getByText(/First Owner creation/)).toBeVisible();
 await page.getByRole("button",{name:"StaffOperator",exact:true}).click();
 await page.getByLabel("Operation",{exact:true}).selectOption("role");
 await page.getByLabel("New role (must be below the actor)").selectOption("registered");
 await page.getByLabel("Active administrator account ID").fill("1");
 await page.getByLabel("Administrator account password").fill("Private staff proof!");
 await page.getByRole("checkbox").check();await page.getByRole("button",{name:"Apply operation"}).click();
 await expect(page.getByRole("button",{name:"StaffOperator",exact:true})).toHaveCount(0);
 expect(new URLSearchParams(seen).get("revision")).toBe("2");
 await expect(page.getByLabel("Administrator account password")).toHaveValue("");
 await page.setViewportSize({width:390,height:844});await page.getByLabel("Interface language").selectOption("pl");
 await expect(page.getByRole("heading",{level:1,name:"Operatorzy i właściciel huba"})).toBeVisible();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
});
test("staff directory refuses older servers that return an unfiltered account page",async({page})=>{
 await page.route("**/admin-api/v1/accounts**",route=>route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,accounts:[],next_after:null})}));
 await page.goto("/#operators");await expect(page.locator(".accounts-message")).toContainText("unavailable");
 await expect(page.getByRole("button",{name:"Apply operation"})).toBeDisabled();
});
