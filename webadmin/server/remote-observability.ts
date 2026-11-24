// No raw audit, account IDs, IPs, usernames, command text, secrets or actions.
// The gateway receives only the C++-validated aggregate projection. No data
// persists across restart: absence of memory is unknown, never healthy.
import type { RemoteStatusProjection } from "../src/lib/status.ts";
import type { RetainedIncidents } from "./remote-incident-journal.ts";

export type RedactedAlertKind = "transport_refusals" | "login_refusals" | "rate_limit_refusals";
export type RedactedEventKind = "hub_started" | "hub_stopped" | RedactedAlertKind;
export type RedactedEvent = { kind: RedactedEventKind; age_seconds: number };
export type RedactedAlert = { kind: RedactedAlertKind; age_seconds: number };
const maximumEvents = 12;
const retentionMs = 10 * 60_000;
// A browser may retain a verified snapshot for up to 45 seconds. Do not let
// the gateway's own timeline claim verification longer than that when status
// stops arriving. Audit-derived signals need an even newer C++ sample.
const sampleValidityMs = 45_000;
const threshold = 5;
const alertCooldownMs = 60_000;

export type IncidentJournalSink = {
  append: (kind: RedactedEventKind) => void;
  read: () => RetainedIncidents;
  unavailable: () => void;
};

export function aggregateObservability(monotonic: () => number, journal?: IncidentJournalSink) {
  const events: { kind: RedactedEventKind; at: number }[] = [];
  let last: { running: boolean; rejects: number; uptime: number; at: number } | undefined;
  let lastClockAt: number | undefined;
  let lastAlertAt = Number.NEGATIVE_INFINITY;
  const auditWindow: Record<"login_refusals" | "rate_limit_refusals", number[]> = {
    login_refusals: [], rate_limit_refusals: [],
  };
  const lastAuditAlert: Record<"login_refusals" | "rate_limit_refusals", number> = {
    login_refusals: Number.NEGATIVE_INFINITY, rate_limit_refusals: Number.NEGATIVE_INFINITY,
  };
  const reset = () => {
    last = undefined; lastClockAt = undefined;
    events.length = 0; lastAlertAt = Number.NEGATIVE_INFINITY;
    auditWindow.login_refusals.length = 0; auditWindow.rate_limit_refusals.length = 0;
    lastAuditAlert.login_refusals = Number.NEGATIVE_INFINITY;
    lastAuditAlert.rate_limit_refusals = Number.NEGATIVE_INFINITY;
  };
  const prune = (at: number) => {
    while (events.length && at - events[0]!.at > retentionMs) events.shift();
  };
  const emit = (kind: RedactedEventKind, at: number) => {
    events.push({ kind, at });
    if (events.length > maximumEvents) events.shift();
    // A journal is optional and never becomes an authentication authority.
    // If it fails, report unknown retained history without crashing C++ auth.
    try { journal?.append(kind); }
    catch { journal?.unavailable(); }
  };
  const validClock = (at: number) => {
    if (!Number.isFinite(at) || at < 0 ||
        (lastClockAt !== undefined && at < lastClockAt)) { reset(); return false; }
    lastClockAt = at;
    return true;
  };
  const observe = (sample: RemoteStatusProjection) => {
    const at = monotonic();
    if (!validClock(at)) return;
    // Only C++ observations accepted through the bounded source-age contract
    // are eligible. Unknown older-v1 data cannot create verified incidents.
    const observation = sample.observation;
    const rejects = sample.network.security_rejected;
    if (!observation || observation.schema !== 1 ||
        !Number.isSafeInteger(observation.source_age_ms) ||
        observation.source_age_ms < 0 || observation.source_age_ms > 10_000 ||
        typeof rejects !== "number" || !Number.isSafeInteger(rejects) || rejects < 0 ||
        !Number.isSafeInteger(sample.server.uptime_seconds) || sample.server.uptime_seconds < 0) {
      reset();
      return;
    }
    if (last && at < last.at) { reset(); return; }
    if (last && (at - last.at > sampleValidityMs || rejects < last.rejects ||
        sample.server.uptime_seconds < last.uptime)) {
      // Stale receipts, counter reset or hub restart invalidate the previous
      // generation; never join its transport counters to the next one.
      reset();
    }
    prune(at);
    if (last && last.running !== sample.server.running)
      emit(sample.server.running ? "hub_started" : "hub_stopped", at);
    if (last && rejects >= last.rejects + threshold && at - last.at <= 60_000 &&
        at - lastAlertAt >= alertCooldownMs) {
      emit("transport_refusals", at);
      lastAlertAt = at;
    }
    last = { running: sample.server.running, rejects,
      uptime: sample.server.uptime_seconds, at };
  };
  const observeAudit = (event: "login_refused" | "mfa_refused" | "passkey_login_refused" | "rate_limited") => {
    const at = monotonic();
    if (!validClock(at)) return;
    // Do not present an audit-derived alert as verified until a valid, recent
    // C++ read sample has been observed in this process. Record no identities.
    if (!last || at - last.at > sampleValidityMs) { reset(); return; }
    const kind = event === "rate_limited" ? "rate_limit_refusals" : "login_refusals";
    const window = auditWindow[kind];
    while (window.length && at - window[0]! > 60_000) window.shift();
    window.push(at);
    if (window.length > threshold) window.shift();
    if (window.length === threshold && at - lastAuditAlert[kind] >= alertCooldownMs) {
      emit(kind, at);
      lastAuditAlert[kind] = at;
    }
  };
  const auditLost = () => { reset(); journal?.unavailable(); };
  const sourceUnavailable = () => { reset(); };
  const read = () => {
    const at = monotonic();
    if (!validClock(at) || (last && at - last.at > sampleValidityMs)) {
      reset();
      return { schema: 1 as const, state: "unknown" as const, events: [] as RedactedEvent[], alerts: [] as RedactedAlert[],
        ...(journal ? { retained: journal.read() } : {}) };
    }
    prune(at);
    const visible: RedactedEvent[] = events.map(e => ({ kind: e.kind,
      age_seconds: Math.floor((at - e.at) / 1000) }));
    return { schema: 1 as const, state: last ? "observed" as const : "unknown" as const,
      events: visible, alerts: visible.filter((e): e is RedactedAlert =>
        e.kind === "transport_refusals" || e.kind === "login_refusals" || e.kind === "rate_limit_refusals"),
      ...(journal ? { retained: journal.read() } : {}) };
  };
  return { observe, observeAudit, auditLost, sourceUnavailable, read };
}
