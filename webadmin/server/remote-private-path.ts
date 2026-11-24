// Shared, Linux-only validation for the owner-private remote gateway catalogs.
// O_NOFOLLOW on a leaf does not protect intermediate path components.
import { lstatSync } from "node:fs";
import { dirname, isAbsolute, normalize, sep } from "node:path";

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
