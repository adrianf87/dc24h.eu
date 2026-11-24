/**
 * Disposable Chromium client for the hosted C++ -> Node -> Apache recovery test.
 *
 * The Python harness owns every service and the stopped-writer SQL/five-store
 * checkpoint. Keep this Chromium process (and its virtual authenticator) alive
 * across that checkpoint, so the restored verifier must accept the *same*
 * independently held private credential through the real HTTP gateway.
 *
 * Stdin carries only synthetic ephemeral fixture credentials. Stdout is a
 * fixed-word protocol; never print a cookie, password, WebAuthn JSON or body.
 */
import { chromium } from "@playwright/test";
import { createInterface } from "node:readline";

// Playwright serializes page.evaluate callbacks into the browser. The tsx
// TypeScript loader transforms nested functions to __name(...) calls, but that
// module-local helper is not available inside Chromium. Keep this fixture as
// native ESM JavaScript so the exact callback runs without loader rewrites.
// Public output stays fixed-word only. This phase identifies the failed step
// without exposing browser exceptions, credentials, URLs or HTTP bodies.
let clientPhase = "startup";

const input = createInterface({ input: process.stdin, crlfDelay: Infinity });
const lines = input[Symbol.asyncIterator]();
function closeCommandInput() {
  // Returning the async iterator does not close readline's live stdin handle.
  // The Python harness deliberately keeps its command pipe open while it
  // waits for this child to exit. Release both handles on success and error.
  input.close();
  process.stdin.pause();
}
async function command() {
  const next = await lines.next();
  if (next.done) throw Error("Missing harness instruction");
  const decoded = JSON.parse(next.value);
  if (!decoded || typeof decoded !== "object" || Array.isArray(decoded)) throw Error("Invalid instruction");
  return decoded;
}

