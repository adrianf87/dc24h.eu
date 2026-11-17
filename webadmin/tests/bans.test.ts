import assert from "node:assert/strict";
import {test} from "node:test";
import {parseBan,parseBans} from "../src/lib/bans.ts";
const ban={id:"9223372036854775807",kind:"ip",subject:"2001:db8::1",reason:"Policy review",created_at:1,expires_at:null,created_by:"1",revoked_at:null,revoked_by:null,revision:"1"};
test("ban metadata preserves IDs and rejects malformed subjects, times and secret fields",()=>{
 assert.deepEqual(parseBan({...ban,proof:"secret"}),ban);
 for(const patch of [{id:1},{kind:"host"},{subject:"evil.example"},{subject:"127.0.0.1/32"},{reason:"bad\nreason"},{reason:"e\u0301"},{expires_at:0},{revoked_by:"1"},{revoked_at:0}])assert.throws(()=>parseBan({...ban,...patch}));
 assert.equal(parseBan({...ban,kind:"account",subject:"7"}).subject,"7");
 assert.equal(parseBan({...ban,kind:"cid",subject:"A".repeat(39)}).kind,"cid");
 assert.throws(()=>parseBan({...ban,kind:"cid",subject:"B".repeat(39)}));
 assert.deepEqual(parseBans({maintenance:true,restart_required:true,bans:[ban],next_after:null,proof:"secret"}),{maintenance:true,restart_required:true,bans:[ban],next_after:null});
 assert.throws(()=>parseBans({maintenance:true,restart_required:false,bans:[],next_after:null}));
});

test("temporary lifecycle uses an exact boundary and revoke precedence",async()=>{
 const {banState,remainingSeconds,matchesFilter}=await import("../src/lib/bans.ts");
 const timed=parseBan({...ban,expires_at:20});
 assert.equal(banState(timed,19),"temporary");assert.equal(remainingSeconds(timed,19.25),1);
 assert.equal(banState(timed,20),"expired");assert.equal(remainingSeconds(timed,20),null);
 assert.equal(banState({...timed,revoked_at:10,revoked_by:"1"},30),"revoked");
 assert.equal(banState(parseBan(ban),30),"permanent");assert.equal(banState(timed,null),"unknown");
 assert.ok(matchesFilter(timed,"active",19));assert.ok(!matchesFilter(timed,"active",20));
 assert.ok(matchesFilter(timed,"expired",20));assert.ok(matchesFilter(timed,"all",null));
});
test("server observation survives normalization and ignores wall-clock skew",async()=>{
 const {serverTime,parseBanResult}=await import("../src/lib/bans.ts");
 assert.equal(serverTime({at:100,received:500},1500),101);
 assert.equal(serverTime({at:100,received:500},0),100);assert.equal(serverTime(null,1500),null);
 const envelope={maintenance:true,restart_required:true,ban,observed_at:100,proof:"secret"};
 assert.deepEqual(parseBanResult(envelope),{maintenance:true,restart_required:true,ban,observed_at:100});
 assert.equal(parseBans({maintenance:true,restart_required:true,bans:[ban],next_after:null,observed_at:100}).observed_at,100);
 for(const at of [null,-1,1.2,"100",Infinity,253402300800]){
  assert.throws(()=>parseBanResult({...envelope,observed_at:at}));
  assert.throws(()=>parseBans({maintenance:true,restart_required:true,bans:[],next_after:null,observed_at:at}));
 }
});
test("duration units enforce whole positive values and the 365-day ceiling",async()=>{
 const {durationSeconds}=await import("../src/lib/bans.ts");
 assert.equal(durationSeconds("1","seconds"),1);assert.equal(durationSeconds("5","minutes"),300);
 assert.equal(durationSeconds("24","hours"),86400);assert.equal(durationSeconds("365","days"),31536000);
 for(const [v,u] of [["0","seconds"],["01","hours"],["1.5","days"],["366","days"],["31536001","seconds"],["999999999999999999999","days"],["1","toString"],["1","__proto__"]])assert.throws(()=>durationSeconds(v,u));
});
