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
 const account={id:"1",name:"Test",role:"registered",status:"disabled",revision:"1",created_at:1,updated_at:1,last_login_at:null};
 let writes=0, observed="", authorization="", staffMode=true;
 const backend=createServer((req,res)=>{authorization=String(req.headers.authorization);let data="";req.on("data",c=>data+=c);req.on("end",()=>{
  if(req.method==="POST"){writes++;observed=data;}
  res.setHeader("Content-Type","application/json");res.end(JSON.stringify(req.method==="POST"?{account:{...account,verifier:"private"},maintenance:true,token}:{accounts:[staffMode&&req.url?.includes("staff_after")?{...account,role:"owner"}:account],staff_only:staffMode&&req.url?.includes("staff_after"),next_after:null,maintenance:true,token}));
 });});backend.listen(socket);await once(backend,"listening");
 const handler=middleware({socket,token}),adapter=createServer((req,res)=>handler(req,res,()=>res.end()));adapter.listen(0,"127.0.0.1");await once(adapter,"listening");
 const port=(adapter.address() as AddressInfo).port,host=`127.0.0.1:${port}`;
 const call=(method:string,headers:Record<string,string>={},body="",path="/admin-api/v1/accounts")=>new Promise<{code:number;text:string}>((resolve,reject)=>{
  const req=request({hostname:"127.0.0.1",port,path,method,headers:{Host:host,...headers}},res=>{let text="";res.on("data",c=>text+=c);res.on("end",()=>resolve({code:res.statusCode!,text}));});req.on("error",reject);req.end(body);
 });
 try {
  assert.equal((await call("GET")).code,200);
  const staff=await call("GET",{},"","/admin-api/v1/accounts?staff_after=0");
  assert.equal(staff.code,200);assert.equal(JSON.parse(staff.text).staff_only,true);assert.ok(!staff.text.includes(token));
  staffMode=false;
  assert.equal((await call("GET",{},"","/admin-api/v1/accounts?staff_after=0")).code,503);
  staffMode=true;
  const body="actor_id=1&proof=account+proof&name=Test&password=private+password";
  const headers={"Content-Type":"application/x-www-form-urlencoded","Content-Length":String(Buffer.byteLength(body)),Origin:`http://${host}`,"X-DC24H-Admin":"1",Authorization:"Bearer browser-token"};
  assert.equal((await call("POST",{...headers,Origin:"https://other.example"},body)).code,403);
  assert.equal((await call("POST",{...headers,"X-DC24H-Admin":""},body)).code,403);assert.equal(writes,0);
  const result=await call("POST",headers,body);assert.equal(result.code,200);assert.deepEqual(JSON.parse(result.text),{account,maintenance:true});
  assert.equal(observed,body);assert.equal(authorization,`Bearer ${token}`);assert.equal(writes,1);assert.ok(!result.text.includes("private")&&!result.text.includes(token));
 } finally {adapter.close();backend.close();adapter.closeAllConnections();backend.closeAllConnections();await Promise.all([once(adapter,"close"),once(backend,"close")]);rmSync(dir,{recursive:true,force:true});}
});
