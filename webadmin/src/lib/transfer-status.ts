export const TRANSFER_COUNTERS = [
  "ctm_received", "ctm_routed", "ctm_rejected", "ctm_rate_limited", "ctm_recipient_limited",
  "ctm_errors", "ctm_queued", "ctm_refused", "ctm_canceled", "ctm_queue_errors",
  "rcm_received", "rcm_routed", "rcm_rejected", "rcm_rate_limited", "rcm_recipient_limited",
  "rcm_errors", "rcm_queued", "rcm_refused", "rcm_canceled", "rcm_queue_errors",
  "peer_protocol_unsupported", "peer_connection_failed",
] as const;
export type TransferCounter = typeof TRANSFER_COUNTERS[number];
export const TRANSFER_FLAGS = ["ctm_enabled", "rcm_enabled", "guest_initiate", "guest_receive"] as const;
export const TRANSFER_LIMITS = ["request_rate", "request_burst", "request_byte_rate", "request_byte_burst", "incoming_rate", "incoming_burst", "incoming_byte_rate", "incoming_byte_burst"] as const;
export interface TransferConfiguration extends Record<typeof TRANSFER_FLAGS[number], boolean>, Record<typeof TRANSFER_LIMITS[number], number> {
  allowed_protocols: "any" | string[];
}
export function parseTransferConfiguration(value: unknown): TransferConfiguration {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid transfer configuration");
  const input = value as Record<string, unknown>, output = {} as TransferConfiguration;
  for (const key of TRANSFER_FLAGS) {
    if (typeof input[key] !== "boolean") throw new Error("Invalid transfer switch");
    output[key] = input[key];
  }
  for (const key of TRANSFER_LIMITS) {
    const count = input[key], maximum = key.includes("byte") ? 16777216 : 1000000;
    if (typeof count !== "number" || !Number.isSafeInteger(count) || count < (key.endsWith("byte_burst") ? 16384 : 1) || count > maximum) throw new Error("Invalid transfer limit");
    output[key] = count;
  }
  const protocols = input.allowed_protocols;
  if (protocols === "any") output.allowed_protocols = "any";
  else {
    if (!Array.isArray(protocols) || protocols.length > 16 || protocols.some(name =>
      typeof name !== "string" || !/^[\x21-\x2b\x2d-\x7e]{1,32}$/.test(name) || name === "any" || name === "none") ||
      new Set(protocols).size !== protocols.length) throw new Error("Invalid transfer protocol policy");
    output.allowed_protocols = [...protocols];
  }
  return output;
}
export interface TransferSnapshot {
  statistics: Record<TransferCounter, number>;
  configuration?: TransferConfiguration;
}
export function parseTransfers(value: unknown): TransferSnapshot {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid transfer schema");
  const stats = (value as Record<string, unknown>).statistics;
  if (!stats || typeof stats !== "object" || Array.isArray(stats)) throw new Error("Invalid transfer statistics");
  const input = stats as Record<string, unknown>;
  const statistics = {} as TransferSnapshot["statistics"];
  for (const key of TRANSFER_COUNTERS) {
    const count = input[key];
    if (typeof count !== "number" || !Number.isSafeInteger(count) || count < 0) throw new Error("Invalid transfer counter");
    statistics[key] = count;
  }
  const configuration = (value as Record<string, unknown>).configuration;
  return { statistics, ...(configuration === undefined ? {} : { configuration: parseTransferConfiguration(configuration) }) };
}
