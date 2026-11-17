import {test} from "node:test";
import assert from "node:assert/strict";
import {parseRoles,ACCOUNT_ROLES} from "../src/lib/roles.ts";
import {parseAccount} from "../src/lib/accounts.ts";
import {parseSnapshot} from "../src/lib/status.ts";
import {configuration} from "./configuration-fixture.ts";
import {snapshot} from "./fixtures.ts";
import {rolePolicy} from "./roles-fixture.ts";
test("role metadata is precise, strips extras and accepts six persisted roles",()=>{
 const policy=rolePolicy(["users.kick","audit.read"]);
 assert.deepEqual(parseRoles({...policy,key_file:"secret",catalog:policy.catalog.map(r=>({...r,verifier:"secret"}))}),policy);
 const base={id:"1",revision:"1",name:"Test",status:"disabled",created_at:0,updated_at:0,last_login_at:null};
 for(const role of ACCOUNT_ROLES)assert.equal(parseAccount({...base,role}).role,role);
 assert.throws(()=>parseAccount({...base,role:"guest"}));
 const data=parseSnapshot({...snapshot,configuration:{...configuration,roles:policy}});
 assert.deepEqual(data.configuration?.roles,policy);
});
test("unknown roles, unsafe VIP delegations, duplicates and malformed support fail closed",()=>{
 const policy=rolePolicy();
 for(const change of [
  {vip_permissions:["roles.manage"]},{vip_permissions:["*"]},{vip_permissions:["users.kick","users.kick"]},
  {catalog:policy.catalog.slice(1)},
  {catalog:policy.catalog.map((r,i)=>i===3?{...r,rank:0}:r)},
  {catalog:policy.catalog.map((r,i)=>i===3?{...r,key:"root"}:r)},
  {catalog:policy.catalog.map((r,i)=>i===3?{...r,permissions:["unknown.permission"]}:r)},
  {operations:policy.operations.map((p,i)=>i===0?{...p,implemented:1}:p)}
 ])assert.throws(()=>parseRoles({...policy,...change}));
});
