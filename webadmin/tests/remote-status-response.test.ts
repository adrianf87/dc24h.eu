import assert from "node:assert/strict";
import { test } from "node:test";
import { parseRemoteSummaryResponse } from "../src/lib/remote-status.ts";

const status = {
  schema: 1, mode: "read-only",
  server: { version: "beta-0.7.20", running: true, uptime_seconds: 130 },
  network: { active_connections: 3, accepted_connections: 20 },
  users: { online: 2, peak_online: 7 },
  chat: { received: 12, rejected: 1 },
};

test("remote browser accepts only safe aggregate fields and strips unknown private data", () => {
  const secret = "PRIVATE_UPSTREAM_IDENTITY";
  const parsed = parseRemoteSummaryResponse({
    ...status, secret, server: { ...status.server, secret },
    users: { ...status.users, nickname: secret },
    search: { queries: 6, results: 9, secret },
    transfers: { ctm: 4, rcm: 5, secret },
  });
  assert.deepEqual(parsed, {
    ...status,
    search: { queries: 6, results: 9 },
    transfers: { ctm: 4, rcm: 5 },
  });
  assert.ok(!JSON.stringify(parsed).includes(secret));
});

test("remote browser refuses malformed nested metrics instead of rendering or caching them", () => {
  const malformed = [
    null, [], { ...status, schema: 2 }, { ...status, mode: "write" },
    { ...status, server: { ...status.server, running: "false" } },
    { ...status, server: { ...status.server, version: { private: "value" } } },
    { ...status, network: { ...status.network, active_connections: "3" } },
    { ...status, users: { ...status.users, online: -1 } },
    { ...status, chat: { ...status.chat, received: Number.MAX_SAFE_INTEGER + 1 } },
    { ...status, chat: null },
    { ...status, search: { queries: "6", results: 9 } },
    { ...status, transfers: { ctm: 4, rcm: NaN } },
  ];
  for (const candidate of malformed) {
    assert.equal(parseRemoteSummaryResponse(candidate), null, JSON.stringify(candidate));
  }
  assert.deepEqual(parseRemoteSummaryResponse(status), status);
});

test("browser never fabricates source freshness for old v1 and rejects malformed source age", () => {
  assert.equal(parseRemoteSummaryResponse(status)?.observation, undefined);
  const valid = { ...status, observation: { schema: 1, source_age_ms: 250 } };
  assert.deepEqual(parseRemoteSummaryResponse(valid), valid);
  for (const invalid of [
    null, { schema: 2, source_age_ms: 250 }, { schema: 1, source_age_ms: -1 },
    { schema: 1, source_age_ms: 10_001 }, { schema: 1, source_age_ms: 10.5 },
    { schema: 1, source_age_ms: 50, actor: "private" },
  ]) assert.equal(parseRemoteSummaryResponse({ ...status, observation: invalid }), null);
});

test("browser redacted timeline accepts fixed bounded aggregates and refuses private identities", () => {
  const fresh = { ...status, observation: { schema: 1, source_age_ms: 100 } };
  const incidents = { schema: 1, state: "observed", events: [
    { kind: "hub_stopped", age_seconds: 2 }, { kind: "transport_refusals", age_seconds: 1 },
    { kind: "login_refusals", age_seconds: 1 }, { kind: "rate_limit_refusals", age_seconds: 1 },
  ], alerts: [
    { kind: "transport_refusals", age_seconds: 1 }, { kind: "login_refusals", age_seconds: 1 },
    { kind: "rate_limit_refusals", age_seconds: 1 },
  ] };
  assert.deepEqual(parseRemoteSummaryResponse({ ...fresh, incidents })?.incidents, incidents);
  for (const invalid of [
    { ...incidents, events: [{ kind: "login_refused", age_seconds: 1, actor: "7" }] },
    { ...incidents, events: [{ kind: "transport_refusals", age_seconds: -1 }] },
    { ...incidents, events: [{ kind: "hub_started", age_seconds: 601 }] },
    { ...incidents, alerts: [{ kind: "account_7", age_seconds: 0 }] },
    { ...incidents, events: Array(13).fill({ kind: "hub_started", age_seconds: 0 }) },
    { ...incidents, state: "unknown" },
  ]) assert.equal(parseRemoteSummaryResponse({ ...fresh, incidents: invalid }), null);
  assert.equal(parseRemoteSummaryResponse({ ...status, incidents }), null,
    "legacy unverified source data cannot assert a verified incident stream");
});

test("retained events are explicitly historical and cannot smuggle private actor fields", () => {
  const fresh = { ...status, observation: { schema: 1, source_age_ms: 100 } };
  const incidents = { schema: 1, state: "unknown", events: [], alerts: [],
    retained: { state: "available", events: [{ kind: "hub_stopped", age_seconds: 3 }] } };
  assert.deepEqual(parseRemoteSummaryResponse({ ...fresh, incidents })?.incidents?.retained, incidents.retained);
  for (const retained of [
    { state: "available", events: [{ kind: "hub_stopped", age_seconds: 0, actor: "private" }] },
    { state: "unknown", events: [{ kind: "hub_stopped", age_seconds: 0 }] },
    { state: "available", events: [{ kind: "login_refused", age_seconds: 1 }] },
    { state: "available", events: Array(13).fill({ kind: "hub_stopped", age_seconds: 1 }) },
    { state: "available", events: [{ kind: "hub_stopped", age_seconds: -1 }] },
  ]) assert.equal(parseRemoteSummaryResponse({ ...fresh, incidents: { ...incidents, retained } }), null);
});
