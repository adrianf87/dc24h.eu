import {
  closeSync, constants, fchmodSync, fstatSync, fsyncSync, linkSync, lstatSync,
  mkdtempSync, openSync, readFileSync, rmSync, writeFileSync,
} from "node:fs";
import { dirname, join } from "node:path";
import { PUBLIC_WEBADMIN_ORIGIN, UBUNTU_WEBADMIN_ORIGIN } from "./public-email-code.ts";

export const STAGE4_APACHE_SITE = "/etc/apache2/sites-available/dc24h-stage4-webadmin.conf";
type Stage4Origin = typeof PUBLIC_WEBADMIN_ORIGIN | typeof UBUNTU_WEBADMIN_ORIGIN;

export function approvedStage4Origin(port: string): Stage4Origin | undefined {
  return ([PUBLIC_WEBADMIN_ORIGIN, UBUNTU_WEBADMIN_ORIGIN] as const)
    .find(origin => new URL(origin).port === port);
}

function siteContract(origin: Stage4Origin): ReadonlyArray<readonly [RegExp, readonly string[]]> {
  const { hostname, port } = new URL(origin);
  const authority = hostname.replaceAll(".", "\\.") + ":" + port;
  const originRule = `RewriteCond %{HTTP:Origin} !^https://${authority}$`;
  return [
    [/^Listen\b/i, [`Listen ${port}`]],
    [/^<VirtualHost\b/i, [`<VirtualHost *:${port}>`]],
    [/^ServerName\b/i, [`ServerName ${hostname}`]],
    [/^SSLEngine\b/i, ["SSLEngine On"]],
    [/^RewriteCond %{HTTP_HOST}(?: |$)/i, [`RewriteCond %{HTTP_HOST} !^${authority}$`]],
    [/^RewriteCond %{HTTP:Origin}(?: |$)/i, ["RewriteCond %{HTTP:Origin} !^$", originRule, originRule]],
  ];
}

function assertSiteContract(text: string, origin: Stage4Origin): void {
  const lines = text.split(/\r?\n/).map(line => line.trim().replace(/[ \t]+/g, " "));
  for (const [directive, expected] of siteContract(origin)) {
    const actual = lines.filter(line => directive.test(line));
    if (actual.length !== expected.length || actual.some((line, index) => line !== expected[index]))
      throw Error("Stage IV Apache site does not match the selected HTTPS Origin");
  }
}

export function checkStage4ApacheSite(origin: Stage4Origin, site = STAGE4_APACHE_SITE): boolean {
  let file: number;
  try {
    file = openSync(site, constants.O_RDONLY | constants.O_NOFOLLOW | constants.O_NONBLOCK);
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") return false;
    throw Error("Unsafe Stage IV Apache site");
  }
  try {
    const stat = fstatSync(file);
    if (!stat.isFile() || stat.uid !== process.geteuid?.() || stat.nlink !== 1 ||
        (stat.mode & 0o022) || stat.size > 256 * 1024)
      throw Error("Unsafe Stage IV Apache site");
    assertSiteContract(readFileSync(file, "utf8"), origin);
    return true;
  } finally {
    closeSync(file);
  }
}

export function prepareStage4ApacheSite(
  origin: Stage4Origin, template: string, publish: boolean, site = STAGE4_APACHE_SITE,
): "preserved" | "ready" | "created" {
  const parent = dirname(site);
  const directory = lstatSync(parent);
  if (!directory.isDirectory() || directory.uid !== process.geteuid?.() || (directory.mode & 0o022))
    throw Error("Unsafe Stage IV Apache site directory");
  if (checkStage4ApacheSite(origin, site)) return "preserved";

  const text = readFileSync(template, "utf8");
  assertSiteContract(text, PUBLIC_WEBADMIN_ORIGIN);
  const selected = siteContract(origin);
  const replacements = new Map(siteContract(PUBLIC_WEBADMIN_ORIGIN).flatMap(([, lines], index) =>
    lines.map((line, offset) => [line, selected[index]![1][offset]!] as const)));
  const rendered = text.split("\n").map(line => {
    const replacement = replacements.get(line.trim().replace(/[ \t]+/g, " "));
    return replacement === undefined ? line : line.replace(line.trim(), replacement);
  }).join("\n");
  assertSiteContract(rendered, origin);
  if (!publish) return "ready";

  // Publish a complete file on the same filesystem. A concurrent install or
  // operator action that fills the destination cannot be overwritten.
  const staging = mkdtempSync(join(parent, ".dc24h-stage4-"));
  try {
    const candidate = join(staging, "site.conf");
    const file = openSync(candidate, "wx", 0o600);
    try {
      writeFileSync(file, rendered, "utf8");
      fchmodSync(file, 0o644);
      fsyncSync(file);
    } finally {
      closeSync(file);
    }
    try {
      linkSync(candidate, site);
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code === "EEXIST" && checkStage4ApacheSite(origin, site))
        return "preserved";
      throw error;
    }
    return "created";
  } finally {
    rmSync(staging, { recursive: true });
  }
}
