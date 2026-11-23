// Bounded, nonidentifying security alerts for the systemd journal. These are
// signals for operators, not authentication decisions or a remote API.
import type { RemoteEvent } from "./remote-gateway.ts";

export type SecurityAlert = "authentication_denials" | "upstream_outage" |
  "audit_loss" | "audit_corruption" | "rate_limiter_loss";
const windowMs = 60_000;
const alertNames = new Set<SecurityAlert>([
  "authentication_denials", "upstream_outage", "audit_loss", "audit_corruption", "rate_limiter_loss",
]);

export function createRemoteSecurityMonitor(emit: (line: string) => void) {
  let since = 0, denials = 0, outages = 0;
  const last = new Map<SecurityAlert, number>();
  const signal = (reason: SecurityAlert, now: number) => {
    if (!alertNames.has(reason) || !Number.isSafeInteger(now) || now < 0 || now > 8_640_000_000_000_000)
      throw Error("Invalid security alert");
    const prior = last.get(reason);
    // A backward wall-clock jump must not suppress a critical storage-loss
    // warning until the old timestamp is reached again. Emit once and begin
    // a fresh bounded cooldown at the observed time.
    if (prior !== undefined && now >= prior && now < prior + windowMs) return;
    last.set(reason, now);
    // Fixed-code JSON only. Never log actor, credential, source address,
    // request path, factor proof or account/host identifiers.
    const line = JSON.stringify({ schema: 1, component: "remote-security", level: "warning",
      reason, at: new Date(now).toISOString() }) + "\n";
    try { emit(line); } catch { /* Monitoring must not replace the audit authority. */ }
  };
  const observe = (event: RemoteEvent, now: number) => {
    if (!Number.isSafeInteger(now) || now < 0 || now > 8_640_000_000_000_000)
      throw Error("Invalid security monitor clock");
    if (now >= since + windowMs || now < since) { since = now; denials = 0; outages = 0; }
    if (["login_refused", "mfa_refused", "passkey_login_refused"].includes(event)) {
      if (++denials >= 10) signal("authentication_denials", now);
    }
    if (event === "api_unavailable" && ++outages >= 3) signal("upstream_outage", now);
    if (event === "rate_limiter_unavailable") signal("rate_limiter_loss", now);
  };
  return { observe, signal };
}
