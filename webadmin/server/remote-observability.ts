// No raw audit, account IDs, IPs, usernames, command text, secrets or actions.
// The gateway receives only the C++-validated aggregate projection. No data
// persists across restart: absence of memory is unknown, never healthy.
import type { RemoteStatusProjection } from "../src/lib/status.ts";

export type RedactedEventKind = "hub_started" | "hub_stopped" | "transport_refusals";
export type RedactedEvent = { kind: RedactedEventKind; age_seconds: number };
export type RedactedAlert = { kind: "transport_refusals"; age_seconds: number };
const maximumEvents = 12;
const retentionMs = 10 * 60_000;
const threshold = 5;
const alertCooldownMs = 60_000;

export function aggregateObservability(monotonic: () => number) {
  const events: { kind: RedactedEventKind; at: number }[] = [];
  let last: { running: boolean; rejects: number; at: number } | undefined;
  let lastAlertAt = Number.NEGATIVE_INFINITY;
  const prune = (at: number) => {
    while (events.length && at - events[0]!.at > retentionMs) events.shift();
  };
  const emit = (kind: RedactedEventKind, at: number) => {
    events.push({ kind, at });
    if (events.length > maximumEvents) events.shift();
  };
  const observe = (sample: RemoteStatusProjection) => {
    const at = monotonic();
    // Only C++ observations accepted through the bounded source-age contract
    // are eligible. Unknown older-v1 data cannot create verified incidents.
    if (!sample.observation || !Number.isFinite(at) || at < 0) return;
    const rejects = sample.network.security_rejected;
    if (rejects === undefined) return;
    if (last && at < last.at) {
      // Clock reversal or a changed synthetic clock invalidates the entire
      // in-memory timeline and its cooldown assumptions.
      last = undefined; events.length = 0; lastAlertAt = Number.NEGATIVE_INFINITY;
    }
    prune(at);
    if (last && last.running !== sample.server.running)
      emit(sample.server.running ? "hub_started" : "hub_stopped", at);
    if (last && rejects >= last.rejects + threshold && at - last.at <= 60_000 &&
        at - lastAlertAt >= alertCooldownMs) {
      emit("transport_refusals", at);
      lastAlertAt = at;
    }
    last = { running: sample.server.running, rejects, at };
  };
  const read = () => {
    const at = monotonic();
    if (!Number.isFinite(at) || at < 0 || (last && at < last.at)) {
      events.length = 0; last = undefined; lastAlertAt = Number.NEGATIVE_INFINITY;
      return { schema: 1 as const, state: "unknown" as const, events: [] as RedactedEvent[], alerts: [] as RedactedAlert[] };
    }
    prune(at);
    const visible: RedactedEvent[] = events.map(e => ({ kind: e.kind,
      age_seconds: Math.floor((at - e.at) / 1000) }));
    return { schema: 1 as const, state: last ? "observed" as const : "unknown" as const,
      events: visible, alerts: visible.filter((e): e is RedactedAlert => e.kind === "transport_refusals") };
  };
  return { observe, read };
}
