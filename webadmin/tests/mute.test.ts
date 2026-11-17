import assert from "node:assert/strict";
import {test} from "node:test";
import {parseMute} from "../src/lib/mute.ts";
import {parseSnapshot} from "../src/lib/status.ts";
import {snapshot} from "./fixtures.ts";
const mute={attempts:"4",applied:"2",cleared:"1",refused:"1",errors:"0",notice_failures:"0",public_blocked:"3",private_blocked:"5"};
test("mute counters preserve uint64 precision, strip private metadata and support older APIs",()=>{
 assert.deepEqual(parseMute({...mute,nickname:"private",scope:"all",actor:"secret",message:"secret"}),mute);
 assert.equal(parseMute({...mute,public_blocked:"18446744073709551615"}).public_blocked,"18446744073709551615");
 for(const patch of [{attempts:4},{applied:"01"},{cleared:"-1"},{public_blocked:"18446744073709551616"},{private_blocked:undefined}])assert.throws(()=>parseMute({...mute,...patch}));
 assert.deepEqual(parseSnapshot({...snapshot,mute}).mute,mute);assert.equal(parseSnapshot(snapshot).mute,undefined);
 assert.throws(()=>parseSnapshot({...snapshot,mute:{...mute,errors:null}}));
});
