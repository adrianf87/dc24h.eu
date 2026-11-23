// Local durable audit foundation. A SHA-256 chain detects accidental record
// modification/gaps but is NOT a keyed proof against a privileged attacker.
import { createHash } from "node:crypto";
import { constants, closeSync, fstatSync, fsyncSync, lstatSync, openSync, readFileSync, writeSync } from "node:fs";
import { dirname, isAbsolute } from "node:path";
import { remoteAuditEvents, remoteAuditRecord } from "./remote-audit.ts";
import type { RemoteEvent } from "./remote-gateway.ts";

const maxBytes = 8 * 1024 * 1024;
const zeroHash = "0".repeat(64);
type Row = { schema: 1; sequence: number; previous: string; event: RemoteEvent; at: string; digest: string };
const eventNames: ReadonlySet<string> = new Set(remoteAuditEvents);

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

export function openRemoteAuditStore(path: string) {
  if (!isAbsolute(path)) throw new Error("Absolute audit file required");
  const uid = process.geteuid?.();
  const parent = lstatSync(dirname(path));
  if (!parent.isDirectory() || parent.uid !== uid || (parent.mode & 0o077))
    throw new Error("Unsafe audit directory");
  const fd = openSync(path, constants.O_RDWR | constants.O_APPEND | constants.O_CREAT | constants.O_NOFOLLOW, 0o600);
  let sequence = 0, previous = zeroHash, size = 0, healthy = true, dropped = 0;
  try {
    const state = fstatSync(fd);
    if (!state.isFile() || state.uid !== uid || state.nlink !== 1 || (state.mode & 0o077) ||
        state.size > maxBytes) throw new Error("Unsafe or full audit file");
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
    const dirFd = openSync(dirname(path), constants.O_RDONLY | constants.O_DIRECTORY | constants.O_NOFOLLOW);
    try { fsyncSync(dirFd); } finally { closeSync(dirFd); }
  } catch (error) { closeSync(fd); throw error; }

  const append = (event: RemoteEvent, milliseconds: number): void => {
    if (!healthy) throw new Error("Audit sink unavailable");
    try {
      const at = JSON.parse(remoteAuditRecord(event, milliseconds)).at as string;
      if (sequence >= Number.MAX_SAFE_INTEGER) throw new Error("Audit sequence exhausted");
      const row: Row = { schema: 1, sequence: sequence + 1, previous, event, at,
        digest: digest(sequence + 1, previous, event, at) };
      const payload = Buffer.from(JSON.stringify(row) + "\n", "utf8");
      try {
        const stat = lstatSync(path), opened = fstatSync(fd);
        if (!stat.isFile() || stat.dev !== opened.dev || stat.ino !== opened.ino ||
            stat.uid !== uid || stat.nlink !== 1 || (stat.mode & 0o077) ||
            opened.size !== size || size + payload.length > maxBytes)
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
  return {
    append,
    health: () => ({ healthy: healthy && !closed, dropped, sequence }),
    close: () => { if (!closed) { closed = true; closeSync(fd); } },
  };
}