async function main() {
const startup = await command();
if (!/^https:\/\/localhost:\d{2,5}$/.test(startup.origin) ||
    !/^[1-9]\d{0,18}$/.test(startup.actor) ||
    !/^dc24h_remote=[0-9a-f]{64}$/.test(startup.sessionCookie) ||
    !/^[0-9a-f]{64}$/.test(startup.csrf)) throw Error("Invalid isolated fixture parameters");

clientPhase = "launch";
// The isolated Apache vhost listens on IPv4 loopback only. Keep Chromium on
// that same interface even on hosted images where localhost also has AAAA.
const browser = await chromium.launch({ headless: true,
  args: ["--host-resolver-rules=MAP localhost 127.0.0.1"],
});
try {
  clientPhase = "navigation";
  const context = await browser.newContext({ ignoreHTTPSErrors: true });
  const page = await context.newPage();
  // Chrome's network error is distinct from an HTTP 401/403. Keep only the
  // allowlisted error *kind* for this one disposable endpoint, never a full
  // request URL, cookie, JavaScript exception or response body.
  let optionsNetworkFailure = "unknown";
  let optionsRequestSeen = false;
  const enrollmentOptions = "/remote/v1/passkey/enroll/options";
  page.on("request", request => {
    if (new URL(request.url()).pathname === enrollmentOptions) optionsRequestSeen = true;
  });
  page.on("requestfailed", request => {
    if (new URL(request.url()).pathname !== enrollmentOptions) return;
    const error = request.failure()?.errorText ?? "";
    optionsNetworkFailure = /ERR_BLOCKED_BY_CSP/.test(error) ? "csp"
      : /ERR_CERT_/.test(error) ? "certificate"
      : /ERR_PROXY_/.test(error) ? "proxy"
      : /ERR_CONNECTION_/.test(error) ? "connection"
      : /ERR_SSL_/.test(error) ? "tls"
      : /ERR_HTTP2_/.test(error) ? "http2"
      : /ERR_ABORTED/.test(error) ? "aborted"
      : /ERR_BLOCKED_BY_CLIENT/.test(error) ? "client_blocked"
      : /ERR_FAILED/.test(error) ? "failed"
      : "other";
  });
  await context.addCookies([{ name: "dc24h_remote", value: startup.sessionCookie.split("=")[1],
    domain: "localhost", path: "/remote/v1", httpOnly: true, secure: true, sameSite: "Strict" }]);
  await page.goto(startup.origin + "/remote.html");
  // Exercise an actual browser fetch before creating a device. HTTP 401
  // indicates a session/cookie issue; a rejected fetch indicates a browser
  // transport issue. Neither is mistaken for a WebAuthn validation failure.
  const sessionPreflight = await page.evaluate(async () => {
    try { return (await fetch("/remote/v1/session")).status; }
    catch { return -1; }
  });
  if (sessionPreflight === -1) throw Error("enroll_session_network");
  if (sessionPreflight !== 200) throw Error(`enroll_session_${sessionPreflight}`);
  clientPhase = "authenticator";
  const cdp = await context.newCDPSession(page);
  await cdp.send("WebAuthn.enable");
  await cdp.send("WebAuthn.addVirtualAuthenticator", { options: {
    protocol: "ctap2", transport: "internal", hasResidentKey: true,
    hasUserVerification: true, isUserVerified: true,
  } });

  clientPhase = "enrollment";
  const enrolled = await page.evaluate(async csrf => {
    // Report only the fixed operation category. A full Playwright exception
    // could contain a URL, request options or private WebAuthn material.
    let operation = "options_fetch";
    try {
    const decode = value => Uint8Array.from(
      atob(value.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(value.length / 4) * 4, "=")),
      character => character.charCodeAt(0));
    const optionsResponse = await fetch("/remote/v1/passkey/enroll/options", {
      method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": csrf }, body: "{}",
    });
    if (optionsResponse.status !== 200) return `enroll_options_${optionsResponse.status}`;
    operation = "options_json";
    const options = await optionsResponse.json();
    operation = "create";
    let credential;
    try {
      credential = await navigator.credentials.create({ publicKey: {
        ...options, challenge: decode(options.challenge),
        user: { ...options.user, id: decode(options.user.id) },
        excludeCredentials: (options.excludeCredentials ?? []).map(entry =>
          ({ ...entry, id: decode(entry.id) })),
      } });
    } catch (error) {
      const name = error instanceof DOMException ? error.name : "";
      return name === "NotAllowedError" ? "enroll_create_not_allowed"
        : name === "SecurityError" ? "enroll_create_security"
        : name === "NotSupportedError" ? "enroll_create_unsupported"
        : name === "InvalidStateError" ? "enroll_create_invalid_state"
        : name === "TypeError" ? "enroll_create_type"
        : "enroll_create_exception";
    }
    if (!credential || !("toJSON" in credential)) return "enroll_no_credential";
    operation = "finish_build";
    const response = credential.toJSON();
    operation = "finish_fetch";
    const finish = await fetch("/remote/v1/passkey/enroll/finish", {
      method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": csrf },
      body: JSON.stringify({ response }),
    });
    if (finish.status !== 200) return `enroll_finish_${finish.status}`;
    operation = "finish_json";
    if ((await finish.json()).enrolled !== true) return "enroll_result_invalid";
    operation = "session_fetch";
    const session = await fetch("/remote/v1/session");
    if (session.status !== 200) return `enroll_session_${session.status}`;
    return "ok";
    } catch {
      return `enroll_${operation}_exception`;
    }
  }, startup.csrf).catch(() => "enroll_evaluate_exception");
  if (enrolled === "enroll_options_fetch_exception") {
    // Deliver the matching requestfailed event before categorizing it.
    await page.waitForTimeout(100);
    throw Error(`enroll_options_network_${optionsRequestSeen ? optionsNetworkFailure : "no_request"}`);
  }
  if (enrolled !== "ok") throw Error(enrolled);
  process.stdout.write("enrolled\n");

  clientPhase = "restore_handoff";
  const next = await command();
  if (next.action !== "restore") throw Error("Restored database handoff missing");
  // The original authenticated cookie has no authority in the new Node
  // process. A new C++ password grant and a fresh challenge are mandatory.
  clientPhase = "restored_login";
  const restored = await page.evaluate(async actor => {
    // This is the *rotated* live browser cookie from enrollment, not an
    // already-invalid pre-enrollment cookie retained by the Python harness.
    if ((await fetch("/remote/v1/session")).status !== 401) return "restored_old_cookie";
    const decode = value => Uint8Array.from(
      atob(value.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(value.length / 4) * 4, "=")),
      character => character.charCodeAt(0));
    const challengeResponse = await fetch("/remote/v1/challenge");
    if (challengeResponse.status !== 200) return `restored_challenge_${challengeResponse.status}`;
    const { csrf } = await challengeResponse.json();
    const optionsResponse = await fetch("/remote/v1/passkey/login/options", {
      method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": csrf },
      body: JSON.stringify({ actor_id: actor,
        password: "WebAdmin operator password!" }),
    });
    if (optionsResponse.status !== 200) return `restored_options_${optionsResponse.status}`;
    const options = await optionsResponse.json();
    const credential = await navigator.credentials.get({ publicKey: {
      ...options, challenge: decode(options.challenge),
        allowCredentials: (options.allowCredentials ?? []).map(entry =>
        ({ ...entry, id: decode(entry.id) })),
    } });
    if (!credential || !("toJSON" in credential)) return "restored_no_credential";
    const response = credential.toJSON();
    const finish = () => fetch("/remote/v1/passkey/login/finish", {
      method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": csrf },
      body: JSON.stringify({ response }),
    });
    const success = await finish();
    if (success.status !== 200) return `restored_finish_${success.status}`;
    const authenticated = await success.json();
    if (!authenticated.authenticated || !authenticated.mfa || authenticated.mode !== "read-only")
      return "restored_grant_invalid";
    const replay = await finish();
    if (replay.status !== 403) return `restored_replay_${replay.status}`; // consumed challenge
    const summary = await fetch("/remote/v1/summary");
    if (summary.status !== 200) return `restored_summary_${summary.status}`;
    const devices = await fetch("/remote/v1/passkey/devices");
    if (devices.status !== 200) return `restored_devices_${devices.status}`;
    const list = (await devices.json()).devices;
    if (!Array.isArray(list) || list.length !== 1 || typeof list[0].id !== "string")
      return "restored_device_mismatch";
    const revoked = await fetch("/remote/v1/passkey/device/revoke", {
      method: "POST", headers: { "Content-Type": "application/json",
        "X-DC24H-Remote-CSRF": authenticated.csrf },
      body: JSON.stringify({ id: list[0].id }),
    });
    if (revoked.status !== 200) return `restored_revoke_${revoked.status}`;
    if (!(await revoked.json()).revoked) return "restored_revoke_invalid";
    const gone = await fetch("/remote/v1/session");
    if (gone.status !== 401) return `restored_session_${gone.status}`;
    return "ok";
  }, startup.actor);
  if (restored !== "ok") throw Error(restored);
} finally {
  try {
    await browser.close();
  } finally {
    closeCommandInput();
  }
}
// The harness must never accept success before Chromium and the IPC channel
// have been cleanly shut down; otherwise a hung child masquerades as success.
process.stdout.write("authenticated_and_revoked\n");
}

