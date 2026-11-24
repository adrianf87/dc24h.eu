import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { aggregateObservability } from "../server/remote-observability.ts";
import { initializeIncidentJournal, openIncidentJournal } from "../server/remote-incident-journal.ts";
import type { RemoteStatusProjection } from "../src/lib/status.ts";

const sample = (running: boolean): RemoteStatusProjection => ({
  observation: { schema: 1, source_age_ms: 5 },
  server: { version: "beta-0.7.20", running, uptime_seconds: 30 },
  network: { active_connections: 1, accepted_connections: 2, security_rejected: 0 },
  users: { online: 1, peak_online: 1 }, chat: { received: 0, rejected: 0 },
});

test("owner-only aggregate journal survives restart while live state remains unknown", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-incident-"));
  const path = join(dir, "incidents.json");
  let wall = 1_700_000_000_000, mono = 1_000;
  try {
    initializeIncidentJournal(path);
    assert.throws(() => initializeIncidentJournal(path), /EEXIST/);
    const persisted = openIncidentJournal(path, () => wall);
    const first = aggregateObservability(() => mono, persisted);
    first.observe(sample(true));
    wall += 1_000; mono += 1_000; first.observe(sample(false));
    assert.deepEqual(first.read().events, [{ kind: "hub_stopped", age_seconds: 0 }]);
    assert.deepEqual(first.read().retained, { state: "available", events: [{ kind: "hub_stopped", age_seconds: 0 }] });

    const reopened = openIncidentJournal(path, () => wall);
    const restarted = aggregateObservability(() => mono, reopened);
    const before = restarted.read();
    assert.equal(before.state, "unknown", "a journal cannot certify a new gateway process");
    assert.deepEqual(before.events, []);
    assert.deepEqual(before.retained?.events, [{ kind: "hub_stopped", age_seconds: 0 }]);
    restarted.observe(sample(true));
    assert.equal(restarted.read().state, "observed");
    assert.deepEqual(restarted.read().events, [], "first new C++ sample is only a baseline");
    restarted.auditLost();
    assert.deepEqual(restarted.read().retained, { state: "unknown", events: [] });
    assert.ok(!readFileSync(path, "utf8").includes("actor"));
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("journal bounds size, kinds and ten-minute retention without private identities", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-incident-cap-"));
  const path = join(dir, "incidents.json");
  let wall = 1_700_000_000_000;
  try {
    initializeIncidentJournal(path);
    const store = openIncidentJournal(path, () => wall);
    assert.throws(() => store.append("password:private" as "hub_started"), /unavailable/);
    for (let n = 0; n < 30; n++) { wall += 1_000; store.append("hub_started"); }
    assert.equal(store.read().events.length, 12);
    assert.ok(readFileSync(path).length < 4096);
    wall += 600_001;
    assert.deepEqual(store.read(), { state: "available", events: [] });
    wall -= 100;
    assert.deepEqual(store.read(), { state: "unknown", events: [] }, "backwards clock fails closed");
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("journal rejects corrupted, public and aliased files without overwriting them", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-incident-safe-"));
  const path = join(dir, "incidents.json");
  try {
    initializeIncidentJournal(path);
    const store = openIncidentJournal(path);
    chmodSync(path, 0o644);
    assert.deepEqual(store.read(), { state: "unknown", events: [] });
    assert.throws(() => openIncidentJournal(path));
    chmodSync(path, 0o600);
    writeFileSync(path, '{"schema":1,"events":[{"kind":"login_refused","at_unix_ms":0,"actor":"secret"}]}');
    assert.throws(() => openIncidentJournal(path), /Incident event invalid/);
    const real = join(dir, "real"), alias = join(dir, "alias");
    mkdirSync(real, { mode: 0o700 });
    symlinkSync(real, alias);
    assert.throws(() => initializeIncidentJournal(join(alias, "secret.json")), /Symlink|ancestor/);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});
