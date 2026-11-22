// Shared strict content-type policy for every private C++ API adapter.
// Do not accept lookalike prefixes, other charsets or multiple media types.
export const isJsonContentType = (value: unknown): boolean =>
  typeof value === "string" &&
  /^application\/json(?:\s*;\s*charset\s*=\s*(?:utf-8|"utf-8"))?\s*$/i.test(value);
