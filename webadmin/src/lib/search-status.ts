export const SEARCH_COUNTERS = [
  "queries_received", "queries_routed", "queries_rejected", "queries_rate_limited", "queries_ignored", "query_errors",
  "query_queued", "query_unavailable", "query_refused", "query_canceled",
  "results_received", "results_routed", "results_rejected", "results_rate_limited", "results_recipient_limited", "result_errors",
  "result_queued", "result_unavailable", "result_refused",
] as const;
export const SEARCH_FLAGS = ["queries_enabled", "results_enabled", "guest_queries", "guest_results", "require_result_token"] as const;
export const SEARCH_LIMITS = ["max_active_searches", "max_active_per_user", "search_lifetime_seconds", "query_rate", "query_burst", "query_byte_rate", "query_byte_burst", "result_rate", "result_burst", "result_byte_rate", "result_byte_burst"] as const;
export type SearchCounter = typeof SEARCH_COUNTERS[number];
export type SearchConfigKey = typeof SEARCH_FLAGS[number] | typeof SEARCH_LIMITS[number];
export interface SearchSnapshot {
  statistics: Record<SearchCounter | "tracked_searches", number>;
  configuration: Record<typeof SEARCH_FLAGS[number], boolean> & Record<typeof SEARCH_LIMITS[number], number>;
}
const record = (value: unknown): Record<string, unknown> => {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid search schema");
  return value as Record<string, unknown>;
};
const integer = (value: unknown, min = 0, max = Number.MAX_SAFE_INTEGER): number => {
  if (typeof value !== "number" || !Number.isSafeInteger(value) || value < min || value > max) throw new Error("Invalid search value");
  return value;
};
export function parseSearch(value: unknown): SearchSnapshot {
  const root = record(value), stats = record(root.statistics), config = record(root.configuration);
  const statistics = Object.fromEntries(SEARCH_COUNTERS.map(key => [key, integer(stats[key])])) as SearchSnapshot["statistics"];
  statistics.tracked_searches = integer(stats.tracked_searches, 0, 65535);
  const configuration = {} as SearchSnapshot["configuration"];
  for (const key of SEARCH_FLAGS) {
    if (typeof config[key] !== "boolean") throw new Error("Invalid search policy");
    configuration[key] = config[key];
  }
  for (const key of SEARCH_LIMITS) {
    const max = key === "max_active_searches" ? 65535 : key === "max_active_per_user" ? 64 : key === "search_lifetime_seconds" ? 3600 : key.includes("byte") ? 16777216 : 1000000;
    configuration[key] = integer(config[key], key.endsWith("byte_burst") ? 16384 : 1, max);
  }
  if (configuration.max_active_per_user > configuration.max_active_searches) throw new Error("Invalid search limits");
  return { statistics, configuration };
}
