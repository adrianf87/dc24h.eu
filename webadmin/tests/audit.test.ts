import assert from "node:assert/strict";
import {test} from "node:test";
import {parseAudit, parseAuditEvent} from "../src/lib/audit.ts";
const event={id:"1",occurred_at:10,action:"accounts.create",actor_id:"2",target_kind:"account",target_id:"3",outcome:"succeeded"};
const page=(events:unknown[],next:string|null)=>({maintenance:true,restart_required:true,events,next_after:next});
test("audit pages keep fixed facts and drop secret fields",()=>{
 assert.deepEqual(parseAuditEvent({...event,proof:"secret",reason:"Private"}),event);
 assert.deepEqual(parseAudit(page([{...event,name:"Owner"}],null)),page([event],null));
 const full=Array.from({length:16},(_,index)=>({...event,id:String(index+1)}));
 assert.equal(parseAudit(page(full,"16")).next_after,"16");
});
test("malformed audit pages and unknown actions fail closed",()=>{
 for(const patch of [{id:"01"},{id:"9223372036854775808"},{occurred_at:-1},{occurred_at:253402300800},{action:"users.kick"},{target_kind:"ip"},{outcome:"refused"},{actor_id:2}]) {
  assert.throws(()=>parseAuditEvent({...event,...patch}));
 }
 assert.throws(()=>parseAudit(page([event,{...event,id:"1"}],null)));
 assert.throws(()=>parseAudit(page([event],"1")));
 assert.throws(()=>parseAudit(page(Array.from({length:16},(_,index)=>({...event,id:String(index+1)})),null)));
 assert.throws(()=>parseAudit({...page([event],null),maintenance:false}));
 assert.throws(()=>parseAudit({...page([event],null),restart_required:false}));
});
