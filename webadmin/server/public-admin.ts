import { parseAccount, parseAccounts, parseStaff } from "../src/lib/accounts.ts";
import { parseAdmin } from "../src/lib/admin.ts";
import { parseAudit } from "../src/lib/audit.ts";
import { BAN_FILTERS, parseBanResult, parseBans } from "../src/lib/bans.ts";
import { parseManagement } from "../src/lib/management.ts";
import { ACCOUNT_ROLES } from "../src/lib/roles.ts";

type Validator = (value: string) => boolean;
const decimal: Validator = value => /^(0|[1-9][0-9]{0,18})$/.test(value) && BigInt(value) <= 9223372036854775807n;
const id: Validator = value => decimal(value) && value !== "0";
const text = (bytes: number, min = 1): Validator => value =>
  value.length >= min && Buffer.byteLength(value) <= bytes && !/[\u0000-\u001f\u007f]/u.test(value);
const proof = text(1024);
const password = text(1024, 15);
const cursor = { after: decimal };
const target = { proof, id, revision: id };
const expiry: Validator = value => value === "permanent" ||
  (value.startsWith("duration:") && id(value.slice(9)) && Number(value.slice(9)) <= 31536000) ||
  (value.startsWith("until:") && id(value.slice(6)) && Number(value.slice(6)) <= 253402300799);

const record = (value: unknown) => {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw Error("Invalid administrator response");
  return value as Record<string, unknown>;
};
const accountResult = (value: unknown) => {
  const data = record(value);
  if (data.maintenance !== true) throw Error("Invalid account response");
  return { account: parseAccount(data.account) };
};

const operations = {
  "management": { fields: {}, parse: parseManagement },
  "admin": { fields: {}, parse: parseAdmin },
  "accounts.list": { fields: cursor, parse: parseAccounts },
  "accounts.staff": { fields: cursor, parse: parseStaff },
  "accounts.detail": { fields: { id }, parse: accountResult },
  "accounts.create": { fields: { proof, name: text(64), password }, parse: accountResult },
  "accounts.activate": { fields: target, parse: accountResult },
  "accounts.disable": { fields: target, parse: accountResult },
  "accounts.reset": { fields: { ...target, password }, parse: accountResult },
  "accounts.role": { fields: { ...target, role: value => value !== "owner" &&
    (ACCOUNT_ROLES as readonly string[]).includes(value) }, parse: accountResult },
  "bans.list": { fields: { ...cursor, state: value => (BAN_FILTERS as readonly string[]).includes(value) }, parse: parseBans },
  "bans.create": { fields: { proof, kind: value => ["account", "cid", "ip"].includes(value),
    subject: text(64), reason: text(256), expiry },
  parse: parseBanResult },
  "bans.revoke": { fields: target, parse: parseBanResult },
  "audit.list": { fields: { ...cursor, proof }, parse: parseAudit },
} satisfies Record<string, { fields: Record<string, Validator>; parse: (value: unknown) => unknown }>;

export type PublicAdminOperation = keyof typeof operations;
export type PublicAdminInput<K extends PublicAdminOperation> =
  Record<keyof typeof operations[K]["fields"], string>;
export type PublicAdminResult<K extends PublicAdminOperation> = ReturnType<typeof operations[K]["parse"]>;

export function readAdminInput(input: Record<string, unknown>) {
  const { operation, ...values } = input;
  if (typeof operation !== "string" || !Object.hasOwn(operations, operation)) return null;
  const name = operation as PublicAdminOperation;
  const { fields, parse } = operations[name];
  const entries = Object.entries<Validator>(fields);
  if (Object.keys(values).length !== entries.length || !entries.every(([field, valid]) =>
    Object.hasOwn(values, field) && typeof values[field] === "string" && valid(values[field]))) return null;
  return { operation: name, values: values as Record<string, string>, parse };
}
