// One-time Stage IV catalog provisioning on an isolated host only.
// Never use for reset, recovery or replacement of existing private state.
import { join, isAbsolute } from "node:path";
import { assertPrivateDirectory, lstatPrivateLeaf } from "./remote-private-path.ts";
import { initializePasskeyStore } from "./remote-passkeys.ts";
import { initializeRemoteAuditStore } from "./remote-audit-store.ts";
import { initializeRemoteRateLimitStore } from "./remote-rate-limit-store.ts";
import { PUBLIC_WEBADMIN_RP_ID } from "./public-two-factor.ts";

export function initializeStageIVState(directory: string) {
  if (!isAbsolute(directory)) throw Error("Absolute, privately owned directory required");
  assertPrivateDirectory(directory);
  const paths = ["stage4-passkeys.json", "stage4-audit.jsonl", "stage4-limits.json"]
    .map(name => join(directory, name));
  for (const path of paths) {
    try { lstatPrivateLeaf(path); }
    catch (e) { if ((e as NodeJS.ErrnoException).code === "ENOENT") continue; throw e; }
    throw Error("Refuse reinitialization or partial catalog replacement");
  }
  initializePasskeyStore(paths[0]!, PUBLIC_WEBADMIN_RP_ID);
  initializeRemoteAuditStore(paths[1]!);
  initializeRemoteRateLimitStore(paths[2]!);
}

if (process.argv[1]?.endsWith("public-private-init.ts")) {
  try {
    const args = process.argv.slice(2);
    if (args.length !== 4 || args[0] !== "--directory" || args[2] !== "--owner-ack" ||
        args[3] !== "STAGE4_FIRST_INSTALL_ONLY") throw Error();
    initializeStageIVState(args[1]!);
    process.stdout.write("Stage IV private catalogs initialized. No user or passkey enrolled.\n");
  } catch {
    process.stderr.write("Stage IV first-install initialization refused.\n");
    process.exitCode = 1;
  }
}
