import { randomBytes } from "node:crypto";

/**
 * Internal Stage IV password + passkey orchestration, NOT a public HTTP server.
 *
 * The caller must implement backend password-proof verification, a genuine
 * origin/RP-ID/user-verification checked WebAuthn ceremony, current-role
 * reauthorization, and opaque session issuance. No network listener, cookie
 * parser, passkey enrollment or fallback endpoint is created here.
 */
export const PUBLIC_WEBADMIN_RP_ID = "server-747585786.dc24h.eu";
export const PUBLIC_WEBADMIN_ORIGIN = "https://server-747585786.dc24h.eu:7278";
export const PUBLIC_WEBADMIN_PORT = 7278;

export type PublicAdminRole = "owner" | "administrator";

export type PublicPasswordGrant = {
  actor: string;
  role: PublicAdminRole;
};

type PasskeyOptions = { challenge: string };

export type PublicTwoFactorDependencies<
  Assertion,
  Grant extends PublicPasswordGrant,
  Session,
  Options extends PasskeyOptions,
> = {
  verifyPassword(actor: string, password: string): Promise<Grant | null>;
  passkeys: {
    beginLogin(actor: string): Promise<Options | null>;
    finishLogin(actor: string, challenge: string, assertion: Assertion, now: number): Promise<boolean>;
  };
  reauthorize(grant: Grant): Promise<boolean>;
  issueSession(grant: Grant): Promise<Session | null>;
  now?: () => number;
  ttlMs?: number;
  maxPending?: number;
};

export function requirePublicWebAuthnIdentity(origin: string, rpID: string) {
  if (origin !== PUBLIC_WEBADMIN_ORIGIN || rpID !== PUBLIC_WEBADMIN_RP_ID) {
    throw new Error("Stage IV WebAuthn identity mismatch");
  }
  return { origin: PUBLIC_WEBADMIN_ORIGIN, rpID: PUBLIC_WEBADMIN_RP_ID };
}

export function createPublicPasswordPasskeyGate<
  Assertion,
  Grant extends PublicPasswordGrant,
  Session,
  Options extends PasskeyOptions,
>(dependencies: PublicTwoFactorDependencies<Assertion, Grant, Session, Options>) {
  const clock = dependencies.now ?? Date.now;
  const ttlMs = dependencies.ttlMs ?? 60_000;
  const maxPending = dependencies.maxPending ?? 32;
  if (!Number.isInteger(ttlMs) || ttlMs < 1 || ttlMs > 60_000 ||
      !Number.isInteger(maxPending) || maxPending < 1 || maxPending > 128) {
    throw new Error("Unsafe public two-factor gate limits");
  }

  type Pending = { actor: string; grant: Grant; challenge: string; expiresAt: number };
  const pending = new Map<string, Pending>();
  const prune = (now: number) => {
    for (const [ticket, value] of pending) {
      if (value.expiresAt <= now) pending.delete(ticket);
    }
  };

  const begin = async (actor: string, password: string): Promise<{ ticket: string; options: Options } | null> => {
    const now = clock();
    prune(now);
    if (!/^[1-9][0-9]{0,18}$/.test(actor) || typeof password !== "string" ||
        password.length === 0 || password.length > 4096 || pending.size >= maxPending) return null;
    try {
      const grant = await dependencies.verifyPassword(actor, password);
      if (!grant || grant.actor !== actor ||
          (grant.role !== "owner" && grant.role !== "administrator")) return null;
      const options = await dependencies.passkeys.beginLogin(actor);
      if (!options || typeof options.challenge !== "string" || options.challenge.length < 16) return null;
      if (pending.size >= maxPending) return null;
      const ticket = randomBytes(32).toString("base64url");
      pending.set(ticket, { actor, grant, challenge: options.challenge, expiresAt: clock() + ttlMs });
      // The HTTP adapter must place ticket in a short-lived Secure/HttpOnly
      // SameSite=Strict host-only cookie, never in a URL or audit log.
      return { ticket, options };
    } catch {
      return null;
    }
  };

  const finish = async (ticket: string, assertion: Assertion): Promise<Session | null> => {
    const now = clock();
    const entry = pending.get(ticket);
    // Consume before awaiting either verifier, preventing parallel/replayed
    // assertions and denying session issuance from a password-only exchange.
    pending.delete(ticket);
    prune(now);
    if (!entry || now >= entry.expiresAt) return null;
    try {
      const passkeyValid = await dependencies.passkeys.finishLogin(
        entry.actor, entry.challenge, assertion, now
      );
      if (!passkeyValid || !await dependencies.reauthorize(entry.grant)) return null;
      // Only after both factors and a fresh role/grant check may a session
      // be issued. A passkey alone never creates public administrator access.
      return await dependencies.issueSession(entry.grant);
    } catch {
      return null;
    }
  };

  const cancel = (ticket: string) => { pending.delete(ticket); };
  return { begin, finish, cancel, pendingCount: () => pending.size };
}
