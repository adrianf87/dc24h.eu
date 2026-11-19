import assert from "node:assert/strict";
import { test } from "node:test";
import { DASHBOARD_COUNTERS, parseDashboard } from "../src/lib/dashboard.ts";
import { parseSnapshot } from "../src/lib/status.ts";
import { searchSnapshot, snapshot, transferConfiguration } from "./fixtures.ts";
import { TRANSFER_COUNTERS } from "../src/lib/transfer-status.ts";
import { KICK_COUNTERS } from "../src/lib/kick.ts";
import { MUTE_COUNTERS } from "../src/lib/mute.ts";

const kick = Object.fromEntries(KICK_COUNTERS.map((key) => [key, key === "succeeded" ? "18446744073709551615" : "0"]));
const mute = Object.fromEntries(MUTE_COUNTERS.map((key) => [key, "0"]));
mute.applied = "7";
mute.public_blocked = "8";
mute.private_blocked = "9";
const transfers = {
  statistics: Object.fromEntries(TRANSFER_COUNTERS.map((key, index) => [key, index + 1])),
  configuration: transferConfiguration,
};
const dashboard = {
  schema: 1,
  core_running: true,
  database_enabled: false,
  uptime_seconds: "3661",
  users_online: "4",
  users_peak: "6",
  users_logins: "23",
  connections_active: "7",
  connections_peak: "9",
  connections_accepted: "48",
  connections_rejected: "5",
  connections_closed: "41",
  security_rejected: "2",
  security_closed: "1",
  adc_frames: "144",
  adcs_handshakes: "3",
  bytes_received: "4096",
  bytes_sent: "8192",
  chat_received: "12",
  chat_public: "5",
  chat_private: "3",
  chat_rejected: "2",
  chat_flood_limited: "1",
  search_queries: "1",
  search_routed: "2",
  search_rejected: "3",
  search_results: "11",
  search_tracked: "1",
  transfers_ctm: "1",
  transfers_rcm: "11",
  transfers_peer_unsupported: "21",
  transfers_peer_failed: "22",
  kicks_succeeded: "18446744073709551615",
  kicks_refused: "0",
  mutes_applied: "7",
  mutes_public_blocked: "8",
  mutes_private_blocked: "9",
};
const full = {
  ...snapshot,
  kick,
  mute,
  search: searchSnapshot,
  transfers,
  dashboard,
};

test("dashboard allowlist keeps decimal strings and drops unknown fields", () => {
  const parsed = parseDashboard({ ...dashboard, password: "DashboardSecret", nickname: "hidden" });
  assert.equal(parsed.schema, 1);
  assert.equal(parsed.kicks_succeeded, "18446744073709551615");
  assert.deepEqual(Object.keys(parsed).sort(), ["core_running", "database_enabled", "schema", ...DASHBOARD_COUNTERS].sort());
  assert.equal(JSON.stringify(parsed).includes("DashboardSecret"), false);
  assert.equal(parseSnapshot(snapshot).dashboard, undefined);
  const accepted = parseSnapshot({ ...full, dashboard: { ...dashboard, token: "DashboardSecret" } });
  assert.equal(accepted.dashboard?.bytes_received, "4096");
  assert.equal(JSON.stringify(accepted).includes("DashboardSecret"), false);
});

test("dashboard rejects a bad shape and a summary that disagrees with the snapshot", () => {
  for (const value of [null, [], {}, { ...dashboard, schema: 2 }, { ...dashboard, core_running: "true" }, { ...dashboard, bytes_received: 4096 }, { ...dashboard, users_online: "00" }, { ...dashboard, users_online: "-1" }, { ...dashboard, kicks_succeeded: "18446744073709551616" }]) {
    assert.throws(() => parseDashboard(value));
  }
  assert.throws(() => parseSnapshot({ ...full, dashboard: { ...dashboard, connections_active: "8" } }));
  assert.throws(() => parseSnapshot({ ...full, dashboard: { ...dashboard, core_running: false } }));
  assert.throws(() => parseSnapshot({ ...full, search: undefined }));
  assert.throws(() => parseSnapshot({ ...full, kick: { ...kick, succeeded: "1" } }));
  assert.throws(() => parseSnapshot({ ...full, dashboard: { ...dashboard, search_tracked: "2" } }));
});
