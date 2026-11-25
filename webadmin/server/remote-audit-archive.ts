// Offline, owner-acknowledged audit preservation rehearsal. This never rotates,
// truncates or resets the live audit hash chain; archived bytes remain private.
import { createHash, randomBytes } from "node:crypto";
import { constants, closeSync, fstatSync, fsyncSync, readFileSync,
  readSync, writeSync } from "node:fs";
import { isAbsolute, resolve } from "node:path";
import { openRemoteAuditStore } from "./remote-audit-store.ts";
import { assertPrivateParent, lstatPrivateLeaf, openPrivateLeaf, publishPrivateLeafNoReplace,
  syncPrivateParent, unlinkPrivateLeaf } from "./remote-private-path.ts";

const ack = "OWNER_STOPPED_WRITER_ACK";
const maxAuditBytes = 8 * 1024 * 1024;

function secureParent(path: string): void {
  if (!isAbsolute(path) || resolve(path) !== path) throw Error("Absolute archive path required");
  assertPrivateParent(path);
}

function absent(path: string): void {
  try { lstatPrivateLeaf(path); }
  catch (error) { if ((error as NodeJS.ErrnoException).code === "ENOENT") return; throw error; }
  throw Error("Archive destination already exists");
}

// Keep the original inode open throughout the archive operation. Reading the
// pathname again after verification would follow a substituted file, even if
// the substitute had identical bytes at the time of the first comparison.
export function openPinnedAuditSource(path: string) {
  secureParent(path);
  // Pin every ancestor as well as the leaf. O_NONBLOCK ensures a replaced
  // FIFO cannot stall an offline archival command before fstat rejects it.
  const fd = openPrivateLeaf(path, constants.O_RDONLY | constants.O_NONBLOCK);
  try {
    const opened = fstatSync(fd), disk = lstatPrivateLeaf(path);
    if (!disk.isFile() || !opened.isFile() || disk.dev !== opened.dev || disk.ino !== opened.ino ||
        opened.uid !== process.geteuid?.() || opened.nlink !== 1 || (opened.mode & 0o077) ||
        opened.size > maxAuditBytes) throw Error("Audit source path replaced or unsafe");
  } catch (error) { closeSync(fd); throw error; }
  let closed = false;
  const read = (): Buffer => {
    if (closed) throw Error("Pinned audit source closed");
    secureParent(path);
    const disk = lstatPrivateLeaf(path), opened = fstatSync(fd);
    if (!disk.isFile() || !opened.isFile() || disk.dev !== opened.dev || disk.ino !== opened.ino ||
        opened.uid !== process.geteuid?.() || opened.nlink !== 1 || (opened.mode & 0o077) ||
        disk.size !== opened.size || opened.size > maxAuditBytes)
      throw Error("Audit source path replaced or unsafe");
    const bytes = Buffer.alloc(opened.size);
    try {
      let offset = 0;
      while (offset < bytes.length) {
        const count = readSync(fd, bytes, offset, bytes.length - offset, offset);
        if (!count) throw Error("Audit source truncated during read");
        offset += count;
      }
      const after = fstatSync(fd), latest = lstatPrivateLeaf(path);
      if (after.dev !== opened.dev || after.ino !== opened.ino || after.size !== opened.size ||
          after.mtimeMs !== opened.mtimeMs || after.ctimeMs !== opened.ctimeMs ||
          latest.dev !== opened.dev || latest.ino !== opened.ino || latest.size !== opened.size)
        throw Error("Audit source changed during read");
      return bytes;
    } catch (error) { bytes.fill(0); throw error; }
  };
  return { read, close: () => { if (!closed) { closed = true; closeSync(fd); } } };
}

export function archiveStoppedRemoteAudit(source: string, destination: string, ownerAck: string) {
  if (ownerAck !== ack || source === destination) throw Error("Offline Owner acknowledgment required");
  secureParent(source);
  secureParent(destination);
  absent(destination);
  const pinned = openPinnedAuditSource(source);
  let original: Buffer | undefined;
  let partial: string | undefined;
  try {
    original = pinned.read();
    const digest = createHash("sha256").update(original).digest("hex");
    const assertSourceUnchanged = (): void => {
      const reread = pinned.read();
      try {
        if (createHash("sha256").update(reread).digest("hex") !== digest)
          throw Error("Audit source changed during archival");
      } finally { reread.fill(0); }
    };
    // The operator must stop the service externally: Node cannot attest its
    // process state. Keep the verifier live until the final snapshot check.
    const audit = openRemoteAuditStore(source);
    try {
      if (!audit.verify()) throw Error("Audit source verification failed");
      assertSourceUnchanged();
      partial = destination + ".partial-" + randomBytes(8).toString("hex");
      const fd = openPrivateLeaf(partial, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL, 0o600);
      try {
        let offset = 0;
        while (offset < original.length) {
          const written = writeSync(fd, original, offset, original.length - offset);
          if (!written) throw Error("Archive short write");
          offset += written;
        }
        fsyncSync(fd);
      } finally { closeSync(fd); }
      const copiedFd = openPrivateLeaf(partial, constants.O_RDONLY | constants.O_NONBLOCK);
      let copied: Buffer;
      try {
        const opened = fstatSync(copiedFd), leaf = lstatPrivateLeaf(partial);
        if (!opened.isFile() || opened.uid !== process.geteuid?.() || opened.nlink !== 1 ||
            (opened.mode & 0o077) || opened.size !== original.length ||
            opened.dev !== leaf.dev || opened.ino !== leaf.ino)
          throw Error("Archive copy path changed");
        copied = readFileSync(copiedFd);
        const after = fstatSync(copiedFd), latest = lstatPrivateLeaf(partial);
        if (copied.length !== opened.size || after.size !== opened.size ||
            after.mtimeMs !== opened.mtimeMs || after.ctimeMs !== opened.ctimeMs ||
            latest.dev !== opened.dev || latest.ino !== opened.ino)
          throw Error("Archive copy changed during read");
      }
      finally { closeSync(copiedFd); }
      try {
        if (createHash("sha256").update(copied).digest("hex") !== digest)
          throw Error("Archive copy changed");
      } finally { copied.fill(0); }
      if (!audit.verify()) throw Error("Audit source verification failed during archive");
      assertSourceUnchanged();
      absent(destination);
      publishPrivateLeafNoReplace(partial, destination);
      partial = undefined;
      syncPrivateParent(destination);
      return { schema: 1 as const, bytes_preserved: original.length, source_retained: true as const };
    } finally { audit.close(); }
  } finally {
    original?.fill(0);
    pinned.close();
    if (partial) { try { unlinkPrivateLeaf(partial); } catch { /* Only our partial file. */ } }
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
