// Defense in depth for the browser: validate the remote gateway response and
// construct a fresh allowlisted snapshot before React can render any fields.
export type Summary = {
  schema: 1;
  mode: "read-only";
  observation?: { schema: 1; source_age_ms: number };
  server: { version: string; running: boolean; uptime_seconds: number };
  network: { active_connections: number; accepted_connections: number };
  users: { online: number; peak_online: number };
  chat: { received: number; rejected: number };
  search?: { queries: number; results: number };
  transfers?: { ctm: number; rcm: number };
};

function record(value: unknown): Record<string, unknown> | null {
  return value && typeof value === "object" && !Array.isArray(value)
    ? value as Record<string, unknown> : null;
}

function counter(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value >= 0;
}

export function parseRemoteSummaryResponse(value: unknown): Summary | null {
  const root = record(value);
  if (!root || root.schema !== 1 || root.mode !== "read-only") return null;
  const server = record(root.server);
  const network = record(root.network);
  const users = record(root.users);
  const chat = record(root.chat);
  if (!server || !network || !users || !chat ||
    typeof server.version !== "string" ||
    !/^(?:alpha-\d{1,3}\.\d{1,3}\.\d{1,3}|beta-\d{1,3}\.\d{1,3}(?:\.\d{1,3})?)$/.test(server.version) ||
    typeof server.running !== "boolean" ||
    !counter(server.uptime_seconds) ||
    !counter(network.active_connections) || !counter(network.accepted_connections) ||
    !counter(users.online) || !counter(users.peak_online) ||
    !counter(chat.received) || !counter(chat.rejected)) return null;

  const result: Summary = {
    schema: 1, mode: "read-only",
    server: { version: server.version, running: server.running, uptime_seconds: server.uptime_seconds },
    network: { active_connections: network.active_connections, accepted_connections: network.accepted_connections },
    users: { online: users.online, peak_online: users.peak_online },
    chat: { received: chat.received, rejected: chat.rejected },
  };
  if (root.observation !== undefined) {
    const observed = record(root.observation);
    if (!observed || Object.keys(observed).sort().join(",") !== "schema,source_age_ms" ||
        observed.schema !== 1 || !counter(observed.source_age_ms) || observed.source_age_ms > 10_000)
      return null;
    result.observation = { schema: 1, source_age_ms: observed.source_age_ms };
  }
  if (root.search !== undefined) {
    const search = record(root.search);
    if (!search || !counter(search.queries) || !counter(search.results)) return null;
    result.search = { queries: search.queries, results: search.results };
  }
  if (root.transfers !== undefined) {
    const transfers = record(root.transfers);
    if (!transfers || !counter(transfers.ctm) || !counter(transfers.rcm)) return null;
    result.transfers = { ctm: transfers.ctm, rcm: transfers.rcm };
  }
  return result;
}
