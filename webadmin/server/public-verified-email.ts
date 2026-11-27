// Read-only, owner-provisioned binding of active C++ account IDs to independently
// verified email addresses. The public browser cannot register or change these.
import { closeSync, constants, fstatSync, readFileSync } from "node:fs";
import { fingerprint, fingerprintPrivateFile } from "./private-catalog-integrity.ts";
import { assertPrivateParent, lstatPrivateLeaf, openPrivateLeaf } from "./remote-private-path.ts";
import { isSafeMailbox } from "./public-msmtp.ts";

type Row = { actor: string; email: string; verified: true };
type Catalog = { schema: 1; accounts: Row[] };
const exact = (object: object, keys: string[]) =>
  Object.keys(object).sort().join(",") === keys.sort().join(",");

export function openVerifiedEmailCatalog(path: string) {
  assertPrivateParent(path);
  const initial = lstatPrivateLeaf(path);
  if (!initial.isFile() || initial.uid !== process.geteuid?.() || initial.nlink !== 1 ||
      (initial.mode & 0o077) || initial.size < 2 || initial.size > 32768)
    throw Error("Unsafe verified email catalog");
  const fd = openPrivateLeaf(path, constants.O_RDONLY);
  let document: unknown, digest = "";
  try {
    const opened = fstatSync(fd);
    if (opened.dev !== initial.dev || opened.ino !== initial.ino) throw Error("Catalog replaced");
    const bytes = readFileSync(fd);
    try {
      if (bytes.length !== opened.size) throw Error("Catalog changed");
      digest = fingerprint(bytes);
      document = JSON.parse(bytes.toString("utf8")) as unknown;
    } finally { bytes.fill(0); }
  } finally { closeSync(fd); }
  if (!document || typeof document !== "object" || Array.isArray(document) ||
      !exact(document, ["schema", "accounts"])) throw Error("Invalid verified email catalog");
  const catalog = document as Catalog;
  if (catalog.schema !== 1 || !Array.isArray(catalog.accounts) || catalog.accounts.length > 64)
    throw Error("Invalid verified email catalog");
  const actors = new Set<string>(), addresses = new Set<string>();
  for (const row of catalog.accounts) {
    if (!row || typeof row !== "object" || !exact(row, ["actor", "email", "verified"]) ||
        typeof row.actor !== "string" || !/^[1-9]\d{0,18}$/.test(row.actor) ||
        BigInt(row.actor) > 9223372036854775807n || !isSafeMailbox(row.email) ||
        row.verified !== true || actors.has(row.actor) || addresses.has(row.email.toLowerCase()))
      throw Error("Invalid verified email binding");
    actors.add(row.actor); addresses.add(row.email.toLowerCase());
  }
  const atOpen = lstatPrivateLeaf(path);
  if (fingerprintPrivateFile(path, atOpen) !== digest) throw Error("Catalog changed during open");
  let healthy = true;
  const check = () => {
    if (!healthy) throw Error("Email binding unavailable");
    try {
      assertPrivateParent(path);
      const s = lstatPrivateLeaf(path);
      if (s.dev !== atOpen.dev || s.ino !== atOpen.ino || s.size !== atOpen.size ||
          s.mtimeMs !== atOpen.mtimeMs || s.ctimeMs !== atOpen.ctimeMs ||
          fingerprintPrivateFile(path, atOpen) !== digest) throw Error("Email catalog changed");
    } catch (error) { healthy = false; throw error; }
  };
  return {
    forActor: (actor: string): string | null => {
      check();
      return catalog.accounts.find(row => row.actor === actor)?.email ?? null;
    },
    healthy: () => { try { check(); return true; } catch { return false; } },
  };
}
