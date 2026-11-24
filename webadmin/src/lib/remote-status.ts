// Defense in depth for the browser: validate the remote gateway response and
// construct a fresh allowlisted snapshot before React can render any fields.
export type Summary = {
  schema: 1;
  mode: "read-only";
  observation?: { schema: 1; source_age_ms: number };
  incidents?: { schema: 1; state: "observed" | "unknown";
    events: { kind: "hub_started" | "hub_stopped" | "transport_refusals" | "login_refusals" | "rate_limit_refusals"; age_seconds: number }[];
    alerts: { kind: "transport_refusals" | "login_refusals" | "rate_limit_refusals"; age_seconds: number }[];
    retained?: { state: "available" | "unknown";
      events: { kind: "hub_started" | "hub_stopped" | "transport_refusals" | "login_refusals" | "rate_limit_refusals"; age_seconds: number }[] } };
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
  if (root.incidents !== undefined) {
    const incident = record(root.incidents);
    if (!result.observation || !incident ||
        !["alerts,events,schema,state", "alerts,events,retained,schema,state"].includes(Object.keys(incident).sort().join(",")) ||
        incident.schema !== 1 || !["observed", "unknown"].includes(incident.state as string) ||
        !Array.isArray(incident.events) || incident.events.length > 12 ||
        !Array.isArray(incident.alerts) || incident.alerts.length > 12) return null;
    const parseEvent = (value: unknown, alert: boolean) => {
      const item = record(value);
      return item && Object.keys(item).sort().join(",") === "age_seconds,kind" &&
        (alert ? ["transport_refusals", "login_refusals", "rate_limit_refusals"].includes(item.kind as string) :
          ["hub_started", "hub_stopped", "transport_refusals", "login_refusals", "rate_limit_refusals"].includes(item.kind as string)) &&
        counter(item.age_seconds) && item.age_seconds <= 600;
    };
    if (!incident.events.every((e: unknown) => parseEvent(e, false)) ||
        !incident.alerts.every((e: unknown) => parseEvent(e, true)) ||
        (incident.state === "unknown" && (incident.events.length || incident.alerts.length))) return null;
    let retained: NonNullable<Summary["incidents"]>["retained"];
    // Retained aggregate events are historical only. They are valid even when
    // the live C++ source state is unknown, but never imply a healthy hub.
    if (incident.retained !== undefined) {
      const historical = record(incident.retained);
      if (!historical || Object.keys(historical).sort().join(",") !== "events,state" ||
          !["available", "unknown"].includes(historical.state as string) ||
          !Array.isArray(historical.events) || historical.events.length > 12 ||
          !historical.events.every((e: unknown) => parseEvent(e, false)) ||
          (historical.state === "unknown" && historical.events.length)) return null;
      retained = { state: historical.state as "available" | "unknown",
        events: historical.events.map((e: { kind: "hub_started" | "hub_stopped" | "transport_refusals" | "login_refusals" | "rate_limit_refusals"; age_seconds: number }) =>
          ({ kind: e.kind, age_seconds: e.age_seconds })) };
    }
    result.incidents = { schema: 1, state: incident.state as "observed" | "unknown",
      events: incident.events.map((e: { kind: "hub_started" | "hub_stopped" | "transport_refusals" | "login_refusals" | "rate_limit_refusals"; age_seconds: number }) =>
        ({ kind: e.kind, age_seconds: e.age_seconds })),
      alerts: incident.alerts.map((e: { kind: "transport_refusals" | "login_refusals" | "rate_limit_refusals"; age_seconds: number }) =>
        ({ kind: e.kind, age_seconds: e.age_seconds })),
      ...(retained ? { retained } : {}) };
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
