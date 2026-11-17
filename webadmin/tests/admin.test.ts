import assert from "node:assert/strict";
import {test} from "node:test";
import {parseAdmin} from "../src/lib/admin.ts";
import {administration} from "./admin-fixture.ts";
test("administration catalog is exact, local ownership is isolated and metadata strips secrets",()=>{
 assert.deepEqual(parseAdmin({...administration,token:"private",statistics:{...administration.statistics,proof:"private"}}),administration);
 for(const patch of [{maintenance:false},{actions:administration.actions.slice(1)},{actions:[...administration.actions].reverse()},
  {actions:administration.actions.map(a=>({...a,available:true}))},{statistics:{...administration.statistics,attempts:9}},
  {statistics:{...administration.statistics,attempts:"01"}},{statistics:{...administration.statistics,attempts:"18446744073709551616"}}])assert.throws(()=>parseAdmin({...administration,...patch}));
 assert.equal(parseAdmin({...administration,statistics:{...administration.statistics,attempts:"18446744073709551615"}}).statistics.attempts,"18446744073709551615");
});
