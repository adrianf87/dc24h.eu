import { execFileSync, spawnSync } from "node:child_process";
import { createHash, randomBytes } from "node:crypto";
import {
  chownSync, chmodSync, closeSync, constants, fchownSync, fstatSync, fsyncSync, lstatSync, mkdirSync,
  mkdtempSync, openSync, readFileSync, renameSync, rmSync, unlinkSync, writeFileSync,
} from "node:fs";
import { dirname, join } from "node:path";
import mariadb from "mariadb";
import type { Connection } from "mariadb";
import { loadBootstrap, msmtpConfig } from "./stage4-bootstrap-config.ts";
import { withPinnedPrivateParent } from "./remote-private-path.ts";

const install = "/opt/dc24h";
const readOnlyTables = ["dc24h_schema_migrations"];
const webTables = ["stage4_challenges", "stage4_registrations", "stage4_attempts", "stage4_sessions"];
const schemaTables = [...webTables, "stage4_emails", "stage4_audit", "stage4_recovery"];
const routines = ["stage4_confirm_registration", "stage4_confirm_owner"];
const columns = {
  stage4_emails: "account_id,email,initial_owner,verified_at",
  stage4_challenges: "ticket,kind,account_id,revision,code_mac,created_at,expires_at,attempts,delivered",
  stage4_registrations: "ticket,username,email,verifier",
  stage4_attempts: "bucket,expires_at,hits",
  stage4_sessions: "token,account_id,revision,issued_at,seen_at,expires_at",
  stage4_audit: "id,at_time,account_id,event",
  stage4_recovery: "id,account_id,opened_at,closed_at,result",
  stage4_schema_migrations: "version,checksum,applied_at",
};
const safeToken = () => randomBytes(32).toString("hex");
const binary = (name: string) => join(install, "bin", name);
const defaultConfig = "/etc/dc24h/stage4-webadmin-bootstrap.env";

function command(path: string, args: string[]) {
  const run = spawnSync(path, args, { timeout: 30_000, stdio: "ignore",
    env: { PATH: "/usr/bin:/bin", LANG: "C.UTF-8" } });
  if (run.status !== 0) throw Error("Native initialization command failed: " + path.split("/").pop());
}

function uidGid(user: string) {
  const run = (option: string) => Number(execFileSync("/usr/bin/id", [option, user],
    { encoding: "utf8", stdio: ["ignore", "pipe", "ignore"] }).trim());
  const ids = { uid: run("-u"), gid: run("-g") };
  if (!ids.uid || !ids.gid) throw Error("Missing dedicated service identity");
  return ids;
}

function privateDirectory(path: string, uid: number, gid: number) {
  try {
    mkdirSync(path, { mode: 0o700 });
    chownSync(path, uid, gid);
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code !== "EEXIST") throw error;
  }
  const info = lstatSync(path);
  if (!info.isDirectory() || info.uid !== uid || info.gid !== gid ||
      (info.mode & 0o7777) !== 0o700) throw Error("Unsafe Stage IV state directory");
}

function publish(path: string, content: string, uid = 0, gid = 0) {
  const parent = dirname(path);
  const fd = openSync(parent, constants.O_RDONLY | constants.O_DIRECTORY | constants.O_NOFOLLOW);
  const dir = "/proc/self/fd/" + fd;
  const target = dir + "/" + path.slice(parent.length + 1);
  const temporary = dir + "/.stage4-" + safeToken();
  try {
    const file = openSync(temporary, constants.O_WRONLY | constants.O_CREAT | constants.O_EXCL, 0o600);
    try {
      writeFileSync(file, content);
      fchownSync(file, uid, gid);
      fsyncSync(file);
    } finally { closeSync(file); }
    renameSync(temporary, target);
    fsyncSync(fd);
  } finally {
    try { unlinkSync(temporary); }
    catch (error) { if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error; }
    finally { closeSync(fd); }
  }
}

function existingSecret(path: string, uid = 0) {
  let fd: number | undefined;
  try {
    fd = openSync(path, constants.O_RDONLY | constants.O_NOFOLLOW | constants.O_NONBLOCK);
    const info = fstatSync(fd);
    if (!info.isFile() || info.nlink !== 1 || info.uid !== uid ||
        (info.mode & 0o7777) !== 0o600 || info.size !== 64) throw Error("Unsafe runtime secret file");
    const result = readFileSync(fd, "ascii");
    if (!/^[0-9a-f]{64}$/.test(result)) throw Error("Malformed runtime secret file");
    return result;
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
    return null;
  } finally { if (fd !== undefined) closeSync(fd); }
}

