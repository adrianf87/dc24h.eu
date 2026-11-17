import {test,expect} from "@playwright/test";
const account={id:"1",name:'Account"<script>',role:"registered",status:"active",revision:"2",created_at:1,updated_at:2,last_login_at:null};
test("accounts selection, confirmed writes, stale errors and Polish mobile",async({page})=>{
 let revision="2",saved="",refuse=false;
 await page.route("**/admin-api/v1/accounts**",async route=>{
  if(route.request().method()==="POST"){
   saved=route.request().postData()??"";
   await route.fulfill({status:refuse?409:200,contentType:"application/json",body:JSON.stringify(refuse?{error:"stale"}:{account:{...account,status:"disabled",revision:"3"},maintenance:true})});revision="3";
  }else await route.fulfill({contentType:"application/json",body:JSON.stringify({accounts:[{...account,revision}],maintenance:true,next_after:null})});
 });
 await page.goto("/#accounts");
 await expect(page.getByRole("heading",{name:"Account management",level:1})).toBeVisible();
 await page.getByRole("button",{name:account.name,exact:true}).click();
 await page.getByLabel("Active administrator account ID").fill("4");await page.getByLabel("Administrator account password").fill("Private account proof!");
 await page.getByRole("checkbox").check();await page.getByRole("button",{name:"Apply operation"}).click();
 await expect(page.locator(".accounts-message")).toContainText("Account saved");expect(saved).toContain("revision=2");expect(saved).toContain("actor_id=4");
 await expect(page.getByLabel("Administrator account password")).toHaveValue("");
 expect(await page.evaluate(()=>JSON.stringify({...localStorage,...sessionStorage}))).not.toContain("Private account proof");
 refuse=true;await page.getByLabel("Active administrator account ID").fill("4");await page.getByLabel("Administrator account password").fill("Private account proof!");await page.getByRole("checkbox").check();
 await page.getByRole("button",{name:"Apply operation"}).click();await expect(page.locator(".accounts-message")).toContainText("account changed");
 await page.setViewportSize({width:390,height:844});await page.getByLabel("Interface language").selectOption("pl");
 await expect(page.getByRole("heading",{name:"Zarządzanie kontami",level:1})).toBeVisible();expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
});
test("unavailable account service disables writes",async({page})=>{
 await page.goto("/#accounts");await expect(page.locator(".accounts-message")).toContainText("maintenance is unavailable");
 await expect(page.getByRole("button",{name:"Apply operation"})).toBeDisabled();
});
