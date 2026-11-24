// Offline operator-only maintenance. Do not route this command through HTTP.
// Usage: node --experimental-strip-types server/remote-mfa-tool.ts \
//   --enrollment ABS --replay ABS --request ABS --offline-ack [--recovery]
// Or: --replay ABS --compact-replay --offline-ack (stopped service only).
import { constants, closeSync, fstatSync, lstatSync, openSync, readFileSync } from "node:fs";
import { dirname, isAbsolute } from "node:path";
import { compactTotpReplay, initializeTotpReplay, replaceTotpSecret } from "./remote-mfa.ts";

function argument(flag: string): string {
  const index = process.argv.indexOf(flag);
  if (index < 0 || !process.argv[index + 1]?.startsWith("/")) throw new Error("Missing absolute " + flag);
  return process.argv[index + 1]!;
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
  if (![enrollmentPath, replayPath, path].every(isAbsolute)) throw new Error("Absolute paths required");
  const parent = lstatSync(dirname(path));
  if (!parent.isDirectory() || parent.uid !== process.geteuid?.() || (parent.mode & 0o077))
    throw new Error("Unsafe maintenance directory");
  const fd = openSync(path, constants.O_RDONLY | constants.O_NOFOLLOW);
  let data: unknown;
  try {
    const stat = fstatSync(fd);
    if (!stat.isFile() || stat.uid !== process.geteuid?.() || stat.nlink !== 1 ||
        (stat.mode & 0o077) || stat.size > 1024) throw new Error("Unsafe maintenance request");
    data = JSON.parse(readFileSync(fd, "utf8"));
  } finally { closeSync(fd); }
  if (!data || typeof data !== "object" || Array.isArray(data)) throw new Error("Invalid maintenance request");
  const row = data as Record<string, unknown>;
  if (row.schema !== 1 || typeof row.actor !== "string" || typeof row.new_seed !== "string" ||
      typeof row.new_code !== "string" || ("old_code" in row && typeof row.old_code !== "string") ||
      Object.keys(row).some(k => !["schema", "actor", "new_seed", "new_code", "old_code"].includes(k)))
    throw new Error("Invalid maintenance request");
  replaceTotpSecret({ enrollmentPath, replayPath, actor: row.actor, newSeed: row.new_seed,
    newCode: row.new_code, now: Date.now(), offlineAcknowledged: true,
    recovery: process.argv.includes("--recovery"),
    ...(typeof row.old_code === "string" ? { oldCode: row.old_code } : {}) });
  // Never print secrets or OTPs to stdout, a shell history or service journal.
  process.stdout.write("TOTP enrollment replaced; restart the local gateway and verify access.\n");
}

try { main(); }
catch { process.stderr.write("Offline MFA maintenance refused; inspect private files and operator procedure.\n"); process.exitCode = 1; }
