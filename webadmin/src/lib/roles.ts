export const ROLE_KEYS=["guest","registered","vip","moderator","operator","administrator","owner"] as const;
export const ACCOUNT_ROLES=["registered","vip","moderator","operator","administrator","owner"] as const;
export const PERMISSION_KEYS=["hub.use","chat.public","chat.private","search.queries","search.results","transfers.initiate","transfers.receive","users.kick","users.mute","users.ban","blocks.manage","accounts.manage","roles.manage","audit.read","configuration.manage","hub.manage","accounts.password.change"] as const;
export type RoleKey=typeof ROLE_KEYS[number];
export type AccountRole=typeof ACCOUNT_ROLES[number];
export type PermissionKey=typeof PERMISSION_KEYS[number];
export interface RolesSnapshot {
 vip_permissions:PermissionKey[];
 catalog:{key:RoleKey;rank:number;permissions:PermissionKey[]}[];
 operations:{key:PermissionKey;implemented:boolean}[];
}
function record(v:unknown):Record<string,unknown>{if(!v||typeof v!=="object"||Array.isArray(v))throw new Error("Invalid roles");return v as Record<string,unknown>;}
function permissions(v:unknown):PermissionKey[]{
 if(!Array.isArray(v)||v.length>PERMISSION_KEYS.length||new Set(v).size!==v.length||v.some(p=>typeof p!=="string"||!(PERMISSION_KEYS as readonly string[]).includes(p)))throw new Error("Invalid permissions");
 return [...v] as PermissionKey[];
}
export function parseRoles(value:unknown):RolesSnapshot {
 const r=record(value),vip_permissions=permissions(r.vip_permissions);
 if(vip_permissions.some(p=>!["users.kick","users.mute","audit.read"].includes(p)))throw new Error("Invalid VIP permissions");
 if(!Array.isArray(r.catalog)||r.catalog.length!==ROLE_KEYS.length||!Array.isArray(r.operations)||r.operations.length!==PERMISSION_KEYS.length)throw new Error("Invalid catalog");
 const catalog=r.catalog.map((value,i)=>{const role=record(value);if(role.key!==ROLE_KEYS[i]||role.rank!==i)throw new Error("Invalid hierarchy");return {key:ROLE_KEYS[i],rank:i,permissions:permissions(role.permissions)};});
 const operations=r.operations.map((value,i)=>{const p=record(value);if(p.key!==PERMISSION_KEYS[i]||typeof p.implemented!=="boolean")throw new Error("Invalid permission support");return {key:PERMISSION_KEYS[i],implemented:p.implemented};});
 return {vip_permissions,catalog,operations};
}
