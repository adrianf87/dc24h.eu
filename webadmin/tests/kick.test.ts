import assert from "node:assert/strict";
import {test} from "node:test";
import {parseKick} from "../src/lib/kick.ts";
import {parseSnapshot} from "../src/lib/status.ts";
import {snapshot} from "./fixtures.ts";
const kick={attempts:"4",succeeded:"1",refused:"2",errors:"1",notice_failures:"0"};
test("kick counters preserve uint64 precision, strip private fields and support older APIs",()=>{
 assert.deepEqual(parseKick({...kick,reason:"private",actor_id:"secret"}),kick);
 assert.equal(parseKick({...kick,attempts:"18446744073709551615"}).attempts,"18446744073709551615");
 for(const patch of [{attempts:4},{attempts:"01"},{attempts:"18446744073709551616"},{errors:undefined}])assert.throws(()=>parseKick({...kick,...patch}));
 assert.deepEqual(parseSnapshot({...snapshot,kick}).kick,kick);
 assert.equal(parseSnapshot(snapshot).kick,undefined);
});
