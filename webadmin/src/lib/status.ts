import {parseKick} from "./kick.ts";
import type {KickSnapshot} from "./kick.ts";
import { parseTransfers } from "./transfer-status.ts";
import type { TransferSnapshot } from "./transfer-status.ts";
import { parseSearch } from "./search-status.ts";
import type { SearchSnapshot } from "./search-status.ts";
import { parseConfiguration } from "./configuration.ts";
import type { StartupConfiguration } from "./configuration.ts";
export const WEBADMIN_VERSION = "alpha-0.8.4";
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
    rejected_connections: number;
    closed_connections: number;
    received_bytes: number;
    sent_bytes: number;
    peak_connections: number;
  };
  users: { online: number; peak_online: number; successful_logins: number };
  chat: {
    received: number;
    public_dispatches: number;
    private_dispatches: number;
    rejected: number;
    flood_limited: number;
    errors: number;
    queued: number;
    unavailable: number;
    refused: number;
  };
  kick?: KickSnapshot;
  search?: SearchSnapshot;
  transfers?: TransferSnapshot;
  database: { enabled: boolean };
  configuration?: StartupConfiguration;
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
    database = record(root.database),
    users = record(root.users),
    chat = record(root.chat);
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
  const configuration = root.configuration === undefined ? undefined : parseConfiguration(root.configuration);
  if(configuration && configuration.database.enabled !== database.enabled) throw new Error("Inconsistent database configuration");
  // Normalize the allowlisted fields; never forward unknown data or credentials.
  return {
    api_version: "v1",
    ...(root.kick===undefined?{}:{kick:parseKick(root.kick)}),
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
      rejected_connections: counter(network.rejected_connections),
      closed_connections: counter(network.closed_connections),
      received_bytes: counter(network.received_bytes),
      sent_bytes: counter(network.sent_bytes),
      peak_connections: counter(network.peak_connections),
    },
    users: {
      online: counter(users.online),
      peak_online: counter(users.peak_online),
      successful_logins: counter(users.successful_logins),
    },
    chat: {
      received: counter(chat.received),
      public_dispatches: counter(chat.public_dispatches),
      private_dispatches: counter(chat.private_dispatches),
      rejected: counter(chat.rejected),
      flood_limited: counter(chat.flood_limited),
      errors: counter(chat.errors),
      queued: counter(chat.queued),
      unavailable: counter(chat.unavailable),
      refused: counter(chat.refused),
    },
    database: { enabled: database.enabled },
    ...(configuration === undefined ? {} : {configuration}),
    ...(root.search === undefined ? {} : { search: parseSearch(root.search) }),
    ...(root.transfers === undefined ? {} : { transfers: parseTransfers(root.transfers) }),
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