await main().catch(async error => {
  // A runner may collect stdout. Emit only fixed failure categories and
  // integer HTTP statuses, never browser errors with echoed options/cookies.
  const message = error instanceof Error ? error.message : "";
  const category = /^(?:enroll_(?:options|finish|session)_[0-9]{3}|enroll_session_network|enroll_(?:options_fetch|options_json|create|finish_build|finish_fetch|finish_json|session_fetch|evaluate)_exception|enroll_options_network_(?:unknown|no_request|csp|certificate|proxy|connection|tls|http2|aborted|client_blocked|failed|other)|enroll_no_credential|enroll_result_invalid|enroll_create_(?:not_allowed|security|unsupported|invalid_state|type|exception)|restored_(?:challenge|options|finish|replay|summary|devices|revoke|session)_[0-9]{3}|restored_(?:old_cookie|no_credential|grant_invalid|device_mismatch|revoke_invalid))$/.test(message)
    ? message : message === "Invalid isolated fixture parameters" ? "fixture_parameters"
    : message === "Missing harness instruction" ? "harness_protocol"
    : message === "Restored browser C++ password/passkey grant or revocation refused" ? "restored_flow"
    : `${clientPhase}_exception`;
  closeCommandInput();
  process.stdout.write(`client_error:${category}\n`);
  process.exitCode = 1;
});
