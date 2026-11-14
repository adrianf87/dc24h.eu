import { TRANSFER_COUNTERS } from "./transfer-status.ts";
import type { TransferCounter } from "./transfer-status.ts";
import type { HubSnapshot } from "./status.ts";

export interface TransferObservation {
  at: number;
  uptime: number;
  version: string;
  counters: Record<TransferCounter, number>;
}
export interface TransferInterval {
  at: number;
  seconds: number;
  ctm: number;
  rcm: number;
  refusals: number;
  queued: number;
}
export const TRANSFER_HISTORY_LIMIT = 30;
export const TRANSFER_MIN_INTERVAL_MS = 1000;
export const TRANSFER_MAX_INTERVAL_MS = 15000;

// Input is a validated API snapshot. Retain only owned aggregate counters.
export function observeTransfers(previous: readonly TransferObservation[], snapshot: HubSnapshot, at: number): TransferObservation[] {
  if (!snapshot.transfers || !Number.isFinite(at) || at < 0) return [];
  const counters = Object.fromEntries(TRANSFER_COUNTERS.map(key => [key, snapshot.transfers!.statistics[key]])) as TransferObservation["counters"];
  const sample = { at, uptime: snapshot.server.uptime_seconds, version: snapshot.server.version, counters };
  const last = previous.at(-1);
  if (!last || at < last.at || at - last.at > TRANSFER_MAX_INTERVAL_MS ||
    sample.uptime < last.uptime || sample.version !== last.version ||
    TRANSFER_COUNTERS.some(key => counters[key] < last.counters[key])) return [sample];
  // Keep the original baseline and its increments through rapid manual refreshes.
  if (at - last.at < TRANSFER_MIN_INTERVAL_MS) return [...previous];
  return [...previous, sample].slice(-TRANSFER_HISTORY_LIMIT);
}

export function transferIntervals(samples: readonly TransferObservation[]): TransferInterval[] {
  return samples.slice(1).map((sample, index) => {
    const before = samples[index];
    const seconds = (sample.at - before.at) / 1000;
    const rate = (key: TransferCounter) => (sample.counters[key] - before.counters[key]) / seconds;
    return {
      at: sample.at, seconds,
      ctm: rate("ctm_received"), rcm: rate("rcm_received"),
      refusals: rate("ctm_rejected") + rate("rcm_rejected") + rate("ctm_rate_limited") +
        rate("rcm_rate_limited") + rate("ctm_recipient_limited") + rate("rcm_recipient_limited"),
      queued: rate("ctm_queued") + rate("rcm_queued"),
    };
  });
}
