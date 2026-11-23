// Local-only TOTP enrollment is deliberately out of band. Neither the seed
// nor the replay ledger is ever returned by the browser gateway.
import { createHmac, randomBytes, timingSafeEqual } from "node:crypto";
import { constants, closeSync, fstatSync, fsyncSync, lstatSync, openSync, readFileSync, readSync, writeSync } from "node:fs";
import { dirname, isAbsolute } from "node:path";

type Secrets = Map<string, Buffer>;
const actorPattern = /^[1-9][0-9]{0,18}$/;
const seedPattern = /^[A-Z2-7]{32}(?:[A-Z2-7]{8}){0,4}$/;
const ledgerLimit = 1024 * 1024;
const noncePattern = /^[0-9a-f]{32}$/;

type ReplayEntry = { actor: string; step: number; nonce?: string };

function visitReplayLedger(fd: number, visit: (entry: ReplayEntry) => void): void {
  const size = fstatSync(fd).size;
  if (size > ledgerLimit) throw new Error("MFA replay ledger full");
  // An explicit offset is essential: O_APPEND advances the shared descriptor
  // on write, so readFileSync(fd) would miss records written by another process.
  const bytes = Buffer.alloc(size);
  try {
    let offset = 0;
    while (offset < size) {
      const count = readSync(fd, bytes, offset, size - offset, offset);
      if (count === 0) throw new Error("Truncated MFA replay ledger");
      offset += count;
    }
    const text = bytes.toString("utf8");
    if (text && !text.endsWith("\n")) throw new Error("Incomplete MFA replay ledger");
    for (const line of text.split("\n")) {
      if (!line) continue;
      const record: unknown = JSON.parse(line);
      if (!record || typeof record !== "object" || Array.isArray(record)) throw new Error("Invalid MFA replay ledger");
      const row = record as Record<string, unknown>;
      if (typeof row.actor !== "string" || !actorPattern.test(row.actor) || !Number.isSafeInteger(row.step) ||
        (row.step as number) < 0 || ("nonce" in row &&
        (typeof row.nonce !== "string" || !noncePattern.test(row.nonce))) ||
        Object.keys(row).length !== ("nonce" in row ? 3 : 2)) throw new Error("Invalid MFA replay entry");
      visit({ actor: row.actor, step: row.step as number,
        ...("nonce" in row ? { nonce: row.nonce as string } : {}) });
    }
  } finally { bytes.fill(0); }
}

function privateFile(path: string, flags: number, create = false): number {
  if (!isAbsolute(path)) throw new Error("Private MFA file configuration required");
  const uid = process.geteuid?.();
  const parent = lstatSync(dirname(path));
  if (!parent.isDirectory() || parent.uid !== uid || (parent.mode & 0o077))
    throw new Error("Unsafe MFA directory");
  const fd = openSync(path, flags | constants.O_NOFOLLOW, create ? 0o600 : undefined);
  try {
    const file = fstatSync(fd);
    if (!file.isFile() || file.uid !== uid || file.nlink !== 1 || (file.mode & 0o077))
      throw new Error("Unsafe MFA file");
    return fd;
  } catch (error) { closeSync(fd); throw error; }
}

function base32Decode(text: string): Buffer {
  if (!seedPattern.test(text)) throw new Error("Invalid MFA seed");
  const result = Buffer.alloc(text.length * 5 / 8);
  let bits = 0, value = 0, at = 0;
  for (const letter of text) {
    const char = letter.charCodeAt(0);
    const digit = char >= 65 && char <= 90 ? char - 65 : char - 50 + 26;
    value = (value << 5) | digit;
    bits += 5;
    if (bits >= 8) { bits -= 8; result[at++] = (value >>> bits) & 255; value &= (1 << bits) - 1; }
  }
  return result;
}

function otpAt(key: Buffer, step: number): string {
  const counter = Buffer.alloc(8);
  counter.writeBigUInt64BE(BigInt(step));
  const digest = createHmac("sha1", key).update(counter).digest();
  const offset = digest[digest.length - 1]! & 15;
  const binary = (digest.readUInt32BE(offset) & 0x7fffffff) % 1_000_000;
  counter.fill(0); digest.fill(0);
  return String(binary).padStart(6, "0");
}

export function totpCodeForTest(seed: string, milliseconds: number): string {
  const key = base32Decode(seed);
  try { return otpAt(key, Math.floor(milliseconds / 30_000)); }
  finally { key.fill(0); }
}

