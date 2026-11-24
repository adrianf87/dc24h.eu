// One-writer, crash-resistant authentication budgets for the private gateway.
// Keys are HMACed with the private C++ service token; the catalog never stores
// a raw actor ID, password, IP address, credential ID or recovery identity.
import { createHmac, randomBytes } from "node:crypto";
import { basename, dirname, isAbsolute, join } from "node:path";
import { closeSync, constants, fstatSync, fsyncSync, lstatSync, openSync, readFileSync,
  renameSync, unlinkSync, writeSync } from "node:fs";

type Bucket = { key: string; count: number; starts: number };
type State = { schema: 1; buckets: Bucket[] };
const maxBytes = 65536;
const maxBuckets = 256;
const validTime = (v: unknown): v is number => Number.isSafeInteger(v) && (v as number) >= 0;
const hashPattern = /^[0-9a-f]{64}$/;

function validate(value: unknown): value is State {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const v = value as Partial<State>;
  if (Object.keys(v).sort().join(",") !== "buckets,schema" || v.schema !== 1 ||
      !Array.isArray(v.buckets) || v.buckets.length > maxBuckets) return false;
  const keys = new Set<string>();
  for (const b of v.buckets) {
    if (!b || typeof b !== "object" || Array.isArray(b) ||
        Object.keys(b).sort().join(",") !== "count,key,starts" ||
        typeof b.key !== "string" || !hashPattern.test(b.key) || keys.has(b.key) ||
        !Number.isSafeInteger(b.count) || b.count < 1 || b.count > 1_000_000 || !validTime(b.starts)) return false;
    keys.add(b.key);
  }
  return true;
}

function parentSafe(path: string) {
  if (!isAbsolute(path)) throw Error("Absolute private rate-limit path required");
  const s = lstatSync(dirname(path));
  if (!s.isDirectory() || s.uid !== process.geteuid?.() || (s.mode & 0o077))
    throw Error("Unsafe rate-limit directory");
}
function fileSafe(path: string) {
  const s = lstatSync(path);
  if (!s.isFile() || s.uid !== process.geteuid?.() || s.nlink !== 1 || (s.mode & 0o077) ||
      s.size < 2 || s.size > maxBytes) throw Error("Unsafe rate-limit catalog");
  return s;
}
function flushDirectory(path: string) {
  const fd = openSync(dirname(path), constants.O_RDONLY | constants.O_DIRECTORY | constants.O_NOFOLLOW);
  try { fsyncSync(fd); } finally { closeSync(fd); }
}

export function initializeRemoteRateLimitStore(path: string): void {
  parentSafe(path);
  const fd = openSync(path, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL | constants.O_NOFOLLOW, 0o600);
  try {
    const bytes = Buffer.from('{"schema":1,"buckets":[]}\n');
    try { if (writeSync(fd, bytes) !== bytes.length) throw Error("Incomplete limiter init"); fsyncSync(fd); }
    finally { bytes.fill(0); }
  } finally { closeSync(fd); }
  flushDirectory(path);
}
function persist(path: string, state: State) {
  const bytes = Buffer.from(JSON.stringify(state) + "\n");
  if (bytes.length > maxBytes) throw Error("Rate-limit catalog full");
  const tmp = join(dirname(path), "." + basename(path) + "." + randomBytes(12).toString("hex"));
  let fd: number | undefined;
  try {
    fd = openSync(tmp, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL | constants.O_NOFOLLOW, 0o600);
    if (writeSync(fd, bytes) !== bytes.length) throw Error("Incomplete rate-limit write");
    fsyncSync(fd); closeSync(fd); fd = undefined;
    renameSync(tmp, path);
    flushDirectory(path);
  } catch (error) {
    if (fd !== undefined) closeSync(fd);
    try { unlinkSync(tmp); } catch { /* The rename may already be durable. */ }
    throw error;
  } finally { bytes.fill(0); }
}

export function openRemoteRateLimitStore(path: string, serviceToken: string) {
  if (!/^[0-9a-f]{64}$/.test(serviceToken)) throw Error("Invalid private limiter key");
  parentSafe(path);
  let state: State;
  const prior = fileSafe(path);
  const fd = openSync(path, constants.O_RDONLY | constants.O_NOFOLLOW);
  try {
    const opened = fstatSync(fd);
    if (opened.dev !== prior.dev || opened.ino !== prior.ino) throw Error("Rate-limit file changed");
    const bytes = readFileSync(fd);
    try {
      if (bytes.length !== opened.size) throw Error("Rate-limit file changed");
      state = JSON.parse(bytes.toString("utf8")) as State;
    } finally { bytes.fill(0); }
  } finally { closeSync(fd); }
  if (!validate(state!)) throw Error("Invalid rate-limit catalog");
  let identity = fileSafe(path);
  let healthy = true;
  const check = () => {
    if (!healthy) throw Error("Rate limiter unavailable");
    try {
      parentSafe(path);
      const s = fileSafe(path);
      if (s.dev !== identity.dev || s.ino !== identity.ino || s.size !== identity.size ||
          s.mtimeMs !== identity.mtimeMs) throw Error("Rate-limit catalog changed outside gateway");
    } catch (error) { healthy = false; throw error; }
  };
  const secret = Buffer.from(serviceToken, "hex");
  return {
    healthy: () => { try { check(); return true; } catch { return false; } },
    hit: (key: string, limit: number, now: number): boolean => {
      if (!/^[a-z0-9:]{1,100}$/.test(key) || !Number.isSafeInteger(limit) || limit < 1 || limit > 100 ||
          !validTime(now)) throw Error("Invalid limiter request");
      check();
      const hash = createHmac("sha256", secret).update(key).digest("hex");
      const live = state.buckets.filter(b => b.starts + 60_000 > now);
      const old = live.find(b => b.key === hash);
      // An exhausted catalog denies *new* buckets; it never evicts a recent
      // actor to let another guesser bypass global and per-actor quotas.
      if (!old && live.length >= maxBuckets) return true;
      // Once a bucket is blocked, repeated abusive calls must not force an
      // fsync/rename for each rejection (an avoidable disk-I/O DoS).
      if (old && old.count > limit) return true;
      const next = old ? { ...old, count: Math.min(1_000_000, old.count + 1) } :
        { key: hash, count: 1, starts: now };
      const doc: State = { schema: 1, buckets: [...live.filter(b => b.key !== hash), next] };
      try { persist(path, doc); identity = fileSafe(path); state = doc; }
      catch (error) { healthy = false; throw error; }
      return next.count > limit;
    },
  };
}
export type RemoteRateLimitStore = ReturnType<typeof openRemoteRateLimitStore>;
