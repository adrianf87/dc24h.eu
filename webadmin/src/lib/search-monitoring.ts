import { SEARCH_COUNTERS } from "./search-status.ts";
import type { SearchCounter } from "./search-status.ts";
import type { HubSnapshot } from "./status.ts";

export interface SearchObservation {
  at: number;
  uptime: number;
  version: string;
  counters: Record<SearchCounter, number>;
}
export interface SearchInterval {
  at: number;
  seconds: number;
  queries: number;
  results: number;
  refusals: number;
  queued: number;
}
export const SEARCH_HISTORY_LIMIT = 30;
export const SEARCH_MIN_INTERVAL_MS = 1000;
export const SEARCH_MAX_INTERVAL_MS = 15000;

// Only sanitized aggregate counters enter this bounded, browser-local history.
export function observeSearch(previous: readonly SearchObservation[], snapshot: HubSnapshot, at: number): SearchObservation[] {
  if (!snapshot.search || !Number.isFinite(at) || at < 0) return [];
  const counters = Object.fromEntries(SEARCH_COUNTERS.map(key => [key, snapshot.search!.statistics[key]])) as SearchObservation["counters"];
  const sample = { at, uptime: snapshot.server.uptime_seconds, version: snapshot.server.version, counters };
  const last = previous.at(-1);
  if (!last || at < last.at || at - last.at > SEARCH_MAX_INTERVAL_MS ||
    sample.uptime < last.uptime || sample.version !== last.version ||
    SEARCH_COUNTERS.some(key => counters[key] < last.counters[key])) return [sample];
  // Rapid manual refreshes still update the snapshot, without unstable tiny intervals.
  if (at - last.at < SEARCH_MIN_INTERVAL_MS) return [...previous];
  return [...previous, sample].slice(-SEARCH_HISTORY_LIMIT);
}

export function searchIntervals(samples: readonly SearchObservation[]): SearchInterval[] {
  return samples.slice(1).map((sample, index) => {
    const before = samples[index];
    const seconds = (sample.at - before.at) / 1000;
    const rate = (key: SearchCounter) => (sample.counters[key] - before.counters[key]) / seconds;
    return {
      at: sample.at, seconds,
      queries: rate("queries_received"), results: rate("results_received"),
      refusals: rate("queries_rejected") + rate("queries_rate_limited") + rate("results_rejected") +
        rate("results_rate_limited") + rate("results_recipient_limited"),
      queued: rate("query_queued") + rate("result_queued"),
    };
  });
}
