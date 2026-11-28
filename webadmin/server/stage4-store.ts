import { createHash, createHmac, randomBytes, randomInt, timingSafeEqual } from "node:crypto";
import mariadb from "mariadb";
import type { Connection, PoolConnection, Pool } from "mariadb";
import { isSafeMailbox } from "./public-msmtp.ts";

export type Kind = "owner" | "login" | "register";
export type Identity = { id: string; role: string; status: string; revision: string };
export type RuntimeConfiguration = {
  environment: "test" | "production";
  socketPath: string; database: string; user: string; password: string; key: string;
  origin?: string;
};
export const stage4Identifier = /^[a-z][a-z0-9_]{0,26}$/;
export const installerDatabase = (name: unknown): name is string =>
  typeof name === "string" && stage4Identifier.test(name) &&
  !["mysql", "sys", "information_schema", "performance_schema", "dc24h_stage4_test"].includes(name);
export const stage4Migrations = ["0001_tables.sql", "0002_activation.sql", "0003_configurable_owner_email.sql"] as const;
type AccountRow = {
  id: bigint; revision: bigint; role: Buffer; status: Buffer; email: string; initial_owner: number | null;
  verified_at: bigint | null;
};
type SessionRow = AccountRow & { issued_at: bigint; seen_at: bigint; expires_at: bigint; session_revision: bigint };
type ChallengeRow = {
  kind: Kind; account_id: bigint | null; revision: bigint | null; created_at: bigint;
  expires_at: bigint; code_mac: Buffer; attempts: number; delivered: number | boolean;
};
type Approval = { ticket: string; code: string; expires: number };
export const opaque = () => randomBytes(32).toString("hex");
export const validToken = (ticket: unknown): ticket is string =>
  typeof ticket === "string" && /^[0-9a-f]{64}$/.test(ticket);
const digest = (ticket: string) => createHash("sha256").update(ticket, "hex").digest();
export const stage4SessionDigest = (token: string) => digest(token).toString("hex");
const decode = (a: AccountRow): Identity => ({
  id: String(a.id), role: a.role.toString(), status: a.status.toString(), revision: String(a.revision),
});
type Query = Pick<Connection, "execute">;
const selectAccount = "SELECT a.id,a.revision,a.role,a.status,e.email,e.initial_owner,e.verified_at " +
  "FROM dc24h_accounts a JOIN stage4_emails e ON e.account_id=a.id WHERE a.id=?";
const invalidCode = (error: unknown) => [1062, 1644].includes(Number((error as { errno?: number }).errno));

