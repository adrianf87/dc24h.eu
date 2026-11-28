export const tlsRenewalErrors = [
  "not_configured", "invalid_config", "unsupported_acme", "dns_mismatch", "http01_unavailable",
  "competing_scheduler", "certbot_unavailable", "invalid_certificate", "state_unavailable",
  "missing_baseline", "hub_reload_unavailable", "apache_reload_unavailable", "services_not_verified",
  "schedule_unavailable", "busy", "certbot_failed", "hook_missing", "activation_failed",
  "rollback_failed", "rollback_pending", "audit_unavailable", "timer_change_failed",
] as const;
export type TlsRenewalError = typeof tlsRenewalErrors[number];
export type TlsRenewalOperation = "status" | "check" | "enable" | "disable";

export function parseTlsRenewal(value: unknown) {
  const record = (input: unknown) => {
    if (!input || typeof input !== "object" || Array.isArray(input)) throw Error("invalid_tls_status");
    return input as Record<string, unknown>;
  };
  const oneOf = <T extends string>(input: unknown, allowed: readonly T[]) => {
    if (typeof input !== "string" || !allowed.includes(input as T)) throw Error("invalid_tls_status");
    return input as T;
  };
  const integer = (input: unknown, min: number, max: number) => {
    if (typeof input !== "number" || !Number.isSafeInteger(input) || input < min || input > max)
      throw Error("invalid_tls_status");
    return input;
  };
  const timestamp = (input: unknown) => integer(input, 0, 253402300799);
  const date = (input: unknown) => input === null ? null : timestamp(input);
  const error = (input: unknown) => input === null ? null : oneOf(input, tlsRenewalErrors);
  const service = (input: unknown) => oneOf(input, ["applied", "mismatch", "unavailable"] as const);
  const data = record(value);
  if (data.schema !== 1 || typeof data.configured !== "boolean" ||
      typeof data.enabled !== "boolean" || typeof data.ready !== "boolean") throw Error("invalid_tls_status");
  const schedule = oneOf(data.schedule, ["active", "disabled", "inconsistent", "unknown"] as const);
  if (data.enabled !== (schedule === "active")) throw Error("invalid_tls_status");
  let certificate = null;
  if (data.certificate !== null) {
    const { hostname, issuer, not_before, not_after } = record(data.certificate);
    if (typeof hostname !== "string" || !/^[a-z0-9.-]{1,253}$/.test(hostname) ||
        typeof issuer !== "string" || !issuer.length || issuer.length > 256 || /[\u0000-\u001f\u007f-\u009f]/u.test(issuer))
      throw Error("invalid_tls_status");
    certificate = { hostname, issuer, not_before: timestamp(not_before), not_after: timestamp(not_after) };
    if (certificate.not_before >= certificate.not_after) throw Error("invalid_tls_status");
  }
  return {
    schema: 1 as const,
    configured: data.configured, enabled: data.enabled, ready: data.ready, schedule, certificate,
    certificate_status: oneOf(data.certificate_status, ["valid", "expiring", "expired", "invalid", "unavailable"] as const),
    days_remaining: data.days_remaining === null ? null : integer(data.days_remaining, -4_000_000, 4_000_000),
    next_check: date(data.next_check), checked_at: date(data.checked_at),
    last_attempt: date(data.last_attempt), last_renewal: date(data.last_renewal),
    apache: service(data.apache), hub: service(data.hub),
    readiness_error: error(data.readiness_error), renewal_error: error(data.renewal_error),
    request_error: error(data.request_error),
  };
}

export type TlsRenewalStatus = ReturnType<typeof parseTlsRenewal>;
