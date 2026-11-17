import assert from "node:assert/strict";
import {test} from "node:test";
import {parseConfiguration,DATABASE_LIMITS,AUTHENTICATION_LIMITS} from "../src/lib/configuration.ts";
import {parseSnapshot} from "../src/lib/status.ts";
import {snapshot} from "./fixtures.ts";
import {configuration} from "./configuration-fixture.ts";

test("configuration allowlist omits credentials and keeps old snapshots compatible",()=>{
 assert.equal(parseSnapshot(snapshot).configuration,undefined);
 const parsed=parseSnapshot({...snapshot,configuration:{...configuration,password:"never",database:{...configuration.database,user:"never",socket:"never",password_file:"never"},authentication:{...configuration.authentication,key_file:"never"}}});
 assert.deepEqual(parsed.configuration,configuration);assert.ok(!JSON.stringify(parsed).includes("never"));
 assert.throws(()=>parseSnapshot({...snapshot,configuration:{...configuration,database:{...configuration.database,enabled:true}}}));
});
test("configuration validates bounds, types, modes and incompatible authentication",()=>{
 for(const field of DATABASE_LIMITS)for(const value of [0,61,"5",1.5,undefined])assert.throws(()=>parseConfiguration({...configuration,database:{...configuration.database,[field]:value}}));
 for(const field of AUTHENTICATION_LIMITS)for(const value of [0,16777217,"1",1.5,undefined])assert.throws(()=>parseConfiguration({...configuration,authentication:{...configuration.authentication,[field]:value}}));
 for(const patch of [{mode:"untrusted"},{mode:["hub"]},{restart_required:false}])assert.throws(()=>parseConfiguration({...configuration,...patch}));
 assert.throws(()=>parseConfiguration({...configuration,authentication:{...configuration.authentication,enabled:true}}));
 assert.throws(()=>parseConfiguration({...configuration,mode:"maintenance",database:{...configuration.database,enabled:true},authentication:{...configuration.authentication,enabled:true}}));
 assert.throws(()=>parseConfiguration({...configuration,authentication:{...configuration.authentication,require_tls:"true"}}));
 assert.equal(parseConfiguration({...configuration,mode:"maintenance"}).mode,"maintenance");
});
