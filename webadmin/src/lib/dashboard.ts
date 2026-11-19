export const DASHBOARD_COUNTERS = [
  "uptime_seconds",
  "users_online",
  "users_peak",
  "users_logins",
  "connections_active",
  "connections_peak",
  "connections_accepted",
  "connections_rejected",
  "connections_closed",
  "security_rejected",
  "security_closed",
  "adc_frames",
  "adcs_handshakes",
  "bytes_received",
  "bytes_sent",
  "chat_received",
  "chat_public",
  "chat_private",
  "chat_rejected",
  "chat_flood_limited",
  "search_queries",
  "search_routed",
  "search_rejected",
  "search_results",
  "search_tracked",
  "transfers_ctm",
  "transfers_rcm",
  "transfers_peer_unsupported",
  "transfers_peer_failed",
  "kicks_succeeded",
  "kicks_refused",
  "mutes_applied",
  "mutes_public_blocked",
  "mutes_private_blocked",
] as const;
export type DashboardCounter = (typeof DASHBOARD_COUNTERS)[number];
export interface DashboardSnapshot extends Record<DashboardCounter, string> {
  schema: 1;
  core_running: boolean;
  database_enabled: boolean;
}
const decimal = /^(0|[1-9]\d{0,19})$/;
const maxUint64 = 18446744073709551615n;
export function parseDashboard(value: unknown): DashboardSnapshot {
  if (!value || typeof value !== "object" || Array.isArray(value))
    throw new Error("Invalid dashboard");
  const data = value as Record<string, unknown>;
  if (data.schema !== 1) throw new Error("Invalid dashboard schema");
  if (
    typeof data.core_running !== "boolean" ||
    typeof data.database_enabled !== "boolean"
  ) {
    throw new Error("Invalid dashboard flag");
  }
  const result = {
    schema: 1 as const,
    core_running: data.core_running,
    database_enabled: data.database_enabled,
  } as DashboardSnapshot;
  for (const key of DASHBOARD_COUNTERS) {
    const count = data[key];
    if (
      typeof count !== "string" ||
      !decimal.test(count) ||
      BigInt(count) > maxUint64
    ) {
      throw new Error("Invalid dashboard counter");
    }
    result[key] = count;
  }
  return result;
}
