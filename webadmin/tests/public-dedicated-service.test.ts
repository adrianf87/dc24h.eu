import assert from "node:assert/strict";
import { test } from "node:test";
import { publicBridgeCredentials } from "../server/public-bridge-credentials.ts";
import { dedicatedMsmtpConfigReady, createMsmtpSender } from "../server/public-msmtp.ts";

test("Stage IV refuses the legacy same-UID hub token and missing bridge", () => {
  assert.throws(() => publicBridgeCredentials({}));
  assert.throws(() => publicBridgeCredentials({ DC24H_STAGE4_BRIDGE_SOCKET: "/tmp/bridge.sock" }));
  assert.throws(() => publicBridgeCredentials({
    DC24H_STAGE4_BRIDGE_SOCKET: "/run/dc24h-stage4-bridge/bridge.sock",
    DC24H_API_TOKEN_FILE: "/run/dc24h/api.token",
  }));
  assert.throws(() => publicBridgeCredentials({
    DC24H_STAGE4_BRIDGE_SOCKET: "/run/dc24h-stage4-bridge/bridge.sock",
    DC24H_API_SOCKET: "/run/dc24h/api.sock",
  }));
});

test("Stage IV email sender fails closed without dedicated non-root configuration", async () => {
  if (!dedicatedMsmtpConfigReady()) {
    assert.equal(await createMsmtpSender()("admin@dc24h.eu", "12345678"), false);
  }
  assert.throws(() => createMsmtpSender("/bin/sh"));
  assert.equal(await createMsmtpSender()("bad\nrecipient@dc24h.eu", "12345678"), false);
});
