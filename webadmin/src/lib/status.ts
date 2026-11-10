export const WEBADMIN_VERSION = "alpha-0.3.2";
export interface HubSnapshot {
  api_version: "v1";
  server: {
    name: "dc24h.eu";
    version: string;
    core_running: boolean;
    uptime_seconds: number;
  };
  network: {
    active_connections: number;
    accepted_connections: number;
    adc_frames: number;
    adcs_handshakes: number;
    security_rejected: number;
    security_closed: number;
  };
  database: { enabled: boolean };
}
const record = (value: unknown): Record<string, unknown> => {
  if (!value || typeof value !== "object" || Array.isArray(value))
    throw new Error("Invalid API response");
  return value as Record<string, unknown>;
};
const counter = (value: unknown): number => {
  if (typeof value !== "number" || !Number.isSafeInteger(value) || value < 0)
    throw new Error("Invalid API counter");
  return value;
};
export function parseSnapshot(value: unknown): HubSnapshot {
  const root = record(value),
    server = record(root.server),
    network = record(root.network),
    database = record(root.database);
  if (
    root.api_version !== "v1" ||
    server.name !== "dc24h.eu" ||
    typeof server.version !== "string" ||
    !/^alpha-\d{1,3}\.\d{1,3}\.\d{1,3}$/.test(server.version) ||
    typeof server.core_running !== "boolean" ||
    typeof database.enabled !== "boolean"
  ) {
    throw new Error("Invalid API schema");
  }
  // Normalize the allowlisted fields; never forward unknown data or credentials.
  return {
    api_version: "v1",
    server: {
      name: "dc24h.eu",
      version: server.version,
      core_running: server.core_running,
      uptime_seconds: counter(server.uptime_seconds),
    },
    network: {
      active_connections: counter(network.active_connections),
      accepted_connections: counter(network.accepted_connections),
      adc_frames: counter(network.adc_frames),
      adcs_handshakes: counter(network.adcs_handshakes),
      security_rejected: counter(network.security_rejected),
      security_closed: counter(network.security_closed),
    },
    database: { enabled: database.enabled },
  };
}
export function duration(seconds: number): string {
  const days = Math.floor(seconds / 86400),
    hours = Math.floor(seconds / 3600) % 24,
    minutes = Math.floor(seconds / 60) % 60;
  return days
    ? `${days}d ${hours}h`
    : hours
      ? `${hours}h ${minutes}m`
      : minutes
        ? `${minutes}m ${seconds % 60}s`
        : `${seconds}s`;
}
