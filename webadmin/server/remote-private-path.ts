// Shared, Linux-only validation for owner-private gateway catalogs. A leaf
// O_NOFOLLOW alone does not protect an intermediate directory rename.
import { closeSync, constants, fstatSync, fsyncSync, lstatSync, openSync, renameSync, unlinkSync } from "node:fs";
import type { Stats } from "node:fs";
import { basename, dirname, isAbsolute, normalize, sep } from "node:path";

export function assertPrivateDirectory(directory: string): void {
  if (!isAbsolute(directory)) throw Error("Absolute private state path required");
  // Different spellings of the same state directory conceal path aliases.
  // Reject redundant separators and trailing slashes before inode traversal.
  if (normalize(directory) !== directory || (directory !== sep && directory.endsWith(sep)))
    throw Error("Noncanonical private state path");
  const components = directory.split(sep).filter(Boolean);
  if (components.some(part => part === "." || part === ".."))
    throw Error("Noncanonical private state path");
  let current: string = sep;
  const fingerprints: Array<{ path: string; dev: number; ino: number }> = [];
  for (const component of components) {
    current = current === sep ? sep + component : current + sep + component;
    const stat = lstatSync(current);
    if (!stat.isDirectory()) throw Error("Symlink or non-directory in private state path");
    fingerprints.push({ path: current, dev: stat.dev, ino: stat.ino });
  }
  // Confirm traversal did not change underneath the initial checks. The
  // calling service still requires stopped writers for an offline restore.
  for (const part of fingerprints) {
    const stat = lstatSync(part.path);
    if (!stat.isDirectory() || stat.dev !== part.dev || stat.ino !== part.ino)
      throw Error("Private state directory changed during validation");
  }
  const leaf = lstatSync(directory);
  if (!leaf.isDirectory() || leaf.uid !== process.geteuid?.() || (leaf.mode & 0o077))
    throw Error("Unsafe private state directory");
}

export function assertPrivateParent(file: string): void {
  if (!isAbsolute(file)) throw Error("Absolute private file path required");
  assertPrivateDirectory(dirname(file));
}

function checkLeaf(file: string): void {
  if (!isAbsolute(file) || normalize(file) !== file || file.endsWith(sep) ||
      basename(file) === "." || basename(file) === "..")
    throw Error("Noncanonical private file path");
}

// Linux procfs /proc/self/fd/<directory-fd>/<single-filename> provides a
// descriptor-relative path for fs functions without exposing a public
// listener or using unstable process.binding syscall interfaces. It is NOT a
// claim of a portable Node openat2 binding; absent /proc must fail closed.
export function withPinnedPrivateParent<T>(file: string, action: (leaf: string, directoryFd: number) => T): T {
  if (process.platform !== "linux") throw Error("Linux private directory FDs required");
  checkLeaf(file);
  const directory = dirname(file);
  assertPrivateDirectory(directory);
  const initial = lstatSync(directory);
  const dirFd = openSync(directory, constants.O_RDONLY | constants.O_DIRECTORY | constants.O_NOFOLLOW);
  const same = (stat: Stats): boolean => stat.isDirectory() && stat.dev === initial.dev &&
    stat.ino === initial.ino && stat.uid === initial.uid && !(stat.mode & 0o077);
  try {
    if (!same(fstatSync(dirFd))) throw Error("Private directory changed during pinned open");
    assertPrivateDirectory(directory);
    if (!same(lstatSync(directory))) throw Error("Private directory alias changed during pinned open");
    const result = action(`/proc/self/fd/${dirFd}/${basename(file)}`, dirFd);
    assertPrivateDirectory(directory);
    if (!same(lstatSync(directory))) throw Error("Private directory alias changed during operation");
    return result;
  } finally { closeSync(dirFd); }
}

export function openPrivateLeaf(file: string, flags: number, mode?: number): number {
  let opened: number | undefined;
  try {
    return withPinnedPrivateParent(file, leaf => {
      opened = openSync(leaf, flags | constants.O_NOFOLLOW, mode);
      return opened;
    });
  } catch (error) {
    if (opened !== undefined) closeSync(opened);
    throw error;
  }
}

export const lstatPrivateLeaf = (file: string): Stats =>
  withPinnedPrivateParent(file, leaf => lstatSync(leaf));

export function renamePrivateLeaf(from: string, to: string): void {
  checkLeaf(from);
  checkLeaf(to);
  if (dirname(from) !== dirname(to)) throw Error("Private rename must remain in one directory");
  withPinnedPrivateParent(from, (source, dirFd) => {
    renameSync(source, `/proc/self/fd/${dirFd}/${basename(to)}`);
  });
}

export const unlinkPrivateLeaf = (file: string): void =>
  withPinnedPrivateParent(file, leaf => unlinkSync(leaf));

export const syncPrivateParent = (file: string): void =>
  withPinnedPrivateParent(file, (_leaf, dirFd) => fsyncSync(dirFd));
