import { createHmac, randomBytes, randomInt, timingSafeEqual } from "node:crypto";

/** Private password + email-code exchange. No account can select its own recipient. */
export const PUBLIC_WEBADMIN_ORIGIN = "https://server-747585786.dc24h.eu:7278";
export const UBUNTU_WEBADMIN_ORIGIN = "https://server-747585786.dc24h.eu:7178";
export const PUBLIC_WEBADMIN_PORT = 7278;
export type PublicAdminRole = "owner" | "administrator";
export type PublicPasswordGrant = { actor: string; role: PublicAdminRole };

export type EmailCodeDependencies<Grant extends PublicPasswordGrant, Session> = {
  verifyPassword(actor: string, password: string): Promise<Grant | null>;
  verifiedEmail(actor: string): string | null;
  sendCode(email: string, code: string): Promise<boolean>;
  reauthorize(grant: Grant): Promise<boolean>;
  issueSession(grant: Grant): Promise<Session | null>;
  now?: () => number;
  ttlMs?: number;
  maxPending?: number;
};

export function createPublicEmailCodeGate<Grant extends PublicPasswordGrant, Session>(
  dependencies: EmailCodeDependencies<Grant, Session>,
) {
  const clock = dependencies.now ?? Date.now;
  const ttlMs = dependencies.ttlMs ?? 300_000;
  const maxPending = dependencies.maxPending ?? 32;
  if (!Number.isSafeInteger(ttlMs) || ttlMs < 1 || ttlMs > 300_000 ||
      !Number.isSafeInteger(maxPending) || maxPending < 1 || maxPending > 128)
    throw Error("Unsafe email code limits");
  const pepper = randomBytes(32);
  type Pending = {
    actor: string; grant: Grant; digest: Buffer;
    issued: number; expires: number; attempts: number;
  };
  const pending = new Map<string, Pending>();
  const forget = (ticket: string) => {
    const value = pending.get(ticket);
    value?.digest.fill(0);
    pending.delete(ticket);
  };
  const prune = (at: number) => {
    for (const [ticket, value] of pending)
      if (at < value.issued || at >= value.expires) forget(ticket);
  };
  const digest = (ticket: string, actor: string, code: string) =>
    createHmac("sha256", pepper).update(ticket + "\0" + actor + "\0" + code).digest();

  const begin = async (actor: string, password: string): Promise<{ ticket: string; expires_in: number } | null> => {
    const at = clock(); prune(at);
    let outstandingTicket: string | undefined;
    if (pending.size >= maxPending || !/^[1-9]\d{0,18}$/.test(actor) ||
        BigInt(actor) > 9223372036854775807n || typeof password !== "string" ||
        !password || password.length > 1024) return null;
    try {
      const grant = await dependencies.verifyPassword(actor, password);
      if (!grant || grant.actor !== actor || (grant.role !== "owner" && grant.role !== "administrator")) return null;
      const email = dependencies.verifiedEmail(actor);
      if (!email) return null;
      const ticket = randomBytes(32).toString("hex");
      outstandingTicket = ticket;
      const code = randomInt(0, 100_000_000).toString().padStart(8, "0");
      const issued = clock();
      if (pending.size >= maxPending) return null;
      pending.set(ticket, { actor, grant, digest: digest(ticket, actor, code),
        issued, expires: issued + ttlMs, attempts: 0 });
      // No password, code, mailbox or token goes to logs or browser response.
      const sent = await dependencies.sendCode(email, code);
      if (!sent || clock() >= issued + ttlMs || clock() < issued) {
        forget(ticket); return null;
      }
      return { ticket, expires_in: Math.ceil((issued + ttlMs - clock()) / 1000) };
    } catch { if (outstandingTicket) forget(outstandingTicket); return null; }
  };

  const finish = async (ticket: string, code: string): Promise<Session | null> => {
    const at = clock(); prune(at);
    const entry = pending.get(ticket);
    if (!entry || at < entry.issued || at >= entry.expires) return null;
    // Fixed-length code, bounded attempts and constant-time digest comparison.
    entry.attempts++;
    const valid = typeof code === "string" && /^[0-9]{8}$/.test(code) &&
      timingSafeEqual(entry.digest, digest(ticket, entry.actor, code));
    if (!valid) { if (entry.attempts >= 5) forget(ticket); return null; }
    // Consume synchronously before async checks; concurrent replay loses.
    forget(ticket);
    try {
      if (!await dependencies.reauthorize(entry.grant) || clock() < entry.issued ||
          clock() >= entry.expires) return null;
      return await dependencies.issueSession(entry.grant);
    } catch { return null; }
  };
  return { begin, finish, cancel: forget, pendingCount: () => pending.size };
}
