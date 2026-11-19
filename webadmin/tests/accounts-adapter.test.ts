import {ACCESS_ROUTES} from "../src/lib/access.ts";
import {PERMISSION_KEYS} from "../src/lib/roles.ts";
import {administration} from "./admin-fixture.ts";
import assert from "node:assert/strict";
import {test} from "node:test";
import {createServer,request} from "node:http";
import {mkdtempSync,rmSync} from "node:fs";
import {tmpdir} from "node:os";
import {join} from "node:path";
import {once} from "node:events";
import type {AddressInfo} from "node:net";
import {middleware} from "../server/local-api.ts";
test("account adapter requires same-origin forms and redacts service credentials",async()=>{
 const dir=mkdtempSync(join(tmpdir(),"dc24h-accounts-adapter-")),socket=join(dir,"api.sock"),token="b".repeat(64);
 const ban={id:"7",kind:"ip",subject:"192.0.2.1",reason:"Policy",created_at:1,expires_at:null,created_by:"1",revoked_at:null,revoked_by:null,revision:"1"};
 const account={id:"1",name:"Test",role:"registered",status:"disabled",revision:"1",created_at:1,updated_at:1,last_login_at:null};
 let writes=0, observed="", authorization="", staffMode=true, backendPath="", grantHeader="", sessionHeader="", csrfHeader="";
 const sessionId="ab".repeat(16), csrfToken="ef".repeat(32);
 const bound={"X-DC24H-Session":sessionId,"X-DC24H-Csrf":csrfToken};
 const accessBody={maintenance:true,restart_required:true,expires_in:600,actor:{id:"1",role:"administrator",status:"active"},grant:"cd".repeat(32),session:sessionId,csrf:csrfToken,permissions:PERMISSION_KEYS.map(key=>({key,granted:key!=="hub.manage",implemented:key!=="configuration.manage"&&key!=="hub.manage"})),routes:ACCESS_ROUTES.map(([method,path,permission])=>({method,path,permission,allowed:true})),token};
 const managementBody={schema:1,maintenance:true,restart_required:true,areas:[{key:"accounts",permission:"accounts.manage",allowed:true,proof:"MgmtSecret"},{key:"roles",permission:"roles.manage",allowed:false},{key:"bans",permission:"users.ban+blocks.manage",allowed:true}],token};
 const backend=createServer((req,res)=>{authorization=String(req.headers.authorization);grantHeader=String(req.headers["x-dc24h-admin-grant"]??"");sessionHeader=String(req.headers["x-dc24h-session"]??"");csrfHeader=String(req.headers["x-dc24h-csrf"]??"");backendPath=req.url??"";let data="";req.on("data",c=>data+=c);req.on("end",()=>{
  if(req.method==="POST"){writes++;observed=data;}
  if(data.includes("deny-grant")){res.statusCode=401;res.setHeader("Content-Type","application/json");res.end(JSON.stringify({error:"admin_grant_required",token}));return;}
  if(data.includes("need-csrf")){res.statusCode=403;res.setHeader("Content-Type","application/json");res.end(JSON.stringify({error:"csrf_required",token}));return;}
  const audit={maintenance:true,restart_required:true,events:[{id:"4",occurred_at:10,action:"accounts.create",actor_id:"1",target_kind:"account",target_id:"2",outcome:"succeeded",proof:"hidden-proof"}],next_after:null,token};
  res.setHeader("Content-Type","application/json");res.end(JSON.stringify(req.url==="/api/v1/management"?managementBody:req.url==="/api/v1/admin/access"?accessBody:req.url==="/api/v1/audit"?audit:req.url?.startsWith("/api/v1/bans")?(req.method==="GET"?{bans:[ban],next_after:null,maintenance:true,restart_required:true,observed_at:100,token}:{ban:{...ban,proof:"private"},maintenance:true,restart_required:true,observed_at:100,token}):req.url==="/api/v1/admin"?{...administration,token}:req.method==="POST"?{account:{...account,verifier:"private"},maintenance:true,token}:{accounts:[staffMode&&req.url?.includes("staff_after")?{...account,role:"owner"}:account],staff_only:staffMode&&req.url?.includes("staff_after"),next_after:null,maintenance:true,token}));
 });});backend.listen(socket);await once(backend,"listening");
 const handler=middleware({socket,token}),adapter=createServer((req,res)=>handler(req,res,()=>res.end()));adapter.listen(0,"127.0.0.1");await once(adapter,"listening");
 const port=(adapter.address() as AddressInfo).port,host=`127.0.0.1:${port}`;
 const call=(method:string,headers:Record<string,string>={},body="",path="/admin-api/v1/accounts")=>new Promise<{code:number;text:string}>((resolve,reject)=>{
  const req=request({hostname:"127.0.0.1",port,path,method,headers:{Host:host,...headers}},res=>{let text="";res.on("data",c=>text+=c);res.on("end",()=>resolve({code:res.statusCode!,text}));});req.on("error",reject);req.end(body);
 });
 try {
  assert.equal((await call("GET")).code,200);
  const core=await call("GET",{},"","/admin-api/v1/admin");
  assert.equal(core.code,200);assert.deepEqual(JSON.parse(core.text),administration);assert.ok(!core.text.includes(token));
  assert.equal((await call("POST",{},"","/admin-api/v1/admin")).code,404);
  const staff=await call("GET",{},"","/admin-api/v1/accounts?staff_after=0");
  assert.equal(staff.code,200);assert.equal(JSON.parse(staff.text).staff_only,true);assert.ok(!staff.text.includes(token));
  staffMode=false;
  assert.equal((await call("GET",{},"","/admin-api/v1/accounts?staff_after=0")).code,503);
  staffMode=true;
  const body="actor_id=1&proof=account+proof&name=Test&password=private+password";
  const headers={"Content-Type":"application/x-www-form-urlencoded","Content-Length":String(Buffer.byteLength(body)),Origin:`http://${host}`,"X-DC24H-Admin":"1",Authorization:"Bearer browser-token"};
  assert.equal((await call("POST",{...headers,Origin:"https://other.example"},body)).code,403);
  assert.equal((await call("POST",{...headers,"X-DC24H-Admin":""},body)).code,403);assert.equal(writes,0);
  const unbound=await call("POST",headers,body);assert.equal(unbound.code,403);assert.equal(JSON.parse(unbound.text).error,"csrf_required");assert.equal(writes,0);
  const malformed=await call("POST",{...headers,"X-DC24H-Session":"CD".repeat(16),"X-DC24H-Csrf":csrfToken},body);assert.equal(malformed.code,403);assert.equal(writes,0);
  const result=await call("POST",{...headers,...bound},body);assert.equal(result.code,200);assert.deepEqual(JSON.parse(result.text),{account,maintenance:true});
  assert.equal(observed,body);assert.equal(authorization,`Bearer ${token}`);assert.equal(sessionHeader,sessionId);assert.equal(csrfHeader,csrfToken);assert.equal(writes,1);assert.ok(!result.text.includes("private")&&!result.text.includes(token));
  const listedBans=await call("GET",{},"","/admin-api/v1/bans?after=0");assert.equal(listedBans.code,200);assert.equal(JSON.parse(listedBans.text).bans[0].id,"7");assert.ok(!listedBans.text.includes(token));
  const filtered=await call("GET",{},"","/admin-api/v1/bans?after=0&state=temporary");assert.equal(filtered.code,200);
  assert.equal(backendPath,"/api/v1/bans?after=0&state=temporary");assert.equal(JSON.parse(filtered.text).observed_at,100);
  assert.equal((await call("GET",{},"","/admin-api/v1/bans?after=0&state=invalid")).code,404);
  const banBody="actor_id=1&proof=account+proof&kind=ip&subject=192.0.2.1&reason=Policy&duration_seconds=3600";
  const createdBan=await call("POST",{...headers,...bound,"Content-Length":String(Buffer.byteLength(banBody))},banBody,"/admin-api/v1/bans");
  assert.equal(observed,banBody);assert.equal(createdBan.code,200);assert.deepEqual(JSON.parse(createdBan.text),{ban,maintenance:true,restart_required:true,observed_at:100});
  assert.equal((await call("GET",{},"","/admin-api/v1/audit")).code,404);
  const auditBody="actor_id=1&proof=secret+proof&after=0";
  const listed=await call("POST",{...headers,...bound,"Content-Length":String(Buffer.byteLength(auditBody))},auditBody,"/admin-api/v1/audit");
  assert.equal(listed.code,200);assert.equal(JSON.parse(listed.text).events[0].action,"accounts.create");
  assert.equal(observed,auditBody);assert.ok(!listed.text.includes("hidden-proof")&&!listed.text.includes("secret")&&!listed.text.includes(token));
  const proved=await call("GET",{"X-DC24H-Admin-Grant":"AB".repeat(32)},"","/admin-api/v1/accounts");
  assert.equal(proved.code,200);assert.equal(grantHeader,"ab".repeat(32));assert.equal(sessionHeader,"");assert.equal(csrfHeader,"");
  assert.equal((await call("GET",{"X-DC24H-Admin-Grant":"zz"},"","/admin-api/v1/accounts")).code,200);assert.equal(grantHeader,"");
  const accessForm="actor_id=1&proof=account+proof";
  const provedAccess=await call("POST",{...headers,"Content-Length":String(Buffer.byteLength(accessForm)),"X-DC24H-Admin-Grant":"CD".repeat(32)},accessForm,"/admin-api/v1/admin/access");
  assert.equal(provedAccess.code,200);assert.equal(JSON.parse(provedAccess.text).grant,"cd".repeat(32));assert.equal(JSON.parse(provedAccess.text).session,sessionId);assert.ok(!provedAccess.text.includes(token));assert.equal(grantHeader,"cd".repeat(32));assert.equal(csrfHeader,"");
  const listedManagement=await call("GET",{"X-DC24H-Admin-Grant":"AB".repeat(32)},"","/admin-api/v1/management");
  assert.equal(listedManagement.code,200);assert.equal(backendPath,"/api/v1/management");assert.equal(grantHeader,"ab".repeat(32));
  assert.deepEqual(JSON.parse(listedManagement.text).areas.map((area:{key:string;allowed:boolean})=>[area.key,area.allowed]),[["accounts",true],["roles",false],["bans",true]]);
  assert.ok(!listedManagement.text.includes("MgmtSecret")&&!listedManagement.text.includes(token));
  assert.equal((await call("POST",{},"","/admin-api/v1/management")).code,404);
  const denied=await call("POST",{...headers,...bound,"Content-Length":String(Buffer.byteLength("deny-grant=1"))},"deny-grant=1","/admin-api/v1/accounts");
  assert.equal(denied.code,401);assert.equal(JSON.parse(denied.text).error,"admin_grant_required");assert.ok(!denied.text.includes(token));
  const passed=await call("POST",{...headers,...bound,"Content-Length":String(Buffer.byteLength("need-csrf=1"))},"need-csrf=1","/admin-api/v1/accounts");
  assert.equal(passed.code,403);assert.equal(JSON.parse(passed.text).error,"csrf_required");assert.ok(!passed.text.includes(token));
 } finally {adapter.close();backend.close();adapter.closeAllConnections();backend.closeAllConnections();await Promise.all([once(adapter,"close"),once(backend,"close")]);rmSync(dir,{recursive:true,force:true});}
});
