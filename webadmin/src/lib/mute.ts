export const MUTE_COUNTERS=["attempts","applied","cleared","refused","errors","notice_failures","public_blocked","private_blocked"] as const;
export type MuteSnapshot=Record<typeof MUTE_COUNTERS[number],string>;
export function parseMute(value:unknown):MuteSnapshot {
 if(!value||typeof value!=="object"||Array.isArray(value))throw new Error("Invalid mute statistics");
 const data=value as Record<string,unknown>,result={} as MuteSnapshot;
 for(const key of MUTE_COUNTERS){const n=data[key];if(typeof n!=="string"||!/^(0|[1-9]\d{0,19})$/.test(n)||BigInt(n)>18446744073709551615n)throw new Error("Invalid mute counter");result[key]=n;}
 return result;
}
