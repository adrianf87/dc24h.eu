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
