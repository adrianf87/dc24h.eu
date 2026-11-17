import {ACCOUNT_ROLES} from "./roles.ts";
import type {AccountRole} from "./roles.ts";
export interface Account {
  id: string; name: string; role: AccountRole;
  status: "active" | "disabled"; revision: string;
  created_at: number; updated_at: number; last_login_at: number | null;
}
export async function readAccountResponse(response: Response): Promise<unknown> {
  if(!response.headers.get("content-type")?.startsWith("application/json"))throw new Error("Invalid content type");
  const reader=response.body?.getReader();if(!reader)throw new Error("Missing body");
  const decoder=new TextDecoder("utf-8",{fatal:true});let size=0,text="";
  try {
    for(;;){const part=await reader.read();if(part.done)break;size+=part.value.length;if(size>16384)throw new Error("Response too large");text+=decoder.decode(part.value,{stream:true});}
    return JSON.parse(text+decoder.decode()) as unknown;
  }finally{await reader.cancel();}
}
const id = (value: unknown): value is string => typeof value === "string" && /^[1-9]\d{0,18}$/.test(value) && BigInt(value) <= 9223372036854775807n;
export function parseAccount(value: unknown): Account {
  if (!value || typeof value !== "object") throw new Error("Invalid account");
  const a = value as Record<string, unknown>;
  const time = (v: unknown): v is number => typeof v === "number" && Number.isSafeInteger(v) && v >= 0 && v <= 253402300799;
  if (!id(a.id) || !id(a.revision) || typeof a.name !== "string" || !a.name || new TextEncoder().encode(a.name).length > 64 || /[\p{C}\p{Z}]/u.test(a.name) || a.name.normalize("NFC") !== a.name ||
      typeof a.role !== "string" || typeof a.status !== "string" ||
      !(ACCOUNT_ROLES as readonly string[]).includes(a.role) || !["active", "disabled"].includes(a.status) ||
      !time(a.created_at) || !time(a.updated_at) || a.updated_at < a.created_at ||
      !(a.last_login_at === null || (time(a.last_login_at) && a.last_login_at >= a.created_at && a.last_login_at <= a.updated_at))) throw new Error("Invalid account");
  return {id:a.id, name:a.name, role:a.role as Account["role"], status:a.status as Account["status"], revision:a.revision,
    created_at:a.created_at, updated_at:a.updated_at, last_login_at:a.last_login_at as number|null};
}
export function parseAccounts(value: unknown): {accounts: Account[]; next_after: string|null; maintenance:true} {
  if (!value || typeof value !== "object") throw new Error("Invalid accounts");
  const page = value as Record<string, unknown>;
  if (page.maintenance !== true || !Array.isArray(page.accounts) || page.accounts.length > 25 || !(page.next_after === null || id(page.next_after))) throw new Error("Invalid accounts");
  const accounts = page.accounts.map(parseAccount);
  if (accounts.some((a, i) => i && BigInt(a.id) <= BigInt(accounts[i-1].id)) ||
      (page.next_after !== null && (accounts.length !== 25 || page.next_after !== accounts.at(-1)?.id))) throw new Error("Invalid account page");
  return {accounts, next_after:page.next_after as string|null, maintenance:true};
}

export const STAFF_ROLES = ["moderator", "operator", "administrator", "owner"] as const;
export const isStaff = (account: Account) => (STAFF_ROLES as readonly string[]).includes(account.role);
export function parseStaff(value: unknown) {
  const page = parseAccounts(value);
  if ((value as Record<string, unknown>).staff_only !== true || !page.accounts.every(isStaff)) throw new Error("Invalid staff page");
  return {...page, staff_only: true as const};
}
