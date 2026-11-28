import type { TlsRenewalStatus } from "../src/lib/tls-renewal.ts";

export const tlsStatus: TlsRenewalStatus = {
  schema: 1, configured: true, enabled: false, ready: true, schedule: "disabled",
  certificate: { hostname: "tls.test", issuer: "Fixture CA", not_before: 1795500000, not_after: 1802500000 },
  certificate_status: "valid", days_remaining: 60, next_check: null, checked_at: 1795500000,
  last_attempt: null, last_renewal: null, apache: "applied", hub: "applied",
  readiness_error: null, renewal_error: null, request_error: null,
};
