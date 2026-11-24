// Local durable audit foundation. A SHA-256 chain detects accidental record
// modification/gaps but is NOT a keyed proof against a privileged attacker.
import { createHash } from "node:crypto";
import { constants, closeSync, fstatSync, fsyncSync, readFileSync, readSync, writeSync } from "node:fs";
import { remoteAuditEvents, remoteAuditRecord } from "./remote-audit.ts";
import type { RemoteEvent } from "./remote-gateway.ts";
import { assertPrivateParent, lstatPrivateLeaf, openPrivateLeaf, syncPrivateParent } from "./remote-private-path.ts";

const maxBytes = 8 * 1024 * 1024;
const zeroHash = "0".repeat(64);
type Row = { schema: 1; sequence: number; previous: string; event: RemoteEvent; at: string; digest: string };
const eventNames: ReadonlySet<string> = new Set(remoteAuditEvents);

// Explicit first-installation only. Normal startup must never replace a lost
// audit chain with an empty file (which would conceal missing security events).
export function initializeRemoteAuditStore(path: string): void {
  assertPrivateParent(path);
  const fd = openPrivateLeaf(path, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL, 0o600);
  try { fsyncSync(fd); }
  finally { closeSync(fd); }
  syncPrivateParent(path);
}

function digest(sequence: number, previous: string, event: RemoteEvent, at: string): string {
  return createHash("sha256").update(JSON.stringify([1, sequence, previous, event, at])).digest("hex");
}

function checkRow(value: unknown, sequence: number, previous: string): Row {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid audit record");
  const item = value as Record<string, unknown>;
  if (Object.keys(item).sort().join(",") !== "at,digest,event,previous,schema,sequence" ||
      item.schema !== 1 || item.sequence !== sequence || item.previous !== previous ||
      typeof item.event !== "string" || !eventNames.has(item.event) ||
      typeof item.at !== "string" || !/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/.test(item.at) ||
      !Number.isFinite(Date.parse(item.at)) || new Date(item.at).toISOString() !== item.at ||
      typeof item.digest !== "string" || !/^[0-9a-f]{64}$/.test(item.digest) ||
      item.digest !== digest(sequence, previous, item.event as RemoteEvent, item.at))
    throw new Error("Invalid audit record or sequence gap");
  return { schema: 1, sequence, previous, event: item.event as RemoteEvent,
    at: item.at as string, digest: item.digest as string };
}

export function openRemoteAuditStore(path: string, limitBytes = maxBytes) {
  assertPrivateParent(path);
  // Production always uses 8 MiB. A lower fixed limit permits deterministic
  // disposable boundary tests without writing tens of thousands of fsyncs.
  if (!Number.isSafeInteger(limitBytes) || limitBytes < 512 || limitBytes > maxBytes)
    throw new Error("Invalid audit capacity");
  const uid = process.geteuid?.();
  const fd = openPrivateLeaf(path, constants.O_RDWR | constants.O_APPEND);
  let sequence = 0, previous = zeroHash, size = 0, healthy = true, dropped = 0;
  try {
    const state = fstatSync(fd);
    if (!state.isFile() || state.uid !== uid || state.nlink !== 1 || (state.mode & 0o077) ||
        state.size > limitBytes) throw new Error("Unsafe or full audit file");
    size = state.size;
    const bytes = readFileSync(fd);
    try {
      if (bytes.length !== size) throw new Error("Audit file changed during verification");
      const text = bytes.toString("utf8");
      if (text.length && !text.endsWith("\n")) throw new Error("Truncated audit record");
      for (const line of text ? text.slice(0, -1).split("\n") : []) {
        if (!line) throw new Error("Empty audit record");
        if (sequence >= Number.MAX_SAFE_INTEGER) throw new Error("Audit sequence exhausted");
        const entry = checkRow(JSON.parse(line), sequence + 1, previous);
        previous = entry.digest;
        sequence++;
      }
    } finally { bytes.fill(0); }
    // Persist the directory entry if this call created the log file.
    syncPrivateParent(path);
  } catch (error) { closeSync(fd); throw error; }

  const append = (event: RemoteEvent, milliseconds: number): void => {
    if (!healthy) throw new Error("Audit sink unavailable");
    try {
      assertPrivateParent(path);
      const at = JSON.parse(remoteAuditRecord(event, milliseconds)).at as string;
      if (sequence >= Number.MAX_SAFE_INTEGER) throw new Error("Audit sequence exhausted");
      const row: Row = { schema: 1, sequence: sequence + 1, previous, event, at,
        digest: digest(sequence + 1, previous, event, at) };
      const payload = Buffer.from(JSON.stringify(row) + "\n", "utf8");
      try {
        const stat = lstatPrivateLeaf(path), opened = fstatSync(fd);
        if (!stat.isFile() || stat.dev !== opened.dev || stat.ino !== opened.ino ||
            stat.uid !== uid || stat.nlink !== 1 || (stat.mode & 0o077) ||
            opened.size !== size || size + payload.length > limitBytes)
          throw new Error("Audit sink rotated, corrupted or full");
        if (writeSync(fd, payload) !== payload.length) throw new Error("Incomplete audit write");
        fsyncSync(fd);
        size += payload.length;
        sequence = row.sequence;
        previous = row.digest;
      } finally { payload.fill(0); }
    } catch (error) {
      healthy = false;
      dropped++;
      throw error;
    }
  };
  let closed = false;
  const verify = (): boolean => {
    if (!healthy || closed) return false;
    // The append path checks inode/size before writes. This separate bounded
    // scan catches an in-place rewrite of an existing row of the *same* size,
    // including during an otherwise idle authentication period.
    const bytes = Buffer.alloc(size);
    try {
      assertPrivateParent(path);
      const stat = lstatPrivateLeaf(path), opened = fstatSync(fd);
      if (!stat.isFile() || stat.dev !== opened.dev || stat.ino !== opened.ino ||
          stat.uid !== uid || stat.nlink !== 1 || (stat.mode & 0o077) ||
          opened.size !== size || size > limitBytes) throw Error("Audit file changed");
      let offset = 0;
      while (offset < bytes.length) {
        const count = readSync(fd, bytes, offset, bytes.length - offset, offset);
        if (!count) throw Error("Audit file truncated");
        offset += count;
      }
      const text = bytes.toString("utf8");
      if (text.length && !text.endsWith("\n")) throw Error("Audit file truncated");
      let atSequence = 0, prev = zeroHash;
      for (const line of text ? text.slice(0, -1).split("\n") : []) {
        if (!line || atSequence >= Number.MAX_SAFE_INTEGER) throw Error("Audit sequence invalid");
        const row = checkRow(JSON.parse(line), atSequence + 1, prev);
        atSequence++; prev = row.digest;
      }
      if (atSequence !== sequence || prev !== previous) throw Error("Audit tail changed");
      return true;
    } catch {
      healthy = false;
      dropped++;
      return false;
    } finally { bytes.fill(0); }
  };
  return {
    append,
    verify,
    health: () => ({ healthy: healthy && !closed, dropped, sequence }),
    // Only a local operator signal; never projected to authenticated browsers.
    capacity: () => ({ used_bytes: size, limit_bytes: limitBytes,
      near_limit: size >= Math.floor(limitBytes * 0.75) }),
    close: () => { if (!closed) { closed = true; closeSync(fd); } },
  };
}
