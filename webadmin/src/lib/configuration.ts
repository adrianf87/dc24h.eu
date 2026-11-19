import {parseModeration} from "./moderation.ts";
import type {ModerationRule} from "./moderation.ts";
import {parseRoles} from "./roles.ts";
import type {RolesSnapshot} from "./roles.ts";
export const DATABASE_LIMITS = ["timeout_seconds", "connect_timeout_seconds", "read_timeout_seconds", "write_timeout_seconds"] as const;
export const AUTHENTICATION_LIMITS = ["max_accounts", "attempts_per_ip", "attempts_per_account", "attempts_per_second", "attempt_window_seconds", "max_tracked_attempt_ips", "max_tracked_attempt_accounts", "login_timeout_seconds", "tls_handshake_timeout_seconds", "output_timeout_seconds", "close_timeout_seconds", "max_pending_logins", "max_pending_per_ip", "max_login_bytes", "max_login_frames"] as const;
export interface StartupConfiguration {
  mode: "hub" | "maintenance";
  roles?: RolesSnapshot;
  moderation?: ModerationRule[];
  restart_required: true;
  database: {enabled: boolean; transport:"unix"} & Record<typeof DATABASE_LIMITS[number], number>;
  authentication: {enabled: boolean; require_tls:boolean} & Record<typeof AUTHENTICATION_LIMITS[number], number>;
}
const record=(value:unknown):Record<string,unknown>=>{
  if(!value||typeof value!=="object"||Array.isArray(value))throw new Error("Invalid configuration");return value as Record<string,unknown>;
};
const boolean=(value:unknown):boolean=>{if(typeof value!=="boolean")throw new Error("Invalid configuration switch");return value;};
const integer=(value:unknown,max:number):number=>{
  if(typeof value!=="number"||!Number.isSafeInteger(value)||value<1||value>max)throw new Error("Invalid configuration limit");return value;
};
export function parseConfiguration(value:unknown):StartupConfiguration {
  const root=record(value),db=record(root.database),auth=record(root.authentication);
  if((root.mode!=="hub"&&root.mode!=="maintenance")||root.restart_required!==true||db.transport!=="unix")throw new Error("Invalid configuration mode");
  const database={enabled:boolean(db.enabled),transport:"unix" as const,...Object.fromEntries(DATABASE_LIMITS.map(key=>[key,integer(db[key],60)]))} as StartupConfiguration["database"];
  const maxima:Record<typeof AUTHENTICATION_LIMITS[number],number>={max_accounts:4096,attempts_per_ip:100000,attempts_per_account:100000,attempts_per_second:100000,attempt_window_seconds:3600,max_tracked_attempt_ips:65536,max_tracked_attempt_accounts:65536,login_timeout_seconds:3600,tls_handshake_timeout_seconds:3600,output_timeout_seconds:3600,close_timeout_seconds:3600,max_pending_logins:65535,max_pending_per_ip:65535,max_login_bytes:16777216,max_login_frames:65535};
  const authentication={enabled:boolean(auth.enabled),require_tls:boolean(auth.require_tls),...Object.fromEntries(AUTHENTICATION_LIMITS.map(key=>[key,integer(auth[key],maxima[key])]))} as StartupConfiguration["authentication"];
  if(authentication.enabled&&(!database.enabled||root.mode==="maintenance"))throw new Error("Incompatible configuration");
  return {mode:root.mode,restart_required:true,database,authentication,...(root.roles===undefined?{}:{roles:parseRoles(root.roles)}),...(root.moderation===undefined?{}:{moderation:parseModeration(root.moderation)})};
}
