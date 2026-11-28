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
 let writes=0, observed="", authorization="", staffMode=true, backendPath="", grantHeader="", sessionHeader="", csrfHeader="", managementMime="application/json";
 const sessionId="ab".repeat(16), csrfToken="ef".repeat(32);
 const bound={"X-DC24H-Session":sessionId,"X-DC24H-Csrf":csrfToken};
 const accessBody={maintenance:true,restart_required:true,expires_in:600,actor:{id:"1",role:"administrator",status:"active"},grant:"cd".repeat(32),session:sessionId,csrf:csrfToken,permissions:PERMISSION_KEYS.map(key=>({key,granted:key!=="hub.manage",implemented:key!=="configuration.manage"})),routes:ACCESS_ROUTES.map(([method,path,permission])=>({method,path,permission,allowed:true})),token};
 const managementBody={schema:2,maintenance:true,restart_required:true,areas:[{key:"accounts",permission:"accounts.manage",allowed:true,implemented:true,proof:"MgmtSecret"},{key:"roles",permission:"roles.manage",allowed:false,implemented:true},{key:"bans",permission:"users.ban+blocks.manage",allowed:true,implemented:true},{key:"configuration",permission:"configuration.manage",allowed:true,implemented:false},{key:"hub",permission:"hub.manage",allowed:false,implemented:true}],token};
 const configRevision="a".repeat(64),configSaved={network_max_connections:"128",network_idle_timeout:"120",logging_level:"info",general_language:"us"};
 const configReview={schema:1,maintenance:true,read_only:true,restart_required:true,active_scope:"maintenance_process",hub_active_available:false,
  revision:configRevision,matches_active:true,saved:configSaved,active:configSaved,token};
 const backend=createServer((req,res)=>{authorization=String(req.headers.authorization);grantHeader=String(req.headers["x-dc24h-admin-grant"]??"");sessionHeader=String(req.headers["x-dc24h-session"]??"");csrfHeader=String(req.headers["x-dc24h-csrf"]??"");backendPath=req.url??"";let data="";req.on("data",c=>data+=c);req.on("end",()=>{
  if(req.method==="POST"){writes++;observed=data;}
  if(req.url==="/api/v1/management/authorize"){res.statusCode=501;res.setHeader("Content-Type","application/json");res.end(JSON.stringify({error:"not_implemented",token,proof:"hidden"}));return;}
  if(data.includes("deny-grant")){res.statusCode=401;res.setHeader("Content-Type","application/json");res.end(JSON.stringify({error:"admin_grant_required",token}));return;}
  if(data.includes("need-csrf")){res.statusCode=403;res.setHeader("Content-Type","application/json");res.end(JSON.stringify({error:"csrf_required",token}));return;}
  if(req.url?.startsWith("/api/v1/management/configuration")){
   if(data.includes("network_max_connections=0")){res.statusCode=422;res.setHeader("Content-Type","application/json");res.end(JSON.stringify({error:"invalid_configuration",token}));return;}
   if(data.includes("revision="+"0".repeat(64))){res.statusCode=409;res.setHeader("Content-Type","application/json");res.end(JSON.stringify({error:"stale",token}));return;}
   const result=req.url.endsWith("/write")||req.url.endsWith("/restore")?{written:true,restart_required:true,revision:"d".repeat(64),backup_available:true,token}:
    req.method==="POST"?{valid:true,written:false,restart_required:true,token}:configReview;
   res.setHeader("Content-Type","application/json");res.end(JSON.stringify(result));return;
  }
  if(req.url==="/api/v1/management/hub/status"){
   res.setHeader("Content-Type","application/json");res.end(JSON.stringify({schema:1,available:true,unit:"dc24h.service",load_state:"loaded",active_state:"active",sub_state:"running",result:"success",token}));return;
  }
  if(req.url==="/api/v1/management/hub/stop"){
   res.setHeader("Content-Type","application/json");res.end(JSON.stringify({operation:"stop",outcome:"succeeded",active_state:"inactive",token}));return;
  }
  if(req.url==="/api/v1/management" && managementMime!=="application/json"){
   res.setHeader("Content-Type",managementMime);res.end(JSON.stringify(managementBody));return;
  }
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
  assert.equal(proved.code,403);assert.equal(JSON.parse(proved.text).error,"csrf_required");assert.ok(!proved.text.includes(token));
  const boundRead=await call("GET",{"X-DC24H-Admin-Grant":"AB".repeat(32),...bound},"","/admin-api/v1/accounts");
  assert.equal(boundRead.code,200);assert.equal(grantHeader,"ab".repeat(32));assert.equal(sessionHeader,sessionId);assert.equal(csrfHeader,csrfToken);
  assert.equal((await call("GET",{"X-DC24H-Admin-Grant":"zz"},"","/admin-api/v1/accounts")).code,200);assert.equal(grantHeader,"");
  const accessForm="actor_id=1&proof=account+proof";
  const provedAccess=await call("POST",{...headers,"Content-Length":String(Buffer.byteLength(accessForm)),"X-DC24H-Admin-Grant":"CD".repeat(32)},accessForm,"/admin-api/v1/admin/access");
  assert.equal(provedAccess.code,200);assert.equal(JSON.parse(provedAccess.text).grant,"cd".repeat(32));assert.equal(JSON.parse(provedAccess.text).session,sessionId);assert.ok(!provedAccess.text.includes(token));assert.equal(grantHeader,"cd".repeat(32));assert.equal(csrfHeader,"");
  const unboundManagement=await call("GET",{"X-DC24H-Admin-Grant":"AB".repeat(32)},"","/admin-api/v1/management");
  assert.equal(unboundManagement.code,403);assert.equal(JSON.parse(unboundManagement.text).error,"csrf_required");
  const listedManagement=await call("GET",{"X-DC24H-Admin-Grant":"AB".repeat(32),...bound},"","/admin-api/v1/management");
  assert.equal(listedManagement.code,200);assert.equal(backendPath,"/api/v1/management");assert.equal(grantHeader,"ab".repeat(32));assert.equal(sessionHeader,sessionId);assert.equal(csrfHeader,csrfToken);
  assert.deepEqual(JSON.parse(listedManagement.text).areas.map((area:{key:string;allowed:boolean;implemented:boolean})=>[area.key,area.allowed,area.implemented]),[["accounts",true,true],["roles",false,true],["bans",true,true],["configuration",true,false],["hub",false,true]]);
  assert.ok(!listedManagement.text.includes("MgmtSecret")&&!listedManagement.text.includes(token));
  const mimeHeaders={"X-DC24H-Admin-Grant":"AB".repeat(32),...bound};
  managementMime="application/json-attacker";
  const refusedMime=await call("GET",mimeHeaders,"","/admin-api/v1/management");
  assert.equal(refusedMime.code,503);assert.equal(JSON.parse(refusedMime.text).error,"invalid_api_response");
  managementMime="application/json; charset=utf-8";
  assert.equal((await call("GET",mimeHeaders,"","/admin-api/v1/management")).code,200);
  managementMime="application/json";
  const configPath="/admin-api/v1/management/configuration",configHeaders={"X-DC24H-Admin-Grant":"AB".repeat(32),...bound};
  assert.equal((await call("GET",{},"",configPath)).code,403);
  assert.equal((await call("GET",{"X-DC24H-Admin-Grant":"AB".repeat(32)},"",configPath)).code,403);
  const configRead=await call("GET",configHeaders,"",configPath);
  assert.equal(configRead.code,200);assert.equal(JSON.parse(configRead.text).revision,configRevision);
  assert.ok(!configRead.text.includes(token));assert.equal(backendPath,"/api/v1/management/configuration");
  const configForm="revision="+configRevision+"&network_max_connections=256";
  const configPost={...headers,...configHeaders,"Content-Length":String(Buffer.byteLength(configForm))};
  assert.equal((await call("POST",{...configPost,Origin:"https://evil.example"},configForm,configPath)).code,403);
  assert.equal((await call("POST",configPost,configForm,configPath)).code,200);
  assert.equal(observed,configForm);assert.equal(grantHeader,"ab".repeat(32));
  const configWrite=await call("POST",configPost,configForm,configPath+"/write");
  assert.equal(configWrite.code,200);assert.equal(JSON.parse(configWrite.text).backup_available,true);
  assert.ok(!configWrite.text.includes(token));assert.equal(backendPath,"/api/v1/management/configuration/write");
  const restoreForm="revision="+configRevision;
  const restore=await call("POST",{...configPost,"Content-Length":String(restoreForm.length)},restoreForm,configPath+"/restore");
  assert.equal(restore.code,200);assert.equal(backendPath,"/api/v1/management/configuration/restore");
  const invalidForm=configForm.replace("256","0");
  const invalid=await call("POST",{...configPost,"Content-Length":String(invalidForm.length)},invalidForm,configPath+"/write");
  assert.equal(invalid.code,422);assert.deepEqual(JSON.parse(invalid.text),{error:"invalid_configuration"});
  const staleForm="revision="+"0".repeat(64)+"&network_max_connections=256";
  const stale=await call("POST",{...configPost,"Content-Length":String(staleForm.length)},staleForm,configPath+"/write");
  assert.equal(stale.code,409);assert.deepEqual(JSON.parse(stale.text),{error:"stale"});
  assert.equal((await call("GET",configHeaders,"",configPath+"/write")).code,404);
  assert.equal((await call("POST",configPost,configForm,configPath+"/write/extra")).code,405);
  const servicePath="/admin-api/v1/management/hub/";
  assert.equal((await call("GET",{},"",servicePath+"status")).code,403);
  assert.equal((await call("GET",{"X-DC24H-Admin-Grant":"AB".repeat(32)},"",servicePath+"status")).code,403);
  const serviceRead=await call("GET",configHeaders,"",servicePath+"status");
  assert.equal(serviceRead.code,200);assert.equal(JSON.parse(serviceRead.text).active_state,"active");
  assert.ok(!serviceRead.text.includes(token));assert.equal(backendPath,"/api/v1/management/hub/status");
  const serviceForm="confirm=STOP",serviceHeaders={...configPost,"Content-Length":String(serviceForm.length)};
  assert.equal((await call("POST",{...serviceHeaders,Origin:"https://evil.example"},serviceForm,servicePath+"stop")).code,403);
  const stopped=await call("POST",serviceHeaders,serviceForm,servicePath+"stop");
  assert.equal(stopped.code,200);assert.deepEqual(JSON.parse(stopped.text),{operation:"stop",outcome:"succeeded",active_state:"inactive"});
  assert.equal(observed,serviceForm);assert.ok(!stopped.text.includes(token));
  assert.equal((await call("GET",configHeaders,"",servicePath+"stop")).code,404);
  assert.equal((await call("POST",serviceHeaders,serviceForm,servicePath+"status")).code,404);
  assert.equal((await call("GET",configHeaders,"",servicePath+"status?unit=other.service")).code,404);
  assert.equal((await call("POST",{},"","/admin-api/v1/management")).code,404);
  const authorizeBody="operation=configuration.read";
  const authorized=await call("POST",{...headers,...bound,"Content-Length":String(Buffer.byteLength(authorizeBody))},authorizeBody,"/admin-api/v1/management/authorize");
  assert.equal(authorized.code,501);assert.equal(JSON.parse(authorized.text).error,"not_implemented");assert.equal(backendPath,"/api/v1/management/authorize");assert.equal(observed,authorizeBody);
  assert.ok(!authorized.text.includes(token)&&!authorized.text.includes("hidden"));
  const beforeExtra=writes;
  assert.equal((await call("POST",{...headers,...bound,"Content-Length":String(Buffer.byteLength(authorizeBody))},authorizeBody,"/admin-api/v1/management/authorize/extra")).code,405);
  assert.equal(writes,beforeExtra);
  const denied=await call("POST",{...headers,...bound,"Content-Length":String(Buffer.byteLength("deny-grant=1"))},"deny-grant=1","/admin-api/v1/accounts");
  assert.equal(denied.code,401);assert.equal(JSON.parse(denied.text).error,"admin_grant_required");assert.ok(!denied.text.includes(token));
  const passed=await call("POST",{...headers,...bound,"Content-Length":String(Buffer.byteLength("need-csrf=1"))},"need-csrf=1","/admin-api/v1/accounts");
  assert.equal(passed.code,403);assert.equal(JSON.parse(passed.text).error,"csrf_required");assert.ok(!passed.text.includes(token));
 } finally {adapter.close();backend.close();adapter.closeAllConnections();backend.closeAllConnections();await Promise.all([once(adapter,"close"),once(backend,"close")]);rmSync(dir,{recursive:true,force:true});}
});
