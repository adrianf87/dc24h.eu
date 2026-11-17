export const KICK_COUNTERS=["attempts","succeeded","refused","errors","notice_failures"] as const;
export type KickSnapshot=Record<typeof KICK_COUNTERS[number],string>;
export function parseKick(value:unknown):KickSnapshot {
 if(!value||typeof value!=="object"||Array.isArray(value))throw new Error("Invalid kick statistics");
 const data=value as Record<string,unknown>,result={} as KickSnapshot;
 for(const key of KICK_COUNTERS){const n=data[key];if(typeof n!=="string"||!/^(0|[1-9]\d{0,19})$/.test(n)||BigInt(n)>18446744073709551615n)throw new Error("Invalid kick counter");result[key]=n;}
 return result;
}
