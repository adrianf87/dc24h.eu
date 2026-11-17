export interface Ban {id:string;kind:"account"|"cid"|"ip";subject:string;reason:string;created_at:number;expires_at:number|null;created_by:string;revoked_at:number|null;revoked_by:string|null;revision:string}
const id=(v:unknown):v is string=>typeof v==="string"&&/^[1-9]\d{0,18}$/.test(v)&&BigInt(v)<=9223372036854775807n;
const time=(v:unknown):v is number=>typeof v==="number"&&Number.isSafeInteger(v)&&v>=0&&v<=253402300799;
export function parseBan(value:unknown):Ban{
 if(!value||typeof value!=="object")throw new Error("Invalid ban");const r=value as Record<string,unknown>;
 let subject=false;
 if(typeof r.subject==="string"){
  if(r.kind==="account")subject=id(r.subject);
  if(r.kind==="cid")subject=/^[A-Z2-7]{38}[AIQY]$/.test(r.subject);
  if(r.kind==="ip"&&r.subject.length<=45){
   if(r.subject.includes(":")){try{subject=/^[0-9a-f:.]+$/.test(r.subject)&&!!new URL(`http://[${r.subject}]/`).hostname;}catch{}}
   else subject=/^(0|[1-9]\d{0,2})(\.(0|[1-9]\d{0,2})){3}$/.test(r.subject)&&r.subject.split(".").every(p=>Number(p)<=255);
  }
 }
 if(!id(r.id)||!id(r.created_by)||!id(r.revision)||!subject||typeof r.reason!=="string"||!r.reason||new TextEncoder().encode(r.reason).length>256||/[\p{C}\p{Zl}\p{Zp}]/u.test(r.reason)||r.reason.normalize("NFC")!==r.reason||
  !time(r.created_at)||!(r.expires_at===null||(time(r.expires_at)&&r.expires_at>r.created_at))||
  !(r.revoked_at===null||(time(r.revoked_at)&&r.revoked_at>=r.created_at))||!(r.revoked_by===null||id(r.revoked_by))||((r.revoked_at===null)!==(r.revoked_by===null)))throw new Error("Invalid ban");
 return {id:r.id,kind:r.kind as Ban["kind"],subject:r.subject as string,reason:r.reason,created_at:r.created_at,expires_at:r.expires_at as number|null,created_by:r.created_by,revoked_at:r.revoked_at as number|null,revoked_by:r.revoked_by as string|null,revision:r.revision};
}
export function parseBans(value:unknown){
 if(!value||typeof value!=="object")throw new Error("Invalid bans");const r=value as Record<string,unknown>;
 if(r.maintenance!==true||r.restart_required!==true||!Array.isArray(r.bans)||r.bans.length>16||!(r.next_after===null||id(r.next_after)))throw new Error("Invalid bans");
 const bans=r.bans.map(parseBan);
 if(bans.some((b,i)=>i&&BigInt(b.id)<=BigInt(bans[i-1].id))||(r.next_after!==null&&(bans.length!==16||r.next_after!==bans.at(-1)?.id)))throw new Error("Invalid bans page");
 const observed_at=parseObservedAt(r.observed_at);
 return {maintenance:true as const,restart_required:true as const,bans,next_after:r.next_after as string|null,...(observed_at===undefined?{}:{observed_at})};
}

export type BanState="permanent"|"temporary"|"expired"|"revoked"|"unknown";
export const BAN_FILTERS=["all","active","permanent","temporary","expired","revoked"] as const;
export type BanFilter=typeof BAN_FILTERS[number];
export function banState(ban:Ban,now:number|null):BanState{
 if(ban.revoked_at!==null)return "revoked";
 if(ban.expires_at===null)return "permanent";
 if(now===null)return "unknown";
 return ban.expires_at>now?"temporary":"expired";
}
export function remainingSeconds(ban:Ban,now:number|null):number|null{
 return banState(ban,now)==="temporary"?Math.ceil(ban.expires_at!-now!):null;
}
export function matchesFilter(ban:Ban,filter:BanFilter,now:number|null):boolean{
 const state=banState(ban,now);return filter==="all"||state===filter||filter==="active"&&(state==="permanent"||state==="temporary");
}
// Anchor authoritative UTC to a monotonic clock, independent of browser clock skew.
export interface BanClock {at:number;received:number}
export function serverTime(clock:BanClock|null,monotonic:number):number|null{
 return clock===null?null:clock.at+Math.max(0,monotonic-clock.received)/1000;
}
export function parseObservedAt(value:unknown):number|undefined{
 if(value===undefined)return undefined; // Older API: do not infer expiry from local wall time.
 if(!time(value))throw new Error("Invalid server time");return value;
}
export function parseBanResult(value:unknown){
 if(!value||typeof value!=="object")throw new Error("Invalid ban response");const r=value as Record<string,unknown>;
 if(r.maintenance!==true||r.restart_required!==true)throw new Error("Invalid ban response");
 const observed_at=parseObservedAt(r.observed_at);
 return {maintenance:true as const,restart_required:true as const,ban:parseBan(r.ban),...(observed_at===undefined?{}:{observed_at})};
}
export const DURATION_UNITS={seconds:1,minutes:60,hours:3600,days:86400} as const;
export function durationSeconds(value:string,unit:string):number{
 if(!/^[1-9]\d{0,7}$/.test(value)||!Object.hasOwn(DURATION_UNITS,unit))throw new Error("Invalid duration");
 const seconds=Number(value)*DURATION_UNITS[unit as keyof typeof DURATION_UNITS];
 if(seconds>31536000)throw new Error("Invalid duration");return seconds;
}
