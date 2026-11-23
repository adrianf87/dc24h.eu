import assert from "node:assert/strict";
import { test } from "node:test";
import { createRemoteSecurityMonitor } from "../server/remote-security-monitor.ts";

test("security monitor emits bounded reason-only alerts with cooldown", () => {
  const lines: string[] = [];
  const monitor = createRemoteSecurityMonitor(line => lines.push(line));
  for (let n = 0; n < 25; n++) monitor.observe("login_refused", 100_000 + n);
  assert.equal(lines.length, 1);
  monitor.observe("api_unavailable", 101_000);
  monitor.observe("api_unavailable", 101_001);
  monitor.observe("api_unavailable", 101_002);
  monitor.observe("rate_limiter_unavailable", 101_003);
  monitor.signal("audit_corruption", 101_004);
  monitor.signal("audit_loss", 101_005);
  monitor.signal("audit_loss", 101_006);
  assert.deepEqual(lines.map(line => JSON.parse(line).reason), [
    "authentication_denials", "upstream_outage", "rate_limiter_loss", "audit_corruption", "audit_loss",
  ]);
  for (const line of lines) {
    assert.ok(line.length < 170 && line.endsWith("\n"));
    for (const secret of ["actor", "password", "cookie", "ip", "credential", "csrf", "token"])
      assert.ok(!line.includes('"' + secret + '"'));
  }
  monitor.observe("login_refused", 161_000);
  for (let n = 0; n < 9; n++) monitor.observe("mfa_refused", 161_010 + n);
  assert.equal(lines.filter(line => line.includes("authentication_denials")).length, 2,
    "new window can send one new alert");
});
