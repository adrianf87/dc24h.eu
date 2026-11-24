// Stage IV DESIGN-ONLY prototype. Never imported by production gateway,
// startup, archive tooling or browser. No audit rollover is enabled.
import { createHash, createHmac, timingSafeEqual } from "node:crypto";
import { remoteAuditEvents } from "../server/remote-audit.ts";

const zero = "0".repeat(64);
const events = new Set<string>(remoteAuditEvents);
// Legacy audit records and archive bytes use an unkeyed SHA-256 checksum.
// The detached Owner key must never flow into this fast, unkeyed digest.
const auditSha256 = (bytes: Buffer | string) => createHash("sha256").update(bytes).digest("hex");

// A public, domain-separated identifier for a high-entropy 32-byte Owner key.
// This is a keyed tag, not a password hash or password-based key derivation.
const ownerKeyIdentifier = (ownerKey: Buffer) =>
  createHmac("sha256", ownerKey)
    .update("dc24h/audit-checkpoint/key-id/v1", "utf8")
    .digest("hex")
    .slice(0, 32);

export type ProposedCheckpoint = {
  schema: 1; algorithm: "HMAC-SHA256-DESIGN-ONLY"; key_id: string; generation: number;
  previous_checkpoint: string; archive_sha256: string; first_sequence: number;
  last_sequence: number; tail_digest: string; mac: string;
};
type Unsigned = Omit<ProposedCheckpoint, "mac">;

function auditSegment(raw: Buffer, first: number, previous: string) {
  if (raw.length === 0 || raw.length > 8 * 1024 * 1024 || raw.at(-1) !== 10)
    throw Error("Archive empty, truncated or over capacity");
  const lines = raw.toString("utf8").slice(0, -1).split("\n");
  let sequence = first - 1, tail = previous;
  for (const line of lines) {
    const item: unknown = JSON.parse(line);
    if (!item || typeof item !== "object" || Array.isArray(item)) throw Error("Invalid audit entry");
    const row = item as Record<string, unknown>;
    if (Object.keys(row).sort().join(",") !== "at,digest,event,previous,schema,sequence" ||
        row.schema !== 1 || row.sequence !== ++sequence || row.previous !== tail ||
        !events.has(row.event as string) || typeof row.at !== "string" ||
        !/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/.test(row.at) ||
        !Number.isFinite(Date.parse(row.at)) || new Date(row.at).toISOString() !== row.at ||
        typeof row.digest !== "string" || !/^[0-9a-f]{64}$/.test(row.digest) ||
        row.digest !== auditSha256(JSON.stringify([1, sequence, tail, row.event, row.at])))
      throw Error("Audit segment does not continue verified chain");
    tail = row.digest;
  }
  return { last: sequence, tail };
}

function validateKey(key: Buffer): void {
  if (key.length !== 32) throw Error("Detached Owner key must have 32 bytes");
}

function unsignedCheckpoint(proof: ProposedCheckpoint): Unsigned {
  const { mac: _mac, ...value } = proof;
  return value;
}

const checkpointHash = (proof: ProposedCheckpoint) => auditSha256(JSON.stringify(proof));
const checkpointMac = (key: Buffer, unsigned: Unsigned) =>
  createHmac("sha256", key).update(JSON.stringify(unsigned)).digest("hex");

export function proposeAuditCheckpoint(rawArchive: Buffer, ownerKey: Buffer, previous?: ProposedCheckpoint): ProposedCheckpoint {
  validateKey(ownerKey);
  const first = previous ? previous.last_sequence + 1 : 1;
  const segment = auditSegment(rawArchive, first, previous?.tail_digest ?? zero);
  const value: Unsigned = {
    schema: 1, algorithm: "HMAC-SHA256-DESIGN-ONLY", key_id: ownerKeyIdentifier(ownerKey),
    generation: previous ? previous.generation + 1 : 1,
    previous_checkpoint: previous ? checkpointHash(previous) : zero,
    archive_sha256: auditSha256(rawArchive), first_sequence: first,
    last_sequence: segment.last, tail_digest: segment.tail,
  };
  return { ...value, mac: checkpointMac(ownerKey, value) };
}

export function verifyProposedCheckpoint(rawArchive: Buffer, proof: ProposedCheckpoint,
  ownerKey: Buffer, trustedLastGeneration: number, previous?: ProposedCheckpoint): void {
  validateKey(ownerKey);
  if (!Number.isSafeInteger(trustedLastGeneration) || trustedLastGeneration < 0 ||
      !proof || Object.keys(proof).sort().join(",") !==
        "algorithm,archive_sha256,first_sequence,generation,key_id,last_sequence,mac,previous_checkpoint,schema,tail_digest" ||
      !Number.isSafeInteger(proof.generation) || proof.generation < trustedLastGeneration ||
      !Number.isSafeInteger(proof.first_sequence) || !Number.isSafeInteger(proof.last_sequence) ||
      proof.first_sequence < 1 || proof.last_sequence < proof.first_sequence ||
      proof.schema !== 1 || proof.algorithm !== "HMAC-SHA256-DESIGN-ONLY" ||
      proof.key_id !== ownerKeyIdentifier(ownerKey) ||
      !/^[0-9a-f]{64}$/.test(proof.mac))
    throw Error("Invalid or rolled-back checkpoint");
  const expected = checkpointMac(ownerKey, unsignedCheckpoint(proof));
  if (!timingSafeEqual(Buffer.from(expected, "hex"), Buffer.from(proof.mac, "hex")))
    throw Error("Forged or wrong-key checkpoint");
  const first = previous ? previous.last_sequence + 1 : 1;
  if (proof.generation !== (previous ? previous.generation + 1 : 1) ||
      proof.previous_checkpoint !== (previous ? checkpointHash(previous) : zero) ||
      proof.first_sequence !== first || proof.archive_sha256 !== auditSha256(rawArchive))
    throw Error("Missing, foreign or replayed archive checkpoint");
  const segment = auditSegment(rawArchive, first, previous?.tail_digest ?? zero);
  if (segment.last !== proof.last_sequence || segment.tail !== proof.tail_digest)
    throw Error("Audit continuity lost");
}
