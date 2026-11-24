// Offline, owner-acknowledged audit preservation rehearsal. This never rotates,
// truncates or resets the live audit hash chain; archived bytes remain private.
import { createHash, randomBytes } from "node:crypto";
import { constants, closeSync, fsyncSync, lstatSync, openSync, readFileSync,
  renameSync, unlinkSync, writeSync } from "node:fs";
import { dirname, isAbsolute, resolve } from "node:path";
import { openRemoteAuditStore } from "./remote-audit-store.ts";
import { assertPrivateParent } from "./remote-private-path.ts";

const ack = "OWNER_STOPPED_WRITER_ACK";

function secureParent(path: string): void {
  if (!isAbsolute(path) || resolve(path) !== path) throw Error("Absolute archive path required");
  assertPrivateParent(path);
}

function absent(path: string): void {
  try { lstatSync(path); }
  catch (error) { if ((error as NodeJS.ErrnoException).code === "ENOENT") return; throw error; }
  throw Error("Archive destination already exists");
}

export function archiveStoppedRemoteAudit(source: string, destination: string, ownerAck: string) {
  if (ownerAck !== ack || source === destination) throw Error("Offline Owner acknowledgment required");
  secureParent(source);
  secureParent(destination);
  absent(destination);
  // A verified stopped-writer snapshot is required. The operator still must
  // stop the service externally: Node cannot attest systemd process state.
  const audit = openRemoteAuditStore(source);
  try { if (!audit.verify()) throw Error("Audit source verification failed"); }
  finally { audit.close(); }
  const original = readFileSync(source);
  let partial: string | undefined;
  try {
    if (original.length > 8 * 1024 * 1024) throw Error("Audit source too large");
    const digest = createHash("sha256").update(original).digest("hex");
    partial = destination + ".partial-" + randomBytes(8).toString("hex");
    const fd = openSync(partial, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL | constants.O_NOFOLLOW, 0o600);
    try {
      let offset = 0;
      while (offset < original.length) {
        const written = writeSync(fd, original, offset, original.length - offset);
        if (!written) throw Error("Archive short write");
        offset += written;
      }
      fsyncSync(fd);
    } finally { closeSync(fd); }
    // An in-place rewrite with unchanged inode, size and timestamp is caught
    // by content comparison; never publish a torn or unverified archive.
    const after = readFileSync(source);
    const copied = readFileSync(partial);
    try {
      if (createHash("sha256").update(after).digest("hex") !== digest ||
          createHash("sha256").update(copied).digest("hex") !== digest)
        throw Error("Audit source changed during archival");
    } finally { after.fill(0); copied.fill(0); }
    absent(destination);
    renameSync(partial, destination);
    partial = undefined;
    const dir = openSync(dirname(destination), constants.O_RDONLY | constants.O_DIRECTORY | constants.O_NOFOLLOW);
    try { fsyncSync(dir); } finally { closeSync(dir); }
    return { schema: 1 as const, bytes_preserved: original.length, source_retained: true as const };
  } finally {
    original.fill(0);
    if (partial) { try { unlinkSync(partial); } catch { /* Only our partial file. */ } }
  }
}

// Manual test-only/offline tool. No implicit archive, rotation or retention
// deletion occurs during authentication or routine gateway startup.
if (process.argv[1]?.endsWith("remote-audit-archive.ts")) {
  try {
    const args = process.argv.slice(2);
    if (args.length !== 5 || args[0] !== "--source" || args[2] !== "--archive" ||
        args[4] !== "--owner-stopped-writer-ack") throw Error();
    archiveStoppedRemoteAudit(args[1]!, args[3]!, ack);
    process.stdout.write("Private offline audit archive preserved; active audit remains unchanged.\n");
  } catch {
    process.stderr.write("Offline audit archival refused; no active audit reset performed.\n");
    process.exitCode = 1;
  }
}
