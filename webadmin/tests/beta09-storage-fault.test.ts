import assert from "node:assert/strict";
import { spawnSync, type SpawnSyncReturns } from "node:child_process";
import { constants, closeSync, existsSync, fsyncSync, mkdtempSync, openSync,
  readFileSync, rmSync, statfsSync, writeSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { test } from "node:test";
import { initializeRemoteAuditStore, openRemoteAuditStore } from "../server/remote-audit-store.ts";
import { initializeIncidentJournal } from "../server/remote-incident-journal.ts";

const childLoader = process.execArgv.includes("--experimental-strip-types") ?
  ["--experimental-strip-types"] : ["--import", "tsx"];

test("SIGKILL immediately after fsync preserves the exact audit chain on restart", () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-beta09-crash-"));
  const path = join(dir, "audit.jsonl");
  try {
    initializeRemoteAuditStore(path);
    const child = spawnSync(process.execPath,
      [...childLoader, "tests/beta09-audit-fault-child.ts", "crash-after-fsync", path],
      { cwd: resolve("."), timeout: 10_000, encoding: "utf8" });
    assert.equal(child.signal, "SIGKILL", child.stderr);
    const reopened = openRemoteAuditStore(path);
    assert.equal(reopened.verify(), true);
    assert.equal(reopened.health().sequence, 1);
    reopened.append("login_refused", 1_700_000_000_001);
    assert.equal(reopened.health().sequence, 2);
    reopened.close();
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

const mount = process.env.DC24H_BETA09_DISPOSABLE_TMPFS;
test("real disposable tmpfs ENOSPC refuses a durable audit append and locks the writer",
  { skip: !mount }, () => {
    assert.ok(mount);
    const sentinel = join(mount, ".dc24h-beta09-disposable");
    assert.equal(readFileSync(sentinel, "utf8"), "dc24h-beta09-disposable\n");
    assert.equal(statfsSync(mount).type, 0x01021994, "refuse to fill a non-tmpfs filesystem");
    const dir = mkdtempSync(join(mount, "audit-enospc-"));
    const path = join(dir, "audit.jsonl"), filler = join(dir, "filler.bin");
    let fd: number | undefined;
    let audit: ReturnType<typeof openRemoteAuditStore> | undefined;
    try {
      initializeRemoteAuditStore(path);
      audit = openRemoteAuditStore(path);
      fd = openSync(filler, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL, 0o600);
      const block = Buffer.alloc(4096);
      let exhausted = false;
      for (let i = 0; i < 8192; i++) {
        try {
          const written = writeSync(fd, block);
          if (written !== block.length) throw Error("Unexpected short fill write");
        } catch (error) {
          if ((error as NodeJS.ErrnoException).code !== "ENOSPC") throw error;
          exhausted = true;
          break;
        }
      }
      assert.equal(exhausted, true, "test requires a real kernel ENOSPC, not a simulated limit");
      assert.throws(() => audit!.append("login_refused", 1_700_000_000_000), /ENOSPC/);
      assert.deepEqual(audit.health(), { healthy: false, dropped: 1, sequence: 0 });
      assert.throws(() => audit!.append("login_ok", 1_700_000_000_001), /unavailable/);
    } finally {
      if (fd !== undefined) { try { fsyncSync(fd); } catch { /* Full volume is expected. */ } closeSync(fd); }
      audit?.close();
      rmSync(dir, { recursive: true, force: true });
    }
  });

const shim = process.env.DC24H_BETA09_FSYNC_SHIM;
test("injected kernel-facing fsync EIO denies audit success without retries",
  { skip: !shim }, () => {
    assert.ok(shim && existsSync(shim));
    const dir = mkdtempSync(join(tmpdir(), "dc24h-beta09-eio-"));
    const path = join(dir, "audit.jsonl");
    try {
      initializeRemoteAuditStore(path);
      const child = spawnSync(process.execPath,
        [...childLoader, "tests/beta09-audit-fault-child.ts", "fsync-error", path],
        { cwd: resolve("."), timeout: 10_000, encoding: "utf8", env: { ...process.env,
          LD_PRELOAD: shim, DC24H_BETA09_FSYNC_TARGET: path } });
      assert.equal(child.status, 0, child.stderr);
      assert.match(child.stdout, /writer locked/);
      // The failed fsync may still leave bytes in cache. Treat that as an
      // uncertain audit generation, not proof of durable success.
    } finally { rmSync(dir, { recursive: true, force: true }); }
  });

test("fsync EIO during archive and incident partial writes never commits a false success",
  { skip: !shim }, () => {
    assert.ok(shim && existsSync(shim));
    for (const mode of ["archive-eio", "incident-eio"] as const) {
      const dir = mkdtempSync(join(tmpdir(), "dc24h-beta09-private-eio-"));
      const path = join(dir, mode === "archive-eio" ? "audit.jsonl" : "incidents.json");
      try {
        if (mode === "archive-eio") {
          initializeRemoteAuditStore(path);
          const writer = openRemoteAuditStore(path);
          writer.append("login_refused", 1_700_000_000_000);
          writer.close();
        } else initializeIncidentJournal(path);
        const prefix = mode === "archive-eio" ? path + ".archive.partial-" : path + ".partial-";
        const child: SpawnSyncReturns<string> = spawnSync(process.execPath,
          [...childLoader, "tests/beta09-private-fault-child.ts", mode, path],
          { cwd: resolve("."), timeout: 10_000, encoding: "utf8", env: { ...process.env,
            LD_PRELOAD: shim, DC24H_BETA09_FSYNC_TARGET_PREFIX: prefix } });
        assert.equal(child.status, 0, child.stderr);
        assert.match(child.stdout, /private partial fsync EIO refused/);
      } finally { rmSync(dir, { recursive: true, force: true }); }
    }
  });
