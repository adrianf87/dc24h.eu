// Fingerprint a bounded, already-validated private file using an O_NOFOLLOW
// descriptor. Metadata alone cannot identify same-length in-place edits when
// an outside writer restores the previous mtime. This is an in-process
// consistency guard, NOT an offline authenticity signature or a recovery MAC.
import { createHash } from "node:crypto";
import { closeSync, constants, fstatSync, readFileSync } from "node:fs";
import type { Stats } from "node:fs";
import { lstatPrivateLeaf, openPrivateLeaf } from "./remote-private-path.ts";

const same = (a: Stats, b: Stats): boolean =>
  a.isFile() && b.isFile() && a.dev === b.dev && a.ino === b.ino &&
  a.size === b.size && a.mtimeMs === b.mtimeMs && a.ctimeMs === b.ctimeMs &&
  a.uid === b.uid && a.gid === b.gid && a.mode === b.mode && a.nlink === b.nlink;

export const fingerprint = (bytes: Buffer | string): string =>
  createHash("sha256").update(bytes).digest("hex");

export function fingerprintPrivateFile(path: string, identity: Stats): string {
  const fd = openPrivateLeaf(path, constants.O_RDONLY | constants.O_NONBLOCK);
  try {
    const before = fstatSync(fd);
    if (!same(identity, before)) throw Error("Private catalog changed outside gateway");
    const bytes = readFileSync(fd);
    try {
      if (bytes.length !== identity.size || !same(identity, fstatSync(fd)) ||
          !same(identity, lstatPrivateLeaf(path)))
        throw Error("Private catalog changed during read");
      return fingerprint(bytes);
    } finally { bytes.fill(0); }
  } finally { closeSync(fd); }
}