function nativeConfig(socket: string, name: string, user: string, passwordFile: string, api?: string) {
  return "[network]\nlisten=127.0.0.1:0\n[database]\nenabled=on\nsocket=" + socket +
    "\nname=" + name + "\nuser=" + user + "\npassword_file=" + passwordFile +
    "\n[logging]\nconnections=off\nchat=off\nfile=stderr\n" +
    (api ? "[api]\nenabled=on\nsocket=/run/dc24h-stage4-auth/api.sock\ntoken_file=" + api + "\n" : "");
}

type Grant = { target: string; privileges: string };
const table = (name: string, privileges: string): Grant => ({ target: name, privileges });
const routine = (name: string): Grant => ({ target: "PROCEDURE " + name, privileges: "EXECUTE" });
const accountSQL = (user: string) => "'" + user + "'@'localhost'";
const escaped = (name: string) => "`" + name + "`";

async function account(db: Connection, database: string, user: string, password: string, grants: Grant[],
  socketPath: string) {
  const allowed = new Map(grants.map(({ target, privileges }) => {
    const isRoutine = target.startsWith("PROCEDURE ");
    const qualified = escaped(database) + "." + escaped(target.replace(/^PROCEDURE /, ""));
    return [(isRoutine ? "PROCEDURE " : "") + qualified, privileges.split(", ")];
  }));
  const name = accountSQL(user);
  const users = await db.query("SELECT Host FROM mysql.user WHERE User=?", [user]) as { Host: string }[];
  if (users.some(row => row.Host !== "localhost")) throw Error("Stage IV SQL user has a nonlocal account");
  if (users.length === 0) await db.query("CREATE USER " + name + " IDENTIFIED BY ?", [password]);
  for (const row of await db.query("SHOW GRANTS FOR " + name) as Record<string, string>[]) {
    const grant = Object.values(row)[0] ?? "";
    if (/\bWITH GRANT OPTION\b|\bWITH ADMIN OPTION\b/i.test(grant))
      throw Error("Unexpected grant option on Stage IV SQL account");
    if (grant.startsWith("GRANT USAGE ON *.* TO ")) continue;
    const match = /^GRANT (.+) ON (.+) TO /.exec(grant);
    const expected = match && allowed.get(match[2]!);
    if (!match || !expected || !match[1]!.split(", ").every(right => expected.includes(right)))
      throw Error("Unexpected existing Stage IV SQL grant");
  }
  const check = await mariadb.createConnection({ socketPath, user, password, connectTimeout: 2000 });
  try { await check.ping(); } finally { await check.end(); }
  for (const [target, rights] of allowed)
    await db.query("GRANT " + rights.join(", ") + " ON " + target + " TO " + name);
}

async function migrate(db: Connection) {
  await db.query("CREATE TABLE IF NOT EXISTS stage4_schema_migrations (" +
    "version INT NOT NULL PRIMARY KEY, checksum CHAR(64) CHARACTER SET ascii NOT NULL," +
    "applied_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP) ENGINE=InnoDB");
  const files = ["0001_tables.sql", "0002_activation.sql"];
  const applied = await db.query("SELECT version, checksum FROM stage4_schema_migrations ORDER BY version") as
    { version: number; checksum: string }[];
  if (applied.some((row, index) => row.version !== index + 1) || applied.length > files.length)
    throw Error("Unexpected Stage IV migration catalog");
  for (const [index, file] of files.entries()) {
    const sql = readFileSync(join(install, "database/stage4", file), "utf8");
    const checksum = createHash("sha256").update(sql).digest("hex");
    const previous = applied[index];
    if (previous) {
      if (previous.checksum !== checksum) throw Error("Stage IV migration checksum mismatch");
      continue;
    }
    for (const statement of sql.split("\n@@\n")) await db.query(statement);
    await db.query("INSERT INTO stage4_schema_migrations(version,checksum) VALUES (?,?)", [index + 1, checksum]);
  }
  const tables = await db.query("SELECT TABLE_NAME name, ENGINE engine FROM information_schema.TABLES " +
    "WHERE TABLE_SCHEMA=DATABASE() AND LEFT(TABLE_NAME,7)='stage4_'") as { name: string; engine: string }[];
  if (![...schemaTables, "stage4_schema_migrations"].every(name =>
    tables.some(row => row.name === name && row.engine === "InnoDB"))) throw Error("Incomplete Stage IV tables");
  const shapes = await db.query("SELECT TABLE_NAME name, GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION) columns " +
    "FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND LEFT(TABLE_NAME,7)='stage4_' GROUP BY TABLE_NAME") as
    { name: string; columns: string }[];
  if (!Object.entries(columns).every(([name, fields]) => shapes.some(row => row.name === name && row.columns === fields)))
    throw Error("Stage IV table layout does not match its catalog");
  const procedures = await db.query("SELECT ROUTINE_NAME name FROM information_schema.ROUTINES " +
    "WHERE ROUTINE_SCHEMA=DATABASE() AND SECURITY_TYPE='DEFINER' AND ROUTINE_TYPE='PROCEDURE'") as { name: string }[];
  if (!routines.every(name => procedures.some(row => row.name === name))) throw Error("Incomplete Stage IV routines");
}