export function openStage4Store(config: RuntimeConfiguration, now = Date.now) {
  const pool = mariadb.createPool({
    socketPath: config.socketPath, database: config.database, user: config.user, password: config.password,
    connectionLimit: 6, minimumIdle: 0, acquireTimeout: 2500, connectTimeout: 2000, socketTimeout: 3500,
  });
  const key = Buffer.from(config.key, "hex");
  const mac = (domain: string, token: string, value = "") => createHmac("sha256", key)
    .update(domain).update("\0").update(token).update("\0").update(value).digest();
  const csrf = (kind: string, token: string) => mac("csrf:" + kind, token).toString("hex");
  function isCsrf(kind: string, token: unknown, supplied: unknown) {
    return validToken(token) && validToken(supplied) &&
      timingSafeEqual(Buffer.from(csrf(kind, token), "hex"), Buffer.from(supplied, "hex"));
  }
  async function transaction<T>(fn: (db: PoolConnection) => Promise<T>): Promise<T> {
    const db = await pool.getConnection();
    try {
      await db.beginTransaction();
      const result = await fn(db);
      await db.commit();
      return result;
    } catch (error) {
      await db.rollback().catch(() => {});
      throw error;
    } finally { db.release(); }
  }
  async function audit(db: Query, event: string, account: string | null = null) {
    await db.execute("INSERT INTO stage4_audit (at_time,account_id,event) VALUES (?,?,?)", [now(), account, event]);
  }
  async function cleanExpired(db: Query | Pool) {
    const at = now();
    await db.execute("DELETE FROM stage4_challenges WHERE expires_at<=? LIMIT 500", [at]);
    await db.execute("DELETE FROM stage4_attempts WHERE expires_at<=? LIMIT 500", [at]);
    await db.execute("DELETE FROM stage4_sessions WHERE expires_at<=? LIMIT 500", [at]);
  }
  async function allowed(buckets: [string, number, number][]) {
    return transaction(async db => {
      const at = now();
      let allow = true;
      for (const [label, limit, ttl] of buckets) {
        const id = mac("limiter", label);
        await db.execute("INSERT INTO stage4_attempts(bucket,hits,expires_at) VALUES(?,1,?) " +
          "ON DUPLICATE KEY UPDATE hits=IF(expires_at<=?,1,LEAST(hits+1,65535)), " +
          "expires_at=IF(expires_at<=?,VALUES(expires_at),expires_at)", [id, at + ttl, at, at]);
        const rows = await db.execute("SELECT hits FROM stage4_attempts WHERE bucket=?", [id]) as { hits: number }[];
        if (Number(rows[0]?.hits) > limit) {
          // The global bucket comes first, bounding storage even during an IP flood.
          allow = false;
          break;
        }
      }
      return allow;
    });
  }
  async function begin(db: Query, kind: Kind, identity?: Identity): Promise<Approval> {
    const ticket = opaque();
    const code = randomInt(0, 100_000_000).toString().padStart(8, "0");
    const created = now(), expires = created + 300_000;
    await db.execute("INSERT INTO stage4_challenges(ticket,kind,account_id,revision,code_mac,created_at,expires_at) " +
      "VALUES (?,?,?,?,?,?,?)", [digest(ticket), kind, identity?.id ?? null, identity?.revision ?? null,
      mac("otp:" + kind, ticket, code), created, expires]);
    return { ticket, code, expires };
  }
  const readAccount = async (db: Query | Pool, id: string, lock = false) => {
    const rows = await db.execute(selectAccount + (lock ? " FOR UPDATE" : ""), [id]) as AccountRow[];
    return rows[0];
  };
  async function beginLogin(identity: Identity) {
    return transaction(async db => {
      const account = await readAccount(db, identity.id, true);
      if (!account || String(account.revision) !== identity.revision) return null;
      const pending = account.initial_owner === 1 && account.verified_at === null &&
        account.role.toString() === "owner" && account.status.toString() === "disabled";
      if (!pending && (account.verified_at === null || account.status.toString() !== "active")) return null;
      if (!isSafeMailbox(account.email)) return null;
      return { ...await begin(db, pending ? "owner" : "login", identity), email: account.email };
    });
  }
  async function beginRegistration(username: string, email: string, verifier: string) {
    try {
      return await transaction(async db => {
        // Expired pending reservations must be reclaimable even before the sweep.
        await db.execute("DELETE c FROM stage4_challenges c JOIN stage4_registrations r ON r.ticket=c.ticket " +
          "WHERE c.expires_at<=? AND (r.email=? OR r.username=?)", [now(), email, username]);
        const names = await db.execute("SELECT id FROM dc24h_accounts WHERE " +
          "LOWER(CONVERT(name USING utf8mb4))=LOWER(?) LIMIT 1", [username]) as unknown[];
        const emails = await db.execute("SELECT account_id FROM stage4_emails WHERE email=?", [email]) as unknown[];
        if (names.length || emails.length) return null;
        const challenge = await begin(db, "register");
        await db.execute("INSERT INTO stage4_registrations(ticket,username,email,verifier) VALUES(?,?,?,?)",
          [digest(challenge.ticket), username, email, verifier]);
        return challenge;
      });
    } catch (error) {
      if (invalidCode(error)) return null;
      throw error;
    }
  }
  async function delivery(ticket: string, ok: boolean) {
    if (!ok) {
      await pool.execute("DELETE FROM stage4_challenges WHERE ticket=?", [digest(ticket)]);
      return false;
    }
    const outcome = await pool.execute("UPDATE stage4_challenges SET delivered=TRUE WHERE ticket=? AND expires_at>?",
      [digest(ticket), now()]) as { affectedRows: number };
    return outcome.affectedRows === 1;
  }
  async function verify(ticket: string, code: string, registration: boolean) {
    try {
      return await transaction(async db => {
        const id = digest(ticket);
        const rows = await db.execute("SELECT * FROM stage4_challenges WHERE ticket=? FOR UPDATE", [id]) as ChallengeRow[];
        const challenge = rows[0], at = now();
        if (!challenge || (challenge.kind === "register") !== registration || !challenge.delivered ||
            at < Number(challenge.created_at) || at >= Number(challenge.expires_at) || challenge.attempts >= 5) return null;
        if (!timingSafeEqual(challenge.code_mac, mac("otp:" + challenge.kind, ticket, code))) {
          if (challenge.attempts >= 4) await db.execute("DELETE FROM stage4_challenges WHERE ticket=?", [id]);
          else await db.execute("UPDATE stage4_challenges SET attempts=attempts+1 WHERE ticket=?", [id]);
          return null;
        }
        if (registration) {
          await db.execute("CALL stage4_confirm_registration(?,?)", [id, at]);
          await db.execute("DELETE FROM stage4_challenges WHERE ticket=?", [id]);
          return { registered: true } as const;
        }
        const accountId = String(challenge.account_id);
        let account = await readAccount(db, accountId, true);
        if (!account || account.revision !== challenge.revision) return null;
        if (challenge.kind === "owner") {
          await db.execute("CALL stage4_confirm_owner(?,?,?)", [accountId, account.revision, at]);
          await audit(db, "verify_email", accountId);
          account = await readAccount(db, accountId, true);
        }
        if (!account || account.verified_at === null || account.status.toString() !== "active") return null;
        await db.execute("DELETE FROM stage4_challenges WHERE ticket=?", [id]);
        const token = opaque();
        await db.execute("INSERT INTO stage4_sessions(token,account_id,revision,issued_at,seen_at,expires_at) " +
          "VALUES(?,?,?,?,?,?)", [digest(token), accountId, account.revision, at, at, at + 600_000]);
        await audit(db, "login", accountId);
        return { token, identity: decode(account), issued: at };
      });
    } catch (error) {
      if (invalidCode(error)) return null;
      throw error;
    }
  }
  async function session(token: string) {
    const id = digest(token);
    const rows = await pool.execute("SELECT a.id,a.revision,a.role,a.status,e.email,e.verified_at," +
      "s.issued_at,s.seen_at,s.expires_at,s.revision session_revision " +
      "FROM stage4_sessions s JOIN dc24h_accounts a ON a.id=s.account_id " +
      "JOIN stage4_emails e ON e.account_id=a.id WHERE s.token=?", [id]) as SessionRow[];
    const entry = rows[0], at = now();
    if (!entry || entry.status.toString() !== "active" || entry.verified_at === null ||
        entry.revision !== entry.session_revision || at < Number(entry.seen_at) ||
        at >= Number(entry.expires_at) || at - Number(entry.seen_at) >= 120_000) return null;
    return { token, identity: decode(entry), issued: Number(entry.issued_at) };
  }
  async function touch(token: string) {
    await pool.execute("UPDATE stage4_sessions SET seen_at=GREATEST(seen_at,?) WHERE token=?", [now(), digest(token)]);
  }
  async function revoke(token: string, accountId: string, event: "logout" | "revoke_sessions" | "password_change") {
    await transaction(async db => {
      if (event === "logout") await db.execute("DELETE FROM stage4_sessions WHERE token=?", [digest(token)]);
      else {
        await db.execute("DELETE FROM stage4_sessions WHERE account_id=?", [accountId]);
        await db.execute("DELETE FROM stage4_challenges WHERE account_id=?", [accountId]);
      }
      await audit(db, event, accountId);
    });
  }
  return {
    csrf, isCsrf, allowed, beginLogin, beginRegistration, delivery, verify, session, touch, revoke,
    async check() {
      const rows = await pool.execute("SELECT COUNT(*) n FROM stage4_schema_migrations") as { n: bigint }[];
      const version = Number(rows[0]?.n);
      // Existing deployments on schema 2 can still use the original mailbox.
      if (version !== 2 && version !== stage4Migrations.length) throw Error("Stage IV database is not initialized");
    },
    clean: () => cleanExpired(pool),
    close: () => pool.end(),
  };
}

export type Stage4Store = ReturnType<typeof openStage4Store>;
