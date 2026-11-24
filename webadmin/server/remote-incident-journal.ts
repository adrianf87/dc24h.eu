// Optional, disposable operator telemetry. This is NOT a sixth authentication
// store, an audit replacement, or proof that the hub is currently healthy.
// Persist only fixed aggregate event names and timestamps, never raw audit.
import { createHash, randomBytes } from "node:crypto";
import { constants, closeSync, fstatSync, fsyncSync, lstatSync, openSync,
  readFileSync, renameSync, unlinkSync, writeSync } from "node:fs";
import { dirname, isAbsolute, resolve } from "node:path";
import type { RedactedEvent, RedactedEventKind } from "./remote-observability.ts";
import { assertPrivateParent } from "./remote-private-path.ts";

const kinds = new Set<RedactedEventKind>([
  "hub_started", "hub_stopped", "transport_refusals", "login_refusals", "rate_limit_refusals",
]);
const maxEvents = 12;
const maxBytes = 4096;
const retentionMs = 10 * 60_000;
type Entry = { kind: RedactedEventKind; at_unix_ms: number };
type Snapshot = { schema: 1; events: Entry[] };
export type RetainedIncidents = { state: "available" | "unknown"; events: RedactedEvent[] };

function assertParent(path: string): void {
  if (!isAbsolute(path) || resolve(path) !== path) throw Error("Absolute canonical incident path required");
  assertPrivateParent(path);
}

function decode(bytes: Buffer): Snapshot {
  if (bytes.length < 2 || bytes.length > maxBytes) throw Error("Incident journal size invalid");
  const value: unknown = JSON.parse(bytes.toString("utf8"));
  if (!value || typeof value !== "object" || Array.isArray(value)) throw Error("Incident journal schema invalid");
  const record = value as Record<string, unknown>;
  if (Object.keys(record).sort().join(",") !== "events,schema" || record.schema !== 1 ||
      !Array.isArray(record.events) || record.events.length > maxEvents)
    throw Error("Incident journal schema invalid");
  let prior = -1;
  const events: Entry[] = [];
  for (const raw of record.events) {
    if (!raw || typeof raw !== "object" || Array.isArray(raw)) throw Error("Incident event invalid");
    const item = raw as Record<string, unknown>;
    if (Object.keys(item).sort().join(",") !== "at_unix_ms,kind" ||
        typeof item.kind !== "string" || !kinds.has(item.kind as RedactedEventKind) ||
        !Number.isSafeInteger(item.at_unix_ms) || (item.at_unix_ms as number) < prior ||
        (item.at_unix_ms as number) < 0)
      throw Error("Incident event invalid");
    prior = item.at_unix_ms as number;
    events.push({ kind: item.kind as RedactedEventKind, at_unix_ms: prior });
  }
  return { schema: 1, events };
}

type FileIdentity = { dev: number; ino: number };
type JournalSnapshot = { value: Snapshot; fingerprint: string; identity: FileIdentity };

function sameGeneration(a: JournalSnapshot, b: JournalSnapshot): boolean {
  return a.identity.dev === b.identity.dev && a.identity.ino === b.identity.ino &&
    a.fingerprint === b.fingerprint;
}

function readSnapshot(path: string): JournalSnapshot {
  assertParent(path);
  const stat = lstatSync(path);
  const fd = openSync(path, constants.O_RDONLY | constants.O_NOFOLLOW);
  try {
    const opened = fstatSync(fd);
    if (!stat.isFile() || !opened.isFile() || stat.ino !== opened.ino ||
        stat.dev !== opened.dev || opened.nlink !== 1 ||
        opened.uid !== process.geteuid?.() || (opened.mode & 0o077) ||
        opened.size < 2 || opened.size > maxBytes)
      throw Error("Unsafe incident journal");
    const bytes = readFileSync(fd);
    try {
      if (bytes.length !== opened.size) throw Error("Incident journal changed during read");
      return { value: decode(bytes), fingerprint: createHash("sha256").update(bytes).digest("hex"),
        identity: { dev: opened.dev, ino: opened.ino } };
    } finally { bytes.fill(0); }
  } finally { closeSync(fd); }
}

