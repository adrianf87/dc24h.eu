import assert from "node:assert/strict";
import { test } from "node:test";
import { remoteAuditEvents, remoteAuditRecord } from "../server/remote-audit.ts";
import type { RemoteEvent } from "../server/remote-gateway.ts";

test("all fixed gateway audit events serialize as bounded, nonidentifying JSON lines", () => {
  assert.equal(remoteAuditEvents.length, 8);
  for (const event of remoteAuditEvents) {
    const line = remoteAuditRecord(event, 1_700_000_000_000);
    assert.ok(line.endsWith("\n") && line.length < 150);
    assert.deepEqual(JSON.parse(line), {
      schema: 1, component: "remote-gateway", event, at: "2023-11-14T22:13:20.000Z",
    });
    for (const forbidden of ["password", "actor_id", "otp", "cookie", "grant", "remote_addr", "csrf"])
      assert.ok(!line.includes(forbidden), forbidden);
  }
});

test("invalid event or clock values are refused, not interpolated into journal output", () => {
  for (const item of ["login_ok\npassword=secret", "otp=123456", "unaudited_success", ""]) {
    assert.throws(() => remoteAuditRecord(item as RemoteEvent, Date.now()), /Invalid local audit event/);
  }
  for (const item of [-1, NaN, Infinity, 1.5, Number.MAX_SAFE_INTEGER]) {
    assert.throws(() => remoteAuditRecord("login_refused", item), /Invalid local audit event/);
  }
});
