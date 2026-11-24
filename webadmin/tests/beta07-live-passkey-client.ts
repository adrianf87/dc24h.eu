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

type Start = { origin: string; actor: string; sessionCookie: string; csrf: string };
// Public output stays fixed-word only. This phase identifies the failed step
// without exposing browser exceptions, credentials, URLs or HTTP bodies.
let clientPhase = "startup";

const lines = createInterface({ input: process.stdin, crlfDelay: Infinity })[Symbol.asyncIterator]();
async function command(): Promise<Record<string, unknown>> {
  const next = await lines.next();
  if (next.done) throw Error("Missing harness instruction");
  const decoded: unknown = JSON.parse(next.value);
  if (!decoded || typeof decoded !== "object" || Array.isArray(decoded)) throw Error("Invalid instruction");
  return decoded as Record<string, unknown>;
}

async function main() {
const startup = await command() as Start;
if (!/^https:\/\/localhost:\d{2,5}$/.test(startup.origin) ||
    !/^[1-9]\d{0,18}$/.test(startup.actor) ||
    !/^dc24h_remote=[0-9a-f]{64}$/.test(startup.sessionCookie) ||
    !/^[0-9a-f]{64}$/.test(startup.csrf)) throw Error("Invalid isolated fixture parameters");

clientPhase = "launch";
const browser = await chromium.launch({ headless: true });
try {
  clientPhase = "navigation";
  const context = await browser.newContext({ ignoreHTTPSErrors: true });
  const page = await context.newPage();
  await context.addCookies([{ name: "dc24h_remote", value: startup.sessionCookie.split("=")[1],
    domain: "localhost", path: "/remote/v1", httpOnly: true, secure: true, sameSite: "Strict" }]);
  await page.goto(startup.origin + "/remote.html");
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
    const decode = (value: string) => Uint8Array.from(
      atob(value.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(value.length / 4) * 4, "=")),
      character => character.charCodeAt(0));
    const optionsResponse = await fetch("/remote/v1/passkey/enroll/options", {
      method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": csrf }, body: "{}",
    });
    if (optionsResponse.status !== 200) return `enroll_options_${optionsResponse.status}`;
    operation = "options_json";
    const options = await optionsResponse.json();
    operation = "create";
    let credential: Credential | null;
    try {
      credential = await navigator.credentials.create({ publicKey: {
        ...options, challenge: decode(options.challenge),
        user: { ...options.user, id: decode(options.user.id) },
        excludeCredentials: (options.excludeCredentials ?? []).map((entry: { id: string }) =>
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
    const response = (credential as PublicKeyCredential & { toJSON(): unknown }).toJSON();
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
    if ((await fetch("/remote/v1/session")).status !== 401) return false;
    const decode = (value: string) => Uint8Array.from(
      atob(value.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(value.length / 4) * 4, "=")),
      character => character.charCodeAt(0));
    const challengeResponse = await fetch("/remote/v1/challenge");
    if (challengeResponse.status !== 200) return false;
    const { csrf } = await challengeResponse.json();
    const optionsResponse = await fetch("/remote/v1/passkey/login/options", {
      method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": csrf },
      body: JSON.stringify({ actor_id: actor,
        password: "WebAdmin operator password!" }),
    });
    if (optionsResponse.status !== 200) return false;
    const options = await optionsResponse.json();
    const credential = await navigator.credentials.get({ publicKey: {
      ...options, challenge: decode(options.challenge),
      allowCredentials: (options.allowCredentials ?? []).map((entry: { id: string }) =>
        ({ ...entry, id: decode(entry.id) })),
    } });
    if (!credential || !("toJSON" in credential)) return false;
    const response = (credential as PublicKeyCredential & { toJSON(): unknown }).toJSON();
    const finish = () => fetch("/remote/v1/passkey/login/finish", {
      method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": csrf },
      body: JSON.stringify({ response }),
    });
    const success = await finish();
    if (success.status !== 200) return false;
    const authenticated = await success.json();
    if (!authenticated.authenticated || !authenticated.mfa || authenticated.mode !== "read-only") return false;
    if ((await finish()).status !== 403) return false; // challenge is consumed
    if ((await fetch("/remote/v1/summary")).status !== 200) return false;
    const devices = await fetch("/remote/v1/passkey/devices");
    if (devices.status !== 200) return false;
    const list = (await devices.json()).devices;
    if (!Array.isArray(list) || list.length !== 1 || typeof list[0].id !== "string") return false;
    const revoked = await fetch("/remote/v1/passkey/device/revoke", {
      method: "POST", headers: { "Content-Type": "application/json",
        "X-DC24H-Remote-CSRF": authenticated.csrf },
      body: JSON.stringify({ id: list[0].id }),
    });
    if (revoked.status !== 200 || !(await revoked.json()).revoked) return false;
    return (await fetch("/remote/v1/session")).status === 401;
  }, startup.actor);
  if (!restored) throw Error("Restored browser C++ password/passkey grant or revocation refused");
  process.stdout.write("authenticated_and_revoked\n");
} finally {
  await browser.close();
  lines.return?.();
}
}

await main().catch(async (error: unknown) => {
  // A runner may collect stdout. Emit only fixed failure categories and
  // integer HTTP statuses, never browser errors with echoed options/cookies.
  const message = error instanceof Error ? error.message : "";
  const category = /^(?:enroll_(?:options|finish|session)_[0-9]{3}|enroll_(?:options_fetch|options_json|create|finish_build|finish_fetch|finish_json|session_fetch|evaluate)_exception|enroll_no_credential|enroll_result_invalid|enroll_create_(?:not_allowed|security|unsupported|invalid_state|type|exception))$/.test(message)
    ? message : message === "Invalid isolated fixture parameters" ? "fixture_parameters"
    : message === "Missing harness instruction" ? "harness_protocol"
    : message === "Restored browser C++ password/passkey grant or revocation refused" ? "restored_flow"
    : `${clientPhase}_exception`;
  process.stdout.write(`client_error:${category}\n`);
  process.exitCode = 1;
  await lines.return?.();
});
