import {test, expect} from "@playwright/test";
import {ACCESS_ROUTES} from "../src/lib/access";
import {PERMISSION_KEYS} from "../src/lib/roles";
import {administration} from "./admin-fixture";
import {snapshot} from "./fixtures";

const grant="ab".repeat(32),session="cd".repeat(16),csrf="ef".repeat(32);
const access={maintenance:true,restart_required:true,expires_in:600,actor:{id:"1",role:"owner",status:"active"},
  grant,session,csrf,permissions:PERMISSION_KEYS.map(key=>({key,granted:true,implemented:key!=="configuration.manage"})),
  routes:ACCESS_ROUTES.map(([method,path,permission])=>({method,path,permission,allowed:true}))};
const areas=[
  {key:"accounts",permission:"accounts.manage",allowed:true,implemented:true},
  {key:"roles",permission:"roles.manage",allowed:true,implemented:true},
  {key:"bans",permission:"users.ban+blocks.manage",allowed:true,implemented:true},
  {key:"configuration",permission:"configuration.manage",allowed:true,implemented:false},
  {key:"hub",permission:"hub.manage",allowed:true,implemented:true},
];
const systemd=(active:boolean)=>({schema:1,available:true,unit:"dc24h.service",load_state:"loaded",
  active_state:active?"active":"inactive",sub_state:active?"running":"dead",result:"success"});

test("Owner sees status, confirms Stop, and observes verified state and audit link",async({page})=>{
  let active=true;
  const calls:{method:string;path:string;body:string;grant:string;csrf:string}[]=[];
  await page.route("**/admin-api/v1/**",route=>{
    const request=route.request(),path=new URL(request.url()).pathname;
    if(path==="/admin-api/v1/admin/access")return route.fulfill({json:access});
    if(path==="/admin-api/v1/admin")return route.fulfill({json:administration});
    if(path==="/admin-api/v1/status")return route.fulfill({json:snapshot});
    if(path==="/admin-api/v1/management")return route.fulfill({json:{schema:2,maintenance:true,restart_required:true,areas}});
    if(path.startsWith("/admin-api/v1/management/hub/")){
      calls.push({method:request.method(),path,body:request.postData()??"",grant:request.headers()["x-dc24h-admin-grant"]??"",
        csrf:request.headers()["x-dc24h-csrf"]??""});
      if(request.method()==="POST"){
        active=false;
        return route.fulfill({json:{operation:"stop",outcome:"succeeded",active_state:"inactive",secret:"omit"}});
      }
      return route.fulfill({json:systemd(active)});
    }
    return route.fallback();
  });
  await page.goto("/#adminCore");
  await page.getByLabel("Active account ID").fill("1");
  await page.getByLabel("Account password").fill("Valid proof");
  await page.getByRole("button",{name:"Prove access",exact:true}).click();
  await expect(page.getByTestId("admin-access-result")).toContainText("Access proved");
  await page.evaluate(()=>{location.hash="#management";});
  await expect(page.getByTestId("hub-service-state")).toHaveText("active");
  await expect(page.getByTestId("hub-start")).toBeDisabled();
  await page.getByTestId("hub-stop").click();
  await expect(page.getByRole("button",{name:"Confirm operation"})).toBeDisabled();
  await page.getByText("I understand the impact").click();
  await page.getByRole("button",{name:"Confirm operation"}).click();
  await expect(page.getByTestId("hub-service-state")).toHaveText("inactive");
  await expect(page.getByTestId("hub-service-notice")).toContainText("succeeded");
  await expect(page.getByTestId("hub-start")).toBeEnabled();
  await expect(page.getByRole("link",{name:"Open administrative history"})).toHaveAttribute("href","#audit");
  const writes=calls.filter(item=>item.method==="POST");
  expect(writes).toHaveLength(1);
  expect(writes[0]).toMatchObject({path:"/admin-api/v1/management/hub/stop",body:"confirm=STOP",grant,csrf});
});

test("unknown service outcome is never retried",async({page})=>{
  let calls=0;
  await page.route("**/admin-api/v1/**",route=>{
    const path=new URL(route.request().url()).pathname;
    if(path==="/admin-api/v1/admin/access")return route.fulfill({json:access});
    if(path==="/admin-api/v1/admin")return route.fulfill({json:administration});
    if(path==="/admin-api/v1/status")return route.fulfill({json:snapshot});
    if(path==="/admin-api/v1/management")return route.fulfill({json:{schema:2,maintenance:true,restart_required:true,areas}});
    if(path==="/admin-api/v1/management/hub/status")return route.fulfill({json:systemd(true)});
    if(path==="/admin-api/v1/management/hub/restart"){
      calls++;
      return route.fulfill({status:503,json:{error:"outcome_unknown"}});
    }
    return route.fallback();
  });
  await page.goto("/#adminCore");
  await page.getByLabel("Active account ID").fill("1");
  await page.getByLabel("Account password").fill("Valid proof");
  await page.getByRole("button",{name:"Prove access",exact:true}).click();
  await expect(page.getByTestId("admin-access-result")).toContainText("Access proved");
  await page.evaluate(()=>{location.hash="#management";});
  await expect(page.getByTestId("hub-restart")).toBeEnabled();
  await page.getByTestId("hub-restart").click();
  await page.getByText("I understand the impact").click();
  await page.getByRole("button",{name:"Confirm operation"}).click();
  await expect(page.getByTestId("hub-service-notice")).toContainText("unknown");
  expect(calls).toBe(1);
  await expect(page.getByTestId("hub-restart")).toBeDisabled();
});

test("a catalog without hub.manage never renders service controls",async({page})=>{
  await page.route("**/admin-api/v1/status",route=>route.fulfill({json:snapshot}));
  await page.route("**/admin-api/v1/management",route=>route.fulfill({json:{schema:2,maintenance:true,restart_required:true,
    areas:areas.map(area=>area.key==="hub"?{...area,allowed:false}:area)}}));
  await page.goto("/#management");
  await expect(page.getByTestId("management-hub")).toContainText("Not allowed");
  await expect(page.getByTestId("hub-service")).toHaveCount(0);
});
