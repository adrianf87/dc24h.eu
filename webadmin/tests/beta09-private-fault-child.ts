// Disposable-child only: inject fsync EIO on the private *partial* file.
// No raw payload, actor, token, path or OS error detail is sent to CI output.
import assert from "node:assert/strict";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname } from "node:path";
import { openRemoteAuditStore } from "../server/remote-audit-store.ts";
import { archiveStoppedRemoteAudit } from "../server/remote-audit-archive.ts";
import { openIncidentJournal } from "../server/remote-incident-journal.ts";

const [mode, path] = process.argv.slice(2);
if (!path || !["archive-eio", "incident-eio"].includes(mode ?? "")) throw Error("Invalid private fault case");
const before = readFileSync(path);
if (mode === "archive-eio") {
  const destination = path + ".archive";
  assert.equal(process.env.DC24H_BETA09_FSYNC_TARGET_PREFIX, destination + ".partial-");
  assert.throws(() => archiveStoppedRemoteAudit(path, destination, "OWNER_STOPPED_WRITER_ACK"), /EIO/);
  assert.equal(existsSync(destination), false, "failed archive must not look complete");
  assert.ok(!readdirSync(dirname(path)).some(name => name.startsWith("audit.jsonl.archive.partial-")));
  assert.deepEqual(readFileSync(path), before, "failed archive must preserve the source chain");
  const recovered = openRemoteAuditStore(path);
  assert.equal(recovered.verify(), true);
  recovered.close();
} else {
  assert.equal(process.env.DC24H_BETA09_FSYNC_TARGET_PREFIX, path + ".partial-");
  const journal = openIncidentJournal(path, () => 1_700_000_000_000);
  assert.throws(() => journal.append("hub_started"), /EIO/);
  assert.deepEqual(journal.read(), { state: "unknown", events: [] }, "failed telemetry must stay unavailable");
  assert.ok(!readdirSync(dirname(path)).some(name => name.startsWith("incidents.json.partial-")));
  assert.deepEqual(readFileSync(path), before);
  assert.deepEqual(openIncidentJournal(path, () => 1_700_000_000_000).read(),
    { state: "available", events: [] }, "a fresh process may inspect the unmodified journal");
}
process.stdout.write("private partial fsync EIO refused without committing a false success\n");
