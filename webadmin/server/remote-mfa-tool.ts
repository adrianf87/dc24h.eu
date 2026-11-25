// Offline operator-only maintenance. Do not route this command through HTTP.
// Usage: node --experimental-strip-types server/remote-mfa-tool.ts \
//   --enrollment ABS --replay ABS --request ABS --offline-ack [--recovery]
// Or: --replay ABS --compact-replay --offline-ack (stopped service only).
import { compactTotpReplay, initializeTotpReplay, replaceTotpSecret } from "./remote-mfa.ts";
import { readPrivateJsonRequest } from "./remote-private-request.ts";

function argument(flag: string): string {
  const index = process.argv.indexOf(flag);
  if (index < 0 || !process.argv[index + 1]?.startsWith("/")) throw new Error("Missing absolute " + flag);
  return process.argv[index + 1]!;
}

export function parseTotpMaintenanceRequest(path: string, recovery: boolean): {
  actor: string; newSeed: string; newCode: string; oldCode?: string;
} {
  const data = readPrivateJsonRequest(path, 1024);
  if (!data || typeof data !== "object" || Array.isArray(data)) throw new Error("Invalid maintenance request");
  const row = data as Record<string, unknown>;
  if (row.schema !== 1 || typeof row.actor !== "string" || typeof row.new_seed !== "string" ||
      typeof row.new_code !== "string" || ("old_code" in row && typeof row.old_code !== "string") ||
      ("approvers" in row && (!Array.isArray(row.approvers) || row.approvers.length !== 2 ||
        !row.approvers.every(x => typeof x === "string" && /^[A-Za-z0-9._-]{3,64}$/.test(x)) ||
        row.approvers[0] === row.approvers[1])) ||
      (recovery ? !Array.isArray(row.approvers) || "old_code" in row : "approvers" in row) ||
      Object.keys(row).some(k => !["schema", "actor", "new_seed", "new_code", "old_code", "approvers"].includes(k)))
    throw new Error("Invalid maintenance request");
  return { actor: row.actor, newSeed: row.new_seed, newCode: row.new_code,
    ...(typeof row.old_code === "string" ? { oldCode: row.old_code } : {}) };
}

function main(): void {
  const allowed = new Set(["--enrollment", "--replay", "--request", "--offline-ack", "--recovery", "--compact-replay", "--init-replay"]);
  for (const item of process.argv.slice(2)) {
    if (item.startsWith("--") && !allowed.has(item)) throw new Error("Unknown maintenance option");
  }
  if (!process.argv.includes("--offline-ack")) throw new Error("Stop gateway and acknowledge offline maintenance");
  if (process.argv.includes("--init-replay")) {
    if (process.argv.includes("--compact-replay") || process.argv.includes("--recovery") ||
        process.argv.includes("--enrollment") || process.argv.includes("--request"))
      throw new Error("First-time replay initialization must run separately");
    initializeTotpReplay(argument("--replay"));
    process.stdout.write("Empty TOTP replay ledger initialized for first installation only.\n");
    return;
  }
  if (process.argv.includes("--compact-replay")) {
    if (process.argv.includes("--recovery") || process.argv.includes("--enrollment") ||
        process.argv.includes("--request")) throw new Error("Replay compaction must run separately");
    compactTotpReplay(argument("--replay"), true);
    process.stdout.write("MFA replay ledger compacted; restart the local gateway.\n");
    return;
  }
  const enrollmentPath = argument("--enrollment"), replayPath = argument("--replay"), path = argument("--request");
  const recovery = process.argv.includes("--recovery");
  const row = parseTotpMaintenanceRequest(path, recovery);
  replaceTotpSecret({ enrollmentPath, replayPath, actor: row.actor, newSeed: row.newSeed,
    newCode: row.newCode, now: Date.now(), offlineAcknowledged: true,
    recovery, ...(row.oldCode ? { oldCode: row.oldCode } : {}) });
  // Never print secrets or OTPs to stdout, a shell history or service journal.
  process.stdout.write("TOTP enrollment replaced; restart the local gateway and verify access.\n");
}

if (process.argv[1]?.endsWith("remote-mfa-tool.ts")) {
  try { main(); }
  catch { process.stderr.write("Offline MFA maintenance refused; inspect private files and operator procedure.\n"); process.exitCode = 1; }
}