async function main() {
  const args = process.argv.slice(2);
  if (args.length && (args.length !== 2 || args[0] !== "--config"))
    throw Error("Usage: dc24h-stage4-init [--config /etc/dc24h/file.env]");
  const config = loadBootstrap(args[1] ?? defaultConfig);
  const production = config.STAGE4_ENVIRONMENT === "production";
  const webPath = "/var/lib/dc24h-stage4";
  const authPath = "/var/lib/dc24h-stage4-auth";
  const state = production ? "/etc/dc24h" : "/etc/dc24h/stage4-test";
  privateDirectory(state, 0, 0);
  const web = production ? uidGid("dc24h-webadmin") : { uid: 0, gid: 0 };
  const auth = production ? uidGid("dc24h-stage3") : { uid: 0, gid: 0 };
  if (production) {
    for (const unit of ["dc24h-twilight-stage4-webadmin", "dc24h-twilight-stage4-auth"]) {
      if (spawnSync("/usr/bin/systemctl", ["is-active", "--quiet", unit], { stdio: "ignore" }).status === 0)
        throw Error("Stop Stage IV services before provisioning");
    }
    privateDirectory(webPath, web.uid, web.gid);
    privateDirectory(authPath, auth.uid, auth.gid);
  }
  const authDir = production ? authPath : state;
  const mailDir = production ? webPath : state;
  const authFile = join(authDir, "auth-db.password");
  const authPassword = existingSecret(authFile, auth.uid) ?? safeToken();
  const keyFile = join(state, "stage4-auth-key");
  const key = existingSecret(keyFile) ?? safeToken();
  publish(authFile, authPassword, auth.uid, auth.gid);
  publish(keyFile, key);

  const name = config.MARIADB_DATABASE;
  const socketPath = config.MARIADB_SOCKET;
  const db = await mariadb.createConnection({ socketPath, user: config.MARIADB_ROOT_USERNAME,
    password: config.MARIADB_ROOT_PASSWORD, connectTimeout: 3000, socketTimeout: 15000 });
  const lock = "dc24h:stage4:init:" + name;
  let phase = "database provisioning";
  try {
    const lockResult = await db.query("SELECT GET_LOCK(?,0) acquired", [lock]) as { acquired: number }[];
    if (Number(lockResult[0]?.acquired) !== 1) throw Error("Bootstrap already in progress");
    await db.query("CREATE DATABASE IF NOT EXISTS " + escaped(name) + " CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");
    await db.query("USE " + escaped(name));
    const migrator = "stage4_init_" + randomBytes(8).toString("hex");
    const scratch = mkdtempSync("/run/dc24h-stage4-init-");
    try {
      chmodSync(scratch, 0o700);
      const pass = safeToken();
      await db.query("CREATE USER " + accountSQL(migrator) + " IDENTIFIED BY ?", [pass]);
      await db.query("GRANT ALL PRIVILEGES ON " + escaped(name) + ".* TO " + accountSQL(migrator));
      publish(join(scratch, "sql.password"), pass);
      publish(join(scratch, "native.ini"), nativeConfig(socketPath, name, migrator, join(scratch, "sql.password")));
      phase = "native MariaDB migration";
      command(binary("dc24h-twilight"), ["--config", join(scratch, "native.ini"), "--migrate-database"]);
      command(binary("dc24h-twilight"), ["--config", join(scratch, "native.ini"), "--database-status"]);
      phase = "Stage IV schema";
      await migrate(db);
      phase = "Administrator bootstrap";
      const existing = await db.query("SELECT a.role, a.name, e.initial_owner, e.email, e.verified_at, " +
        "c.account_id credential FROM dc24h_accounts a LEFT JOIN stage4_emails e ON e.account_id=a.id " +
        "LEFT JOIN dc24h_account_credentials c ON c.account_id=a.id " +
        "WHERE LOWER(CONVERT(a.name USING utf8mb4))='administrator' OR a.role='owner'") as
        { role: Buffer; name: Buffer; initial_owner: number; email: string; credential: bigint; verified_at: bigint | null }[];
      let owner = "PENDING";
      if (existing.length) {
        const only = existing[0]!;
        if (existing.length !== 1 || only.role.toString() !== "owner" || only.name.toString() !== "Administrator" ||
            only.initial_owner !== 1 || only.email !== "admin@dc24h.eu" || !only.credential)
          throw Error("Existing Administrator requires an audited operator migration");
        owner = only.verified_at === null ? "PENDING" : "PASS";
      } else {
        if (!config.ADMIN_INITIAL_PASSWORD) throw Error("ADMIN_INITIAL_PASSWORD is required for a fresh database");
        const passwordFile = join(scratch, "owner.password");
        publish(passwordFile, config.ADMIN_INITIAL_PASSWORD);
        try {
          command(binary("dc24h-twilight-owner"), ["--config", join(scratch, "native.ini"),
            "--stage4-bootstrap", "Administrator", "--password-file", passwordFile, "--confirm-stopped"]);
        } finally { unlinkSync(passwordFile); }
      }
      phase = "least-privilege SQL grants";
      await account(db, name, config.MARIADB_APP_USERNAME, config.MARIADB_APP_PASSWORD, [
        ...webTables.map(tableName => table(tableName, "SELECT, INSERT, UPDATE, DELETE")),
        table("dc24h_accounts", "SELECT"), table("stage4_emails", "SELECT"),
        table("stage4_audit", "INSERT"), table("stage4_schema_migrations", "SELECT"),
        ...routines.map(routine),
      ], socketPath);
      const authUser = config.MARIADB_APP_USERNAME + "_auth";
      await account(db, name, authUser, authPassword, [
        ...readOnlyTables.map(tableName => table(tableName, "SELECT")),
        table("dc24h_accounts", "SELECT, INSERT, UPDATE"), table("dc24h_account_credentials", "SELECT, INSERT, UPDATE"),
        table("dc24h_account_adc_credentials", "SELECT, DELETE"), table("dc24h_audit_events", "SELECT, INSERT"),
        table("dc24h_bans", "SELECT, INSERT, UPDATE"),
        table("stage4_emails", "SELECT"), table("stage4_sessions", "SELECT"),
      ], socketPath);
      phase = "private runtime configuration";
      const apiToken = join(authDir, "api.token");
      publish(apiToken, existingSecret(apiToken, auth.uid) ?? safeToken(), auth.uid, auth.gid);
      publish(join(authDir, "auth.ini"),
        nativeConfig(socketPath, name, authUser, authFile, apiToken), auth.uid, auth.gid);
      publish(join(mailDir, ".msmtprc"), msmtpConfig(config), web.uid, web.gid);
      withPinnedPrivateParent(join(state, "stage4-runtime.json"), () => publish(join(state, "stage4-runtime.json"),
        JSON.stringify({ environment: config.STAGE4_ENVIRONMENT, database: name, socketPath,
          user: config.MARIADB_APP_USERNAME, password: config.MARIADB_APP_PASSWORD, key }) + "\n"));
      if (production) publish(join(state, "stage4-activation.env"),
        "DC24H_STAGE4_STARTUP_ACK=OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO\n");
      const result = { environment: config.STAGE4_ENVIRONMENT, database: name, schema: "PASS",
        sql_grants: "PASS", administrator: "PASS", administrator_email: owner, smtp_delivery: "NOT TESTED" };
      process.stdout.write(JSON.stringify(result) + "\n");
    } finally {
      try { await db.query("DROP USER IF EXISTS " + accountSQL(migrator)); }
      finally { rmSync(scratch, { recursive: true, force: true }); }
    }
  } catch (error) {
    // The connector can include SQL text and parameters in errors. Only
    // predefined phase names, numeric server codes and fixed diagnostics leave here.
    if (phase === "Administrator bootstrap" && error instanceof Error &&
        /^(?:Existing Administrator|ADMIN_INITIAL_PASSWORD)/.test(error.message)) throw error;
    const errno = (error as { errno?: unknown }).errno;
    throw Error(phase + " failed" + (Number.isInteger(errno) ? " (MariaDB " + errno + ")" : ""));
  } finally {
    await db.query("DO RELEASE_LOCK(?)", [lock]).catch(() => {});
    await db.end().catch(() => {});
  }
}

main().catch(error => {
  const message = typeof error?.message === "string" ? error.message : "";
  const safe = /^(?:Invalid bootstrap field [A-Z0-9_]+|Invalid or repeated bootstrap field|Bootstrap requires root.*|Unsafe bootstrap .*|Bootstrap file changed during reading|Use distinct Stage IV credentials|Usage:.*|Stop Stage IV services before provisioning|Existing Administrator requires an audited operator migration|ADMIN_INITIAL_PASSWORD is required for a fresh database|[A-Za-z ]+ failed(?: \(MariaDB [0-9]+\))?)$/.test(message);
  process.stderr.write("Stage IV initialization FAIL: " + (safe ? message : "private input or connection error") + "\n");
  process.exitCode = 1;
});
