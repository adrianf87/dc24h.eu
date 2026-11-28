import { closeSync, constants, fstatSync, lstatSync, readFileSync } from "node:fs";
import { dirname } from "node:path";
import { openPrivateLeaf } from "./remote-private-path.ts";
import { PUBLIC_WEBADMIN_ORIGIN } from "./public-email-code.ts";
import { approvedStage4Origin, checkStage4ApacheSite } from "./stage4-apache-site.ts";

const fixed = {
  ADMIN_USERNAME: "Administrator", ADMIN_EMAIL: "admin@dc24h.eu", ADMIN_ROLE: "owner",
  ADMIN_REQUIRE_EMAIL_OTP: "true", SMTP_FROM: "admin@dc24h.eu", SMTP_AUTH: "true",
  SMTP_TLS: "true", SMTP_VERIFY_CERTIFICATE: "true", MARIADB_HOST: "127.0.0.1",
  WEBADMIN_HOST: new URL(PUBLIC_WEBADMIN_ORIGIN).hostname,
  WEBADMIN_INTERNAL_HOST: "127.0.0.1", WEBADMIN_INTERNAL_PORT: "4278", WEBADMIN_HTTPS: "true",
  WEBADMIN_REQUIRE_PASSWORD: "true", WEBADMIN_REQUIRE_EMAIL_OTP: "true",
  WEBADMIN_REGISTRATION_EMAIL_VERIFICATION: "true",
};
const variable = [
  "STAGE4_ENVIRONMENT", "MARIADB_APPROVED_DATABASE", "ADMIN_INITIAL_PASSWORD", "SMTP_HOST",
  "SMTP_PORT", "SMTP_USERNAME", "SMTP_PASSWORD", "SMTP_STARTTLS", "MARIADB_PORT",
  "MARIADB_SOCKET", "MARIADB_ROOT_USERNAME", "MARIADB_ROOT_PASSWORD", "MARIADB_DATABASE",
  "MARIADB_APP_USERNAME", "MARIADB_APP_PASSWORD", "WEBADMIN_PUBLIC_PORT",
] as const;
type Key = keyof typeof fixed | typeof variable[number];
const keys = new Set<string>([...Object.keys(fixed), ...variable]);
const identifier = /^[a-z][a-z0-9_]{0,26}$/;
const unsafe = /^(?:password|administrator|admin|root|changeme|replace|secret|example|placeholder|1234|qwerty|test)[0-9!@#$._-]*$/i;
const passwordOK = (value: string, min: number) =>
  [...value].length >= min && Buffer.byteLength(value) <= 1024 &&
  !/[\u0000-\u001f\u007f]/u.test(value) && value === value.trim() &&
  new Set(value).size >= 8 && !unsafe.test(value);

export function loadBootstrap(path: string) {
  if (process.geteuid?.() !== 0 || !/^\/etc\/dc24h\/[a-zA-Z0-9._-]+\.env$/.test(path))
    throw Error("Bootstrap requires root and a private config under /etc/dc24h");
  for (let parent = dirname(path); ; parent = dirname(parent)) {
    const info = lstatSync(parent);
    if (!info.isDirectory() || info.uid !== 0 || info.mode & 0o022)
      throw Error("Unsafe bootstrap parent directory");
    if (parent === "/") break;
  }
  const fd = openPrivateLeaf(path, constants.O_RDONLY | constants.O_NONBLOCK);
  const values = Object.create(null) as Record<Key, string>;
  let bytes: Buffer | undefined;
  try {
    const stat = fstatSync(fd);
    if (!stat.isFile() || stat.uid !== 0 || stat.nlink !== 1 || (stat.mode & 0o7777) !== 0o600 ||
        stat.size < 1 || stat.size > 8192) throw Error("Unsafe bootstrap file");
    bytes = readFileSync(fd);
    if (bytes.length !== stat.size) throw Error("Bootstrap file changed during reading");
    for (const line of new TextDecoder("utf-8", { fatal: true }).decode(bytes).split(/\r?\n/)) {
      if (!line.trim() || line.trimStart().startsWith("#")) continue;
      const match = /^([A-Z][A-Z0-9_]*)=([^\u0000-\u001f\u007f]*)$/u.exec(line);
      if (!match || !keys.has(match[1]!) || Object.hasOwn(values, match[1]!))
        throw Error("Invalid or repeated bootstrap field");
      values[match[1]! as Key] = match[2]!;
    }
  } finally { bytes?.fill(0); closeSync(fd); }
  const requireField = (key: Key, ok: unknown) => {
    if (!Object.hasOwn(values, key) || !ok) throw Error("Invalid bootstrap field " + key);
  };
  for (const [key, expected] of Object.entries(fixed))
    requireField(key as Key, values[key as Key] === expected);
  const publicOrigin = approvedStage4Origin(values.WEBADMIN_PUBLIC_PORT);
  if (!publicOrigin) throw Error("Invalid bootstrap field WEBADMIN_PUBLIC_PORT");
  const matchEnvironment = values.STAGE4_ENVIRONMENT === "test" ? "dc24h_stage4_test" :
    values.STAGE4_ENVIRONMENT === "production" ? "dc24h_stage4_production" : "";
  requireField("STAGE4_ENVIRONMENT", matchEnvironment);
  requireField("MARIADB_DATABASE", values.MARIADB_DATABASE === matchEnvironment);
  requireField("MARIADB_APPROVED_DATABASE", values.MARIADB_APPROVED_DATABASE === matchEnvironment);
  requireField("MARIADB_APP_USERNAME", identifier.test(values.MARIADB_APP_USERNAME ?? "") &&
    !["root", "mysql", "mariadb", "admin"].includes(values.MARIADB_APP_USERNAME) &&
    (values.MARIADB_APP_USERNAME.endsWith("_production") === (values.STAGE4_ENVIRONMENT === "production")));
  requireField("MARIADB_ROOT_USERNAME", identifier.test(values.MARIADB_ROOT_USERNAME ?? "") &&
    values.MARIADB_ROOT_USERNAME !== values.MARIADB_APP_USERNAME &&
    values.MARIADB_ROOT_USERNAME !== values.MARIADB_APP_USERNAME + "_auth");
  requireField("MARIADB_SOCKET", /^\/run\/(?:[a-z0-9_-]+\/)+[a-z0-9_.-]+\.sock$/.test(values.MARIADB_SOCKET ?? ""));
  requireField("MARIADB_PORT", values.MARIADB_PORT === "3306");
  requireField("ADMIN_INITIAL_PASSWORD", values.ADMIN_INITIAL_PASSWORD === "" ||
    passwordOK(values.ADMIN_INITIAL_PASSWORD ?? "", 15));
  requireField("MARIADB_APP_PASSWORD", passwordOK(values.MARIADB_APP_PASSWORD ?? "", 20));
  requireField("MARIADB_ROOT_PASSWORD", values.MARIADB_ROOT_PASSWORD === "" ||
    passwordOK(values.MARIADB_ROOT_PASSWORD ?? "", 12));
  requireField("SMTP_HOST", /^[a-zA-Z0-9](?:[a-zA-Z0-9.-]{0,251}[a-zA-Z0-9])?$/.test(values.SMTP_HOST ?? ""));
  requireField("SMTP_PORT", /^[1-9]\d{0,4}$/.test(values.SMTP_PORT ?? "") && Number(values.SMTP_PORT) <= 65535);
  requireField("SMTP_STARTTLS", /^(true|false)$/.test(values.SMTP_STARTTLS ?? ""));
  requireField("SMTP_USERNAME", /^[a-zA-Z0-9@._+%-]{1,254}$/.test(values.SMTP_USERNAME ?? ""));
  requireField("SMTP_PASSWORD", passwordOK(values.SMTP_PASSWORD ?? "", 12));
  if (new Set([values.ADMIN_INITIAL_PASSWORD, values.SMTP_PASSWORD, values.MARIADB_APP_PASSWORD]
    .filter(Boolean)).size !== (values.ADMIN_INITIAL_PASSWORD ? 3 : 2))
    throw Error("Use distinct Stage IV credentials");
  if (!checkStage4ApacheSite(publicOrigin)) throw Error("Stage IV Apache site is missing");
  return Object.assign(values, { publicOrigin });
}

export function msmtpConfig(values: ReturnType<typeof loadBootstrap>) {
  // msmtp's password directive takes the rest of the line literally.
  return [
    "defaults", "auth on", "tls on", "tls_certcheck on", "tls_trust_file system",
    "tls_starttls " + (values.SMTP_STARTTLS === "true" ? "on" : "off"),
    "timeout 8", "account default", "host " + values.SMTP_HOST, "port " + values.SMTP_PORT,
    "from admin@dc24h.eu", "user " + values.SMTP_USERNAME, 'password "' + values.SMTP_PASSWORD + '"', "",
  ].join("\n");
}
