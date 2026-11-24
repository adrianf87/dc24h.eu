// Explicit first-installation-only bootstrap for the four non-secret remote
// gateway state files. A lost file on an active installation MUST be restored
// from its matched backup; this command is never a recovery/reset operation.
import { isAbsolute, join } from "node:path";
import { assertPrivateDirectory, lstatPrivateLeaf } from "./remote-private-path.ts";
import { initializeTotpReplay } from "./remote-mfa.ts";
import { initializeRemoteAuditStore } from "./remote-audit-store.ts";
import { initializePasskeyStore } from "./remote-passkeys.ts";
import { initializeRemoteRateLimitStore } from "./remote-rate-limit-store.ts";

export function initializeRemotePrivateState(directory: string, rpID: string): void {
  if (!isAbsolute(directory)) throw Error("Absolute first-installation state directory required");
  assertPrivateDirectory(directory);
  // MFA enrollment is provisioned separately by the offline operator. Do not
  // mint, copy, print or replace any TOTP seed as part of this initialization.
  const enrollment = lstatPrivateLeaf(join(directory, "mfa.json"));
  if (!enrollment.isFile() || enrollment.uid !== process.geteuid?.() ||
      enrollment.nlink !== 1 || (enrollment.mode & 0o077) ||
      enrollment.size < 2 || enrollment.size > 16_384)
    throw Error("Existing private MFA enrollment required");

  const names = ["replay.jsonl", "audit.jsonl", "passkeys.json", "rate-limits.json"] as const;
  for (const name of names) {
    try { lstatPrivateLeaf(join(directory, name)); }
    catch (e) {
      if ((e as NodeJS.ErrnoException).code === "ENOENT") continue;
      throw e;
    }
    throw Error("State already exists: refuse partial bootstrap or recovery reset");
  }
  initializeTotpReplay(join(directory, names[0]));
  initializeRemoteAuditStore(join(directory, names[1]));
  initializePasskeyStore(join(directory, names[2]), rpID);
  initializeRemoteRateLimitStore(join(directory, names[3]));
}

if (process.argv[1]?.endsWith("remote-private-init.ts")) {
  try {
    const args = process.argv.slice(2);
    if (args.length !== 6 || args[0] !== "--directory" || args[2] !== "--rp-id" ||
        args[4] !== "--offline-ack" || args[5] !== "--first-install-ack") throw Error();
    initializeRemotePrivateState(args[1]!, args[3]!);
    process.stdout.write("First-installation gateway catalogs initialized; keep the matched backup.\n");
  } catch {
    process.stderr.write("Offline gateway bootstrap refused; inspect the isolated private state.\n");
    process.exitCode = 1;
  }
}