function syncDirectory(path: string): void {
  const fd = openSync(dirname(path), constants.O_RDONLY | constants.O_DIRECTORY | constants.O_NOFOLLOW);
  try { fsyncSync(fd); } finally { closeSync(fd); }
}

export function initializeIncidentJournal(path: string): void {
  assertParent(path);
  const fd = openSync(path, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL | constants.O_NOFOLLOW, 0o600);
  try {
    writeSync(fd, Buffer.from('{"schema":1,"events":[]}\n'));
    fsyncSync(fd);
  } finally { closeSync(fd); }
  syncDirectory(path);
}

export function openIncidentJournal(path: string, now: () => number = Date.now) {
  let current = readSnapshot(path);
  let healthy = true;
  let lastWall: number | undefined;
  const checkedTime = () => {
    const time = now();
    if (!Number.isSafeInteger(time) || time < 0 || (lastWall !== undefined && time < lastWall))
      throw Error("Incident clock reversed or invalid");
    lastWall = time;
    return time;
  };
  const fail = (): RetainedIncidents => ({ state: "unknown", events: [] });
  const read = (): RetainedIncidents => {
    if (!healthy) return fail();
    try {
      const time = checkedTime();
      const fresh = readSnapshot(path);
      if (!sameGeneration(fresh, current)) throw Error("Incident journal changed outside writer");
      if (fresh.value.events.some(e => e.at_unix_ms > time)) throw Error("Incident clock reversed");
      return { state: "available", events: fresh.value.events
        .filter(e => time - e.at_unix_ms <= retentionMs)
        .map(e => ({ kind: e.kind, age_seconds: Math.floor((time - e.at_unix_ms) / 1000) })) };
    } catch { healthy = false; return fail(); }
  };
  const append = (kind: RedactedEventKind): void => {
    if (!healthy || !kinds.has(kind)) throw Error("Incident journal unavailable");
    let temporary: string | undefined;
    try {
      const time = checkedTime();
      const fresh = readSnapshot(path);
      if (!sameGeneration(fresh, current) ||
          fresh.value.events.some(e => e.at_unix_ms > time)) throw Error("Incident generation invalid");
      const entries = fresh.value.events.filter(e => time - e.at_unix_ms <= retentionMs);
      entries.push({ kind, at_unix_ms: time });
      const bytes = Buffer.from(JSON.stringify({ schema: 1, events: entries.slice(-maxEvents) }) + "\n");
      try {
        if (bytes.length > maxBytes) throw Error("Incident journal full");
        temporary = path + ".partial-" + randomBytes(8).toString("hex");
        const fd = openSync(temporary, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL | constants.O_NOFOLLOW, 0o600);
        try {
          if (writeSync(fd, bytes) !== bytes.length) throw Error("Short incident write");
          fsyncSync(fd);
        } finally { closeSync(fd); }
        // A changed/aliased destination must not be silently replaced.
        if (!sameGeneration(readSnapshot(path), current))
          throw Error("Incident destination changed");
        renameSync(temporary, path);
        temporary = undefined;
        syncDirectory(path);
        current = readSnapshot(path);
      } finally { bytes.fill(0); }
    } catch (error) { healthy = false; throw error; }
    finally { if (temporary) { try { unlinkSync(temporary); } catch { /* Own partial only. */ } } }
  };
  return { append, read, unavailable: () => { healthy = false; } };
}

if (process.argv[1]?.endsWith("remote-incident-journal.ts")) {
  try {
    const args = process.argv.slice(2);
    if (args.length !== 3 || args[0] !== "--initialize" || args[2] !== "--offline-ack") throw Error();
    initializeIncidentJournal(args[1]!);
    process.stdout.write("Optional private incident history initialized; not an authentication store.\n");
  } catch {
    process.stderr.write("Offline incident journal initialization refused.\n");
    process.exitCode = 1;
  }
}
