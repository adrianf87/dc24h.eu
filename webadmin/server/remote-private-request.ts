// Offline operator requests can contain MFA secrets. Read them only through a
// pinned owner-private path, including every ancestor, and reject replaced
// leaves instead of following a symlink in an intermediate directory.
import { closeSync, constants, fstatSync, readFileSync } from "node:fs";
import type { Stats } from "node:fs";
import { lstatPrivateLeaf, openPrivateLeaf } from "./remote-private-path.ts";

const safe = (s: Stats, limit: number): boolean =>
  s.isFile() && s.uid === process.geteuid?.() && s.nlink === 1 &&
  !(s.mode & 0o077) && s.size >= 2 && s.size <= limit;

const unchanged = (a: Stats, b: Stats): boolean =>
  a.dev === b.dev && a.ino === b.ino && a.size === b.size &&
  a.mtimeMs === b.mtimeMs && a.ctimeMs === b.ctimeMs &&
  a.mode === b.mode && a.uid === b.uid && a.nlink === b.nlink;

export function readPrivateJsonRequest(path: string, limit: number): unknown {
  if (!Number.isSafeInteger(limit) || limit < 2) throw Error("Invalid private request limit");
  const before = lstatPrivateLeaf(path);
  if (!safe(before, limit)) throw Error("Unsafe private request");
  // O_NONBLOCK also prevents a substituted FIFO/device from hanging an
  // offline recovery command before its descriptor can be validated.
  const fd = openPrivateLeaf(path, constants.O_RDONLY | constants.O_NONBLOCK);
  try {
    const opened = fstatSync(fd);
    if (!safe(opened, limit) || !unchanged(before, opened)) throw Error("Unsafe private request");
    const bytes = readFileSync(fd);
    try {
      if (bytes.length !== opened.size || !unchanged(opened, fstatSync(fd)) ||
          !unchanged(opened, lstatPrivateLeaf(path))) throw Error("Private request changed during read");
      return JSON.parse(bytes.toString("utf8")) as unknown;
    } finally { bytes.fill(0); }
  } finally { closeSync(fd); }
}
