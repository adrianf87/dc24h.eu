// Fixed, non-personal security-event projection for the local-only gateway.
// Never accept a raw request, actor identifier, cookie, proof or OTP here.
import type { RemoteEvent } from "./remote-gateway.ts";

export const remoteAuditEvents = [
  "login_ok", "login_refused", "mfa_ok", "mfa_refused", "logout",
  "session_revoked", "route_refused", "api_unavailable",
] as const satisfies readonly RemoteEvent[];

const allowed: ReadonlySet<string> = new Set(remoteAuditEvents);

export function remoteAuditRecord(event: RemoteEvent, milliseconds: number): string {
  if (!allowed.has(event) || !Number.isSafeInteger(milliseconds) ||
    milliseconds < 0 || milliseconds > 8_640_000_000_000_000)
    throw new Error("Invalid local audit event");
  const at = new Date(milliseconds).toISOString();
  return JSON.stringify({ schema: 1, component: "remote-gateway", event, at }) + "\n";
}
