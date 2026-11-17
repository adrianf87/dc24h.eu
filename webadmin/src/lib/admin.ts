export const ADMIN_ACTIONS = ["accounts.create", "accounts.activate", "accounts.disable", "accounts.password", "accounts.reset", "accounts.role", "ownership.bootstrap", "ownership.transfer"] as const;
export const ADMIN_COUNTERS = ["attempts", "succeeded", "refused", "unavailable", "outcome_unknown"] as const;
export interface AdminSnapshot {
  maintenance: true;
  actions: {key: typeof ADMIN_ACTIONS[number]; available: boolean; local_owner_only: boolean}[];
  statistics: Record<typeof ADMIN_COUNTERS[number], string>;
}
export function parseAdmin(value: unknown): AdminSnapshot {
  if (!value || typeof value !== "object") throw new Error("Invalid administration");
  const data=value as Record<string,unknown>;
  if (data.maintenance!==true || !Array.isArray(data.actions) || data.actions.length!==ADMIN_ACTIONS.length || !data.statistics || typeof data.statistics!=="object") throw new Error("Invalid administration");
  const actions=data.actions.map((entry:unknown,i:number)=>{
    if(!entry || typeof entry!=="object") throw new Error("Invalid action");
    const a=entry as Record<string,unknown>,owner=i>=6;
    if(a.key!==ADMIN_ACTIONS[i] || a.available!==!owner || a.local_owner_only!==owner) throw new Error("Invalid action");
    return {key:ADMIN_ACTIONS[i], available:a.available as boolean, local_owner_only:owner};
  });
  const statistics={} as AdminSnapshot["statistics"];
  for(const key of ADMIN_COUNTERS){
    const n=(data.statistics as Record<string,unknown>)[key];
    if(typeof n!=="string" || !/^(0|[1-9]\d{0,19})$/.test(n) || BigInt(n)>18446744073709551615n) throw new Error("Invalid counter");
    statistics[key]=n;
  }
  return {maintenance:true, actions, statistics};
}
