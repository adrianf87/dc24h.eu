// Offline, one-time mailbox proof for a pre-existing C++ OWNER/admin identity.
// This never creates an administrator, changes roles or exposes an HTTP route.
import { randomBytes, randomInt, timingSafeEqual } from "node:crypto";
import { closeSync, constants, fsyncSync, fstatSync, writeSync } from "node:fs";
import { isAbsolute } from "node:path";
import { isSafeMailbox } from "./public-msmtp.ts";
import { assertPrivateParent, lstatPrivateLeaf, openPrivateLeaf,
  publishPrivateLeafNoReplace, syncPrivateParent, unlinkPrivateLeaf } from "./remote-private-path.ts";
import type { PublicPasswordGrant } from "./public-email-code.ts";

const validActor = (id: string) => /^[1-9]\d{0,18}$/.test(id) &&
  BigInt(id) <= 9223372036854775807n;

export type EnrollmentDependencies<Grant extends PublicPasswordGrant> = {
  verifyPassword(actor: string, password: string): Promise<Grant | null>;
  reauthorize(grant: Grant): Promise<boolean>;
  sendCode(email: string, code: string): Promise<boolean>;
  readCode(signal: AbortSignal): Promise<string>;
  now?: () => number;
};

/** Only bootstraps a MISSING catalog. Existing bindings demand a separate,
 * audited, offline rotation procedure; never silently replace a mailbox. */
export async function enrollFirstVerifiedEmail<Grant extends PublicPasswordGrant>(
  catalogPath: string, actor: string, email: string, password: string,
  deps: EnrollmentDependencies<Grant>,
): Promise<boolean> {
  if (!isAbsolute(catalogPath) || !validActor(actor) || !isSafeMailbox(email) ||
      !password || password.length > 1024) return false;
  assertPrivateParent(catalogPath);
  const absent = () => {
    try { lstatPrivateLeaf(catalogPath); return false; }
    catch (error) {
      if ((error as NodeJS.ErrnoException).code === "ENOENT") return true;
      throw error;
    }
  };
  if (!absent()) return false;
  const grant = await deps.verifyPassword(actor, password).catch(() => null);
  if (!grant || grant.actor !== actor || !["owner", "administrator"].includes(grant.role) ||
      !await deps.reauthorize(grant).catch(() => false) || !absent()) return false;

  const code = randomInt(0, 100_000_000).toString().padStart(8, "0");
  const start = (deps.now ?? Date.now)();
  if (!await deps.sendCode(email, code).catch(() => false)) return false;

  const abort = new AbortController();
  const timer = setTimeout(() => abort.abort(), 300_000);
  let supplied: string;
  try { supplied = await deps.readCode(abort.signal); }
  catch { return false; }
  finally { clearTimeout(timer); }
  const at = (deps.now ?? Date.now)();
  if (at < start || at - start >= 300_000 || !/^[0-9]{8}$/.test(supplied) ||
      !timingSafeEqual(Buffer.from(code), Buffer.from(supplied)) ||
      !await deps.reauthorize(grant).catch(() => false) || !absent()) return false;

  // Pin the private directory and atomically publish without replacing a
  // pre-existing file. A unique private temporary file is fsync'ed first.
  const temporary = catalogPath + ".enroll-" + randomBytes(12).toString("hex");
  const bytes = Buffer.from(JSON.stringify({ schema: 1,
    accounts: [{ actor, email, verified: true }] }) + "\n", "utf8");
  let fd: number | undefined;
  try {
    fd = openPrivateLeaf(temporary, constants.O_CREAT | constants.O_EXCL | constants.O_WRONLY, 0o600);
    const stat = fstatSync(fd);
    if (!stat.isFile() || stat.uid !== process.geteuid?.() || stat.nlink !== 1 ||
        (stat.mode & 0o077)) throw Error("Unsafe enrollment state");
    let written = 0;
    while (written < bytes.length) written += writeSync(fd, bytes, written, bytes.length - written);
    fsyncSync(fd); closeSync(fd); fd = undefined;
    publishPrivateLeafNoReplace(temporary, catalogPath);
    syncPrivateParent(catalogPath);
    return true;
  } finally {
    bytes.fill(0);
    if (fd !== undefined) closeSync(fd);
    try { unlinkPrivateLeaf(temporary); }
    catch (error) { if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error; }
  }
}
