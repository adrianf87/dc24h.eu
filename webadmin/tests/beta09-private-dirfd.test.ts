import assert from "node:assert/strict";
import { test } from "node:test";
import { chmodSync, closeSync, constants, mkdirSync, mkdtempSync, openSync, readFileSync,
  renameSync, rmSync, symlinkSync, writeFileSync, writeSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { openPrivateLeaf, renamePrivateLeaf, syncPrivateParent,
  withPinnedPrivateParent } from "../server/remote-private-path.ts";

test("Linux private files open through a directory FD, not an ancestor alias", () => {
  const root = mkdtempSync(join(tmpdir(), "beta09-dirfd-")); chmodSync(root, 0o700);
  try {
    const real = join(root, "real"); mkdirSync(real, { mode: 0o700 });
    const state = join(real, "state"); mkdirSync(state, { mode: 0o700 });
    const decoy = join(root, "decoy"); mkdirSync(decoy, { mode: 0o700 });
    const file = join(state, "mfa.json");
    writeFileSync(file, "original\n", { mode: 0o600 });
    writeFileSync(join(decoy, "mfa.json"), "foreign!\n", { mode: 0o600 });
    const moved = join(root, "moved");
    assert.throws(() => withPinnedPrivateParent(file, pinned => {
      // Model a hostile ancestor replacement *after* acquiring the FD, but
      // before the actual leaf open. The FD must continue to refer to real.
      renameSync(real, moved);
      symlinkSync(decoy, real, "dir");
      const fd = openSync(pinned, constants.O_RDONLY | constants.O_NOFOLLOW);
      try { assert.equal(readFileSync(fd, "utf8"), "original\n"); }
      finally { closeSync(fd); }
    }), /Symlink/);
    assert.equal(readFileSync(join(moved, "state", "mfa.json"), "utf8"), "original\n");
    assert.equal(readFileSync(join(decoy, "mfa.json"), "utf8"), "foreign!\n");
    assert.throws(() => openPrivateLeaf(file, constants.O_RDONLY), /Symlink/);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test("descriptor-relative create, rename and parent fsync preserve private state", () => {
  const root = mkdtempSync(join(tmpdir(), "beta09-dirfd-write-")); chmodSync(root, 0o700);
  try {
    const original = join(root, "passkeys.json");
    const tmp = join(root, ".passkeys.tmp");
    writeFileSync(original, "old\n", { mode: 0o600 });
    const fd = openPrivateLeaf(tmp, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL, 0o600);
    try { assert.equal(writeSync(fd, Buffer.from("new\n")), 4); }
    finally { closeSync(fd); }
    renamePrivateLeaf(tmp, original);
    syncPrivateParent(original);
    assert.equal(readFileSync(original, "utf8"), "new\n");
    assert.throws(() => renamePrivateLeaf(root + "/../invalid", original), /Noncanonical/);
    assert.throws(() => openPrivateLeaf(original + "/", constants.O_RDONLY), /Noncanonical/);
  } finally { rmSync(root, { recursive: true, force: true }); }
});
