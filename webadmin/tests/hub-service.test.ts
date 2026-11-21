import assert from "node:assert/strict";
import {test} from "node:test";
import {currentServiceUsers, parseHubServiceResult, parseHubServiceStatus, serviceConfirmation} from "../src/lib/hub-service.ts";

test("fixed service status is parsed and untrusted fields are stripped", () => {
  const status = {schema: 1, available: true, unit: "dc24h.service", load_state: "loaded",
    active_state: "active", sub_state: "running", result: "success"};
  assert.deepEqual(parseHubServiceStatus({...status, token: "hidden"}), status);
  assert.deepEqual(parseHubServiceStatus({schema: 1, available: false, error: "systemd_unavailable", secret: "hidden"}),
    {schema: 1, available: false, error: "systemd_unavailable"});
  for (const wrong of [{...status, unit: "ssh.service"}, {...status, active_state: "active\nscript"},
    {...status, available: "true"}, {...status, schema: 2},
    {schema: 1, available: false, error: "arbitrary"}]) assert.throws(() => parseHubServiceStatus(wrong));
});

test("exact action result and typed confirmation", () => {
  for (const operation of ["start", "stop", "restart"] as const) {
    const result = {operation, outcome: "succeeded" as const, active_state: operation === "stop" ? "inactive" as const : "active" as const};
    assert.deepEqual(parseHubServiceResult({...result, token: "hidden"}), result);
    assert.equal(serviceConfirmation(operation), operation.toUpperCase());
    assert.throws(() => parseHubServiceResult({...result, active_state: operation === "stop" ? "active" : "inactive"}));
  }
  assert.throws(() => parseHubServiceResult({operation: "delete", outcome: "succeeded", active_state: "active"}));
  assert.throws(() => parseHubServiceResult({operation: "start", outcome: "started", active_state: "active"}));
});

test("service user count requires telemetry observed after a service action", () => {
  const snapshot = {server: {core_running: true}, users: {online: 4}};
  const now = 20000;
  assert.equal(currentServiceUsers(snapshot, 19000, null, true, now), 4);
  assert.equal(currentServiceUsers(snapshot, 19000, 19500, true, now), null);
  assert.equal(currentServiceUsers(snapshot, 19500, 19500, true, now), null);
  assert.equal(currentServiceUsers({...snapshot, users: {online: 0}}, 19600, 19500, true, now), 0);
  assert.equal(currentServiceUsers(snapshot, 19000, null, false, now), null);
  assert.equal(currentServiceUsers(snapshot, 4000, null, true, now), null);
  assert.equal(currentServiceUsers(null, null, null, true, now), null);
});