export function loadTotpVerifier(secretsPath: string, replayPath: string) {
  const secrets: Secrets = new Map();
  const lastUsed = new Map<string, number>();
  const seedFd = privateFile(secretsPath, constants.O_RDONLY | constants.O_NONBLOCK);
  try {
    if (fstatSync(seedFd).size > 16_384) throw new Error("MFA enrollment file too large");
    const body: unknown = JSON.parse(readFileSync(seedFd, "utf8"));
    if (!body || typeof body !== "object" || Array.isArray(body)) throw new Error("Invalid MFA enrollment");
    const root = body as Record<string, unknown>;
    if (root.schema !== 1 || !root.accounts || typeof root.accounts !== "object" ||
      Array.isArray(root.accounts) || Object.keys(root).length !== 2) throw new Error("Invalid MFA enrollment");
    const accounts = root.accounts as Record<string, unknown>;
    if (Object.keys(accounts).length < 1 || Object.keys(accounts).length > 64) throw new Error("Invalid MFA accounts");
    const distinct = new Set<string>();
    for (const [actor, value] of Object.entries(accounts)) {
      if (!actorPattern.test(actor) || typeof value !== "string") throw new Error("Invalid MFA actor");
      if (distinct.has(value)) throw new Error("MFA secrets must differ by account");
      distinct.add(value);
      secrets.set(actor, base32Decode(value));
    }
  } catch (error) {
    for (const key of secrets.values()) key.fill(0);
    throw error;
  } finally { closeSync(seedFd); }

  let ledgerFd: number;
  try { ledgerFd = privateFile(replayPath, constants.O_RDWR | constants.O_APPEND | constants.O_CREAT, true); }
  catch (error) { for (const key of secrets.values()) key.fill(0); throw error; }
  try {
    visitReplayLedger(ledgerFd, ({ actor, step }) => {
      lastUsed.set(actor, Math.max(lastUsed.get(actor) ?? -1, step));
    });
  } catch (error) { closeSync(ledgerFd); for (const key of secrets.values()) key.fill(0); throw error; }

  const verify = (actor: string, code: string, now: number): boolean => {
    const secret = secrets.get(actor);
    if (!secret || !/^\d{6}$/.test(code) || !Number.isSafeInteger(now) || now < 0) return false;
    const current = Math.floor(now / 30_000);
    // Compare all supported steps before deciding, including a replayed code.
    let accepted = -1;
    const presented = Buffer.from(code, "ascii");
    for (const delta of [-1, 0, 1]) {
      const step = current + delta;
      if (step < 0) continue;
      const expected = Buffer.from(otpAt(secret, step), "ascii");
      if (timingSafeEqual(expected, presented) && step > accepted) accepted = step;
      expected.fill(0);
    }
    presented.fill(0);
    if (accepted <= (lastUsed.get(actor) ?? -1)) return false;
    // A verifier may have opened this ledger before another instance used the
    // code. Re-read the on-disk records immediately before attempting a write.
    let previouslyUsed = lastUsed.get(actor) ?? -1;
    visitReplayLedger(ledgerFd, ({ actor: usedActor, step }) => {
      if (usedActor === actor) previouslyUsed = Math.max(previouslyUsed, step);
    });
    lastUsed.set(actor, previouslyUsed);
    if (accepted <= previouslyUsed) return false;

    // Local-filesystem O_APPEND writes have a single append order. The unique
    // marker lets concurrent verifiers distinguish their own record from an
    // earlier accepted step, even if both passed the pre-write check.
    const nonce = randomBytes(16).toString("hex");
    const line = JSON.stringify({ actor, step: accepted, nonce }) + "\n";
    if (fstatSync(ledgerFd).size + Buffer.byteLength(line) > ledgerLimit)
      throw new Error("MFA replay ledger full");
    const bytes = Buffer.from(line, "utf8");
    try {
      if (writeSync(ledgerFd, bytes) !== bytes.length) throw new Error("MFA replay persistence failed");
      fsyncSync(ledgerFd);
      let ownRecordSeen = false;
      let earlierStep = -1;
      visitReplayLedger(ledgerFd, (entry) => {
        if (entry.nonce === nonce) {
          if (ownRecordSeen || entry.actor !== actor || entry.step !== accepted)
            throw new Error("Invalid MFA replay marker");
          ownRecordSeen = true;
        } else if (!ownRecordSeen && entry.actor === actor) {
          earlierStep = Math.max(earlierStep, entry.step);
        }
      });
      if (!ownRecordSeen) throw new Error("Missing MFA replay marker");
      lastUsed.set(actor, accepted);
      return earlierStep < accepted;
    } finally { bytes.fill(0); }
  };
  const close = () => { closeSync(ledgerFd); for (const key of secrets.values()) key.fill(0); secrets.clear(); };
  return { verify, close };
}
