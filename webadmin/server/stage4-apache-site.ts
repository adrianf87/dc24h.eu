import {
  closeSync, constants, fchmodSync, fstatSync, fsyncSync, linkSync, lstatSync,
  mkdtempSync, openSync, readFileSync, rmSync, writeFileSync,
} from "node:fs";
import { isIPv4 } from "node:net";
import { dirname, join } from "node:path";
import { PUBLIC_WEBADMIN_ORIGIN, UBUNTU_WEBADMIN_ORIGIN } from "./public-email-code.ts";

export const STAGE4_APACHE_SITE = "/etc/apache2/sites-available/dc24h-stage4-webadmin.conf";

export function approvedStage4Origin(port: string): string | undefined {
  return ([PUBLIC_WEBADMIN_ORIGIN, UBUNTU_WEBADMIN_ORIGIN] as const)
    .find(origin => new URL(origin).port === port);
}

export function webAdminOrigin(hostname: string, port: string): string | undefined {
  const label = /^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/;
  const labels = hostname.split(".");
  if (hostname.length > 253 || labels.length < 2 || !/^[a-z]/.test(labels.at(-1)!) ||
      !labels.every(part => label.test(part)) || !/^[1-9]\d{0,4}$/.test(port) || Number(port) > 65535)
    return undefined;
  return "https://" + hostname + (port === "443" ? "" : ":" + port);
}

export function isWebAdminOrigin(origin: unknown): origin is string {
  if (typeof origin !== "string") return false;
  try {
    const { hostname, port } = new URL(origin);
    return origin === webAdminOrigin(hostname, port || "443");
  } catch { return false; }
}

export function isStage4BindAddress(address: string): boolean {
  return isIPv4(address) && address !== "0.0.0.0";
}

type SiteOptions = { bindAddress: string; certificate: string; privateKey: string };

function siteContract(origin: string, bindAddress?: string): ReadonlyArray<readonly [RegExp, readonly string[]]> {
  if (!isWebAdminOrigin(origin) || (bindAddress !== undefined && !isStage4BindAddress(bindAddress)))
    throw Error("Invalid Stage IV HTTPS endpoint");
  const { hostname, host, port: explicitPort } = new URL(origin);
  const port = explicitPort || "443";
  const authority = host.replaceAll(".", "\\.");
  const originRule = `RewriteCond %{HTTP:Origin} !^https://${authority}$`;
  // Apache's standard ssl_module configuration provides the shared 443 listener.
  const listen = bindAddress ? (port === "443" ? [] : [`Listen ${bindAddress}:${port} https`]) : [`Listen ${port}`];
  return [
    [/^Listen\b/i, listen],
    [/^<VirtualHost\b/i, [`<VirtualHost ${bindAddress ?? "*"}:${port}>`]],
    [/^ServerName\b/i, [`ServerName ${hostname}`]],
    [/^SSLEngine\b/i, ["SSLEngine On"]],
    [/^RewriteCond %{HTTP_HOST}(?: |$)/i, [`RewriteCond %{HTTP_HOST} !^${authority}${explicitPort ? "" : "(?::443)?"}$`]],
    [/^RewriteCond %{HTTP:Origin}(?: |$)/i, ["RewriteCond %{HTTP:Origin} !^$", originRule, originRule]],
  ];
}

function assertSiteContract(text: string, origin: string, bindAddress?: string): void {
  const lines = text.split(/\r?\n/).map(line => line.trim().replace(/[ \t]+/g, " "));
  for (const [directive, expected] of siteContract(origin, bindAddress)) {
    const actual = lines.filter(line => directive.test(line));
    if (actual.length !== expected.length || actual.some((line, index) => line !== expected[index]))
      throw Error("Stage IV Apache site does not match the selected HTTPS Origin");
  }
}

export function checkStage4ApacheSite(origin: string, site = STAGE4_APACHE_SITE, bindAddress?: string): boolean {
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
    assertSiteContract(readFileSync(file, "utf8"), origin, bindAddress);
    return true;
  } finally {
    closeSync(file);
  }
}

export function prepareStage4ApacheSite(
  origin: string, template: string, publish: boolean, site = STAGE4_APACHE_SITE, options?: SiteOptions,
): "preserved" | "ready" | "created" {
  const parent = dirname(site);
  const directory = lstatSync(parent);
  if (!directory.isDirectory() || directory.uid !== process.geteuid?.() || (directory.mode & 0o022))
    throw Error("Unsafe Stage IV Apache site directory");
  const bindAddress = options?.bindAddress;
  if (checkStage4ApacheSite(origin, site, bindAddress)) return "preserved";

  const text = readFileSync(template, "utf8");
  assertSiteContract(text, PUBLIC_WEBADMIN_ORIGIN);
  const selected = siteContract(origin, bindAddress);
  const replacements = new Map(siteContract(PUBLIC_WEBADMIN_ORIGIN).flatMap(([, lines], index) =>
    lines.map((line, offset) => [line, selected[index]![1][offset] ?? ""] as const)));
  if (options) {
    for (const [directive, path] of [
      ["SSLCertificateFile", options.certificate], ["SSLCertificateKeyFile", options.privateKey],
    ] as const) {
      const matches = text.split("\n").map(line => line.trim()).filter(line => line.startsWith(directive + " "));
      if (!/^\/[A-Za-z0-9._/-]+$/.test(path) || matches.length !== 1)
        throw Error("Invalid Stage IV TLS file");
      replacements.set(matches[0]!, `${directive} "${path}"`);
    }
  }
  const rendered = text.split("\n").map(line => {
    const replacement = replacements.get(line.trim().replace(/[ \t]+/g, " "));
    return replacement === undefined ? line : line.replace(line.trim(), replacement);
  }).join("\n");
  assertSiteContract(rendered, origin, bindAddress);
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
      if ((error as NodeJS.ErrnoException).code === "EEXIST" && checkStage4ApacheSite(origin, site, bindAddress))
        return "preserved";
      throw error;
    }
    return "created";
  } finally {
    rmSync(staging, { recursive: true });
  }
}
