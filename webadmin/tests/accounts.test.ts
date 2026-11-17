import assert from "node:assert/strict";
import {test} from "node:test";
import {parseAccount,parseAccounts} from "../src/lib/accounts.ts";
const record={id:"9223372036854775807",name:'Żółw"Test',role:"registered",status:"disabled",revision:"1",created_at:1,updated_at:2,last_login_at:null};
test("account metadata bounds and credential redaction",()=>{
 assert.deepEqual(parseAccount({...record,verifier:"secret"}),record);
 for(const patch of [{id:9},{id:"092"},{id:"9223372036854775808"},{name:"bad name"},{name:"a\n"},{name:"e\u0301"},{role:"root"},{role:["registered"]},{status:"enabled"},{status:["disabled"]},{revision:"0"},{updated_at:0},{last_login_at:3}]) assert.throws(()=>parseAccount({...record,...patch}));
 assert.deepEqual(parseAccounts({accounts:[record],maintenance:true,next_after:null,token:"secret"}),{accounts:[record],maintenance:true,next_after:null});
 for(const patch of [{accounts:[record,record]},{maintenance:false},{next_after:record.id}]) assert.throws(()=>parseAccounts({accounts:[record],maintenance:true,next_after:null,...patch}));
});

test("account browser responses enforce byte and content-type bounds",async()=>{
 const {readAccountResponse}=await import("../src/lib/accounts.ts");
 await assert.rejects(readAccountResponse(new Response("{}",{headers:{"Content-Type":"text/html"}})));
 await assert.rejects(readAccountResponse(new Response("x".repeat(16385),{headers:{"Content-Type":"application/json"}})));
 assert.deepEqual(await readAccountResponse(new Response("{}",{headers:{"Content-Type":"application/json"}})),{});
});
