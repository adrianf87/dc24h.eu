export const TRANSFER_COUNTERS = [
  "ctm_received", "ctm_routed", "ctm_rejected", "ctm_rate_limited", "ctm_recipient_limited",
  "ctm_errors", "ctm_queued", "ctm_refused", "ctm_canceled", "ctm_queue_errors",
  "rcm_received", "rcm_routed", "rcm_rejected", "rcm_rate_limited", "rcm_recipient_limited",
  "rcm_errors", "rcm_queued", "rcm_refused", "rcm_canceled", "rcm_queue_errors",
  "peer_protocol_unsupported", "peer_connection_failed",
] as const;
export type TransferCounter = typeof TRANSFER_COUNTERS[number];
export interface TransferSnapshot {
  statistics: Record<TransferCounter, number>;
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
  return { statistics };
}
