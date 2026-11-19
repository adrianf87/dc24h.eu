import { request } from "node:http";
import type { IncomingMessage, ServerResponse } from "node:http";
import {parseAudit} from "../src/lib/audit.ts";
import {parseBanResult,parseBans} from "../src/lib/bans.ts";
import {parseAdmin} from "../src/lib/admin.ts";
import { parseAccount, parseAccounts, parseStaff } from "../src/lib/accounts.ts";

export function accountMiddleware(config: {socket:string; token:string}|null) {
  let active = 0;
  return (req:IncomingMessage, res:ServerResponse) => {
    const reply = (status:number, data:unknown) => {
      if(res.destroyed || res.writableEnded) return;
      res.writeHead(status, {"Content-Type":"application/json", "Cache-Control":"no-store", "X-Content-Type-Options":"nosniff"});
      res.end(JSON.stringify(data));
    };
    const path=req.url ?? "";
    const read=req.method === "GET" && (path==="/admin-api/v1/admin" || /^\/admin-api\/v1\/bans(?:\?(?:after=(?:0|[1-9]\d{0,18})(?:&state=(?:all|active|permanent|temporary|expired|revoked))?|state=(?:all|active|permanent|temporary|expired|revoked)))?$/.test(path) || /^\/admin-api\/v1\/accounts(?:\?(?:after|staff_after)=(?:0|[1-9]\d{0,18})|\/[1-9]\d{0,18})?$/.test(path));
    const write=req.method === "POST" && (path === "/admin-api/v1/audit" || /^\/admin-api\/v1\/bans(?:\/[1-9]\d{0,18}\/revoke)?$/.test(path) || /^\/admin-api\/v1\/accounts(?:\/[1-9]\d{0,18}\/(?:activate|disable|reset|password|role))?$/.test(path));
    if(!read && !write){reply(404,{error:"not_found"});return;}
    if(req.headers["transfer-encoding"] || (read && req.headers["content-length"] && req.headers["content-length"] !== "0")) {reply(400,{error:"bad_request"});return;}
    if(write && (req.headers.origin !== `http://${req.headers.host}` || req.headers["x-dc24h-admin"] !== "1" ||
        req.headers["content-type"] !== "application/x-www-form-urlencoded" || !/^[1-9]\d{0,3}$/.test(String(req.headers["content-length"])) || Number(req.headers["content-length"]) > 7000)) {
      reply(403,{error:"forbidden"});return;
    }
    if(!config){reply(503,{error:"api_unconfigured"});return;}
    if(active >= 4){reply(429,{error:"api_busy"});return;}
    active++;
    let done=false;
    const chunks:Buffer[]=[]; let size=0;
    const upstream=request({socketPath:config.socket, path:path.replace("/admin-api/","/api/"), method:req.method, agent:false,
      headers:{Host:"localhost",Authorization:`Bearer ${config.token}`, Connection:"close",
        ...(write?{"Content-Type":"application/x-www-form-urlencoded", "Content-Length":req.headers["content-length"]}: {})}});
    const finish=()=>{if(!done){done=true;active--;clearTimeout(timer);for(const chunk of chunks)chunk.fill(0);}};
    const timer=setTimeout(()=>upstream.destroy(new Error("timeout")),15000);
    res.once("close",()=>{finish();upstream.destroy();});
    upstream.on("error",()=>{if(!done){finish();reply(503,{error:write?"outcome_unknown":"api_unavailable"});}});
    upstream.on("response", response=>{
      const received:Buffer[]=[];let bytes=0;
      response.on("data",(chunk:Buffer)=>{bytes+=chunk.length;if(bytes>16384){upstream.destroy();return;}received.push(chunk);});
      response.on("error",()=>upstream.destroy());
      response.on("end",()=>{
        if(done)return;
        try {
          if(!response.headers["content-type"]?.startsWith("application/json"))throw new Error();
          const data=JSON.parse(Buffer.concat(received).toString("utf8"));
          if(response.statusCode!==200){
            const status=[400,403,404,409,429,503].includes(response.statusCode ?? 0)?response.statusCode!:503;
            const error=["outcome_unknown","stale","rate_limited","unauthorized","management_refused","invalid_registration","invalid_input","account_unavailable","database_unavailable"].includes(data?.error)?data.error:"api_unavailable";
            finish();reply(status,{error});return;
          }
          const safe=path==="/admin-api/v1/audit"?parseAudit(data):path.startsWith("/admin-api/v1/bans")?(req.method==="GET"?parseBans(data):parseBanResult(data)):path==="/admin-api/v1/admin"?parseAdmin(data):data && "accounts" in data?(path.includes("?staff_after=")?parseStaff(data):parseAccounts(data)):data?.maintenance===true?{account:parseAccount(data.account),maintenance:true}:null;
          if(!safe)throw new Error();finish();reply(200,safe);
        }catch{finish();reply(503,{error:write?"outcome_unknown":"invalid_api_response"});}
      });
    });
    if(read){upstream.end();return;}
    req.on("data",(chunk:Buffer)=>{size+=chunk.length;if(size>7000){upstream.destroy();req.resume();return;}chunks.push(chunk);});
    req.on("aborted",()=>upstream.destroy());
    req.on("error",()=>upstream.destroy());
    req.on("end",()=>{if(done)return;if(size!==Number(req.headers["content-length"])){upstream.destroy();return;}const body=Buffer.concat(chunks);upstream.end(body,()=>body.fill(0));});
  };
}
