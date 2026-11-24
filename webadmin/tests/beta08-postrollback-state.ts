// VM-only verifier over the newly restored private destination after a real
// C++ package downgrade. No browser, live operator accounts or raw secrets.
import assert from "node:assert/strict";
import { isAbsolute, join } from "node:path";
import { loadTotpVerifier, totpCodeForTest } from "../server/remote-mfa.ts";
import { openPasskeyStore } from "../server/remote-passkeys.ts";
import { openRemoteAuditStore } from "../server/remote-audit-store.ts";
import { openRemoteRateLimitStore } from "../server/remote-rate-limit-store.ts";

const directory = process.argv[2];
if (!directory || !isAbsolute(directory) || process.env.CI !== "true" ||
    process.env.GITHUB_ACTIONS !== "true" || process.env.RUNNER_ENVIRONMENT !== "github-hosted" ||
    process.env.RUNNER_OS !== "Linux") throw Error("Isolated hosted rollback fixture required");

const at = 1_763_856_000_000;
const replay = loadTotpVerifier(join(directory, "mfa.json"), join(directory, "replay.jsonl"));
try {
  const code = totpCodeForTest("A".repeat(32), at);
  const firstUseExpected = process.argv[3] !== "--expect-consumed";
  assert.equal(replay.verify("7", code, at), firstUseExpected);
  assert.equal(replay.verify("7", code, at), false, "consumed TOTP step must remain denied");
} finally { replay.close(); }
const passkeys = openPasskeyStore(join(directory, "passkeys.json"), "localhost");
assert.equal(passkeys.epoch("7"), 2);
assert.equal(passkeys.forActor("7").length, 0, "revoked synthetic credential must remain denied");
const audit = openRemoteAuditStore(join(directory, "audit.jsonl"));
try { assert.equal(audit.verify(), true); assert.equal(audit.health().sequence, 1); }
finally { audit.close(); }
const limits = openRemoteRateLimitStore(join(directory, "rate-limits.json"), "a".repeat(64));
assert.equal(limits.healthy(), true);
process.stdout.write("Restored Node TOTP/replay, revoked passkey, audit and budget state accepted\n");
