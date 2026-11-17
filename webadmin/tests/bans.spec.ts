import {test,expect} from "@playwright/test";
const ban={id:"7",kind:"ip",subject:"192.0.2.1",reason:"Private policy",created_at:1,expires_at:null,created_by:"1",revoked_at:null,revoked_by:null,revision:"3"};
test("ban maintenance creates and revokes with proof, revision and restart guidance in Polish mobile",async({page})=>{
 const requests:string[]=[];
 await page.route("**/admin-api/v1/bans**",async route=>{
  if(route.request().method()==="POST"){
   requests.push(route.request().postData()??"");const revoked=route.request().url().endsWith("/revoke");
   await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,restart_required:true,ban:{...ban,revoked_at:revoked?2:null,revoked_by:revoked?"1":null,revision:revoked?"4":"3"}})});
  }else await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,restart_required:true,bans:[ban],next_after:null})});
 });
 await page.goto("/#bans");await expect(page.getByRole("heading",{name:"Account, CID and IP bans",level:1})).toBeVisible();
 await page.getByLabel("Block type").selectOption("ip");await page.getByLabel("Account ID, CID or IP address").fill("::ffff:192.0.2.1");await page.getByLabel("Reason",{exact:true}).fill("Private policy");
 await page.getByLabel("Active Operator or higher account ID").fill("1");await page.getByLabel("Actor account password").fill("Secret ban proof!");await page.getByRole("checkbox").check();await page.getByRole("button",{name:"Create block",exact:true}).last().click();
 await expect(page.getByTestId("bans").getByRole("status")).toContainText("Restart");await expect(page.getByLabel("Actor account password")).toHaveValue("");
 await page.getByLabel("Active Operator or higher account ID").fill("1");await page.getByLabel("Actor account password").fill("Secret ban proof!");await page.getByRole("checkbox").check();await page.getByRole("button",{name:"Revoke selected block",exact:true}).click();
 expect(new URLSearchParams(requests[0]).get("kind")).toBe("ip");expect(new URLSearchParams(requests[1]).get("revision")).toBe("3");
 await page.setViewportSize({width:390,height:844});await page.getByLabel("Interface language").selectOption("pl");await expect(page.getByRole("heading",{name:"Blokady kont, CID i IP",level:1})).toBeVisible();
 expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
 expect(await page.evaluate(()=>JSON.stringify({...localStorage,...sessionStorage}))).not.toContain("Secret ban proof");
});

test("duration, server-clock expiry and status filters work despite browser clock skew",async({page})=>{
 let query="",submitted="";const timed={...ban,expires_at:103};
 await page.addInitScript(()=>{Date.now=()=>253402300000000;});
 await page.route("**/admin-api/v1/bans**",async route=>{
  const post=route.request().method()==="POST";query=new URL(route.request().url()).search;
  if(post)submitted=route.request().postData()??"";
  await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,restart_required:true,observed_at:100,...(post?{ban:timed}:{bans:[timed],next_after:null})})});
 });
 await page.goto("/#bans");
 const panel=page.getByTestId("bans");
 await expect(panel.getByRole("cell",{name:"Temporary",exact:true})).toBeVisible();
 await panel.getByRole("button",{name:"7",exact:true}).click();
 await expect(panel.getByRole("cell",{name:"Expired",exact:true})).toBeVisible({timeout:6000});
 await expect(panel.locator("legend")).toHaveText("Revoke selected block");
 await page.getByLabel("Block status filter").selectOption("expired");
 await expect.poll(()=>query).toBe("?after=0&state=expired");
 await page.getByLabel("Block status filter").selectOption("all");
 await panel.getByRole("button",{name:"Create block",exact:true}).first().click();
 await page.getByLabel("Block type").selectOption("ip");await page.getByLabel("Account ID, CID or IP address").fill("192.0.2.1");await page.getByLabel("Reason",{exact:true}).fill("Private policy");
 await page.getByLabel("Block lifetime").selectOption("duration");await page.getByLabel("Duration (whole units)").fill("2");await page.getByLabel("Time unit").selectOption("hours");
 await page.getByLabel("Active Operator or higher account ID").fill("1");await page.getByLabel("Actor account password").fill("Secret duration proof!");await page.getByRole("checkbox").check();
 await panel.getByRole("button",{name:"Create block",exact:true}).last().click();
 await expect(panel.getByRole("status")).toContainText("Restart");
 const fields=new URLSearchParams(submitted);expect(fields.get("duration_seconds")).toBe("7200");expect(fields.has("expires_at")).toBe(false);
 await expect(page.getByLabel("Actor account password")).toHaveValue("");
 await page.setViewportSize({width:390,height:844});await page.getByLabel("Interface language").selectOption("pl");
 await expect(page.getByLabel("Filtr stanu blokady")).toBeVisible();expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
});

test("older or unavailable APIs never fabricate timed state from the local clock",async({page})=>{
 let offline=false;
 await page.route("**/admin-api/v1/bans**",async route=>{
  if(offline){await route.fulfill({status:503,contentType:"application/json",body:'{"error":"api_unavailable"}'});return;}
  await route.fulfill({contentType:"application/json",body:JSON.stringify({maintenance:true,restart_required:true,bans:[{...ban,expires_at:2}],next_after:null})});
 });
 await page.goto("/#bans");const panel=page.getByTestId("bans");
 await expect(panel.getByRole("cell",{name:"Server time unavailable",exact:true})).toBeVisible();
 await expect(panel.getByRole("cell",{name:"Expired",exact:true})).toHaveCount(0);
 offline=true;await panel.getByRole("button",{name:"Refresh blocks",exact:true}).click();
 await expect(panel.locator("tbody tr")).toHaveCount(0);
 await expect(panel.getByRole("button",{name:"Create block",exact:true}).last()).toBeDisabled();
});
