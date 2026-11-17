import {test,expect} from "@playwright/test";
import {snapshot} from "./fixtures";
import {configuration} from "./configuration-fixture";
import {rolePolicy} from "./roles-fixture";
test("seven-role policy is bilingual and separates grants from available actions",async({page})=>{
 await page.route("**/admin-api/v1/status",route=>route.fulfill({contentType:"application/json",body:JSON.stringify({...snapshot,configuration:{...configuration,roles:rolePolicy(["users.kick"])}})}));
 await page.goto("/#roles");const panel=page.getByTestId("role-policy");
 await expect(page.getByRole("heading",{level:1,name:"Roles and permissions"})).toBeVisible();
 await page.getByLabel("Inspect role").selectOption("operator");
 await expect(panel.locator('[data-permission="users.ban"] td').nth(1)).toHaveText("Yes");
 await expect(panel.locator('[data-permission="users.ban"] td').nth(2)).toContainText("Available service");
 await expect(panel.locator('[data-permission="accounts.manage"] td').nth(1)).toHaveText("No");
 await page.getByLabel("Inspect role").selectOption("vip");
 await expect(panel.locator('[data-permission="users.kick"] td').nth(1)).toHaveText("Yes");
 await page.getByLabel("Inspect role").selectOption("owner");
 await expect(panel.locator('[data-permission="roles.manage"] td').nth(1)).toHaveText("Yes");
 await page.setViewportSize({width:390,height:844});await page.getByLabel("Interface language").selectOption("pl");
 await expect(page.getByRole("heading",{level:1,name:"Role i uprawnienia"})).toBeVisible();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
});
test("account role changes submit proof, revision and selected role without storing secrets",async({page})=>{
 const account={id:"2",name:"RoleTarget",role:"registered",status:"active",revision:"4",created_at:0,updated_at:0,last_login_at:null};let saved="";
 await page.route("**/admin-api/v1/accounts**",async route=>{
  if(route.request().method()==="POST"){
   saved=route.request().postData()??"";await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,account:{...account,role:"vip",status:"disabled",revision:"5"}})});
  }else await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,accounts:[account],next_after:null})});
 });
 await page.goto("/#accounts");await page.getByRole("button",{name:"RoleTarget",exact:true}).click();
 await page.getByLabel("Operation",{exact:true}).selectOption("role");
 await page.getByLabel("New role (must be below the actor)").selectOption("vip");
 await page.getByLabel("Active administrator account ID").fill("1");await page.getByLabel("Administrator account password").fill("Private role proof!");
 await page.getByRole("checkbox").check();await page.getByRole("button",{name:"Apply operation"}).click();
 await expect(page.locator(".accounts-message")).toContainText("Account saved");
 expect(new URLSearchParams(saved).get("role")).toBe("vip");expect(new URLSearchParams(saved).get("revision")).toBe("4");
 await expect(page.getByLabel("Administrator account password")).toHaveValue("");
 expect(await page.evaluate(()=>JSON.stringify({...localStorage,...sessionStorage}))).not.toContain("Private role proof");
});
