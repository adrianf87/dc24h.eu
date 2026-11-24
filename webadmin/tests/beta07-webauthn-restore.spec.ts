import { test, expect } from "@playwright/test";
import { createServer } from "node:https";
import { spawnSync } from "node:child_process";
import { chmodSync, mkdirSync, mkdtempSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import type { AddressInfo } from "node:net";
import { initializePasskeyStore, openPasskeyStore } from "../server/remote-passkeys.ts";
import { passkeyCeremonies } from "../server/remote-webauthn.ts";

// This is an actual Chromium/WebAuthn cryptographic ceremony against a real
// file-backed verifier, not a Playwright route mock. It does not impersonate a
// C++ password/grant check or claim the entire production gateway is tested.
test.use({ ignoreHTTPSErrors: true });

test("real virtual authenticator authenticates after fresh private-file restore and refuses replay", async ({ page }) => {
  test.setTimeout(60_000);
  const base = mkdtempSync(join(tmpdir(), "beta07-webauthn-")); chmodSync(base, 0o700);
  const source = join(base, "source"), snapshot = join(base, "snapshot"), restored = join(base, "restored");
  const cert = join(base, "leaf.pem"), key = join(base, "leaf.key");
  const recoveryScript = fileURLToPath(new URL("../../scripts/stage4_private_recovery_drill.py", import.meta.url));
  const generated = spawnSync("openssl", ["req", "-x509", "-newkey", "rsa:2048", "-nodes",
    "-days", "1", "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
    "-keyout", key, "-out", cert], { stdio: "pipe", timeout: 15000 });
  if (generated.status !== 0) { rmSync(base, { recursive: true, force: true }); throw Error("Isolated TLS fixture setup refused"); }
  mkdirSync(source, { mode: 0o700 });
  const sourceFile = join(source, "passkeys.json"), restoredFile = join(restored, "passkeys.json");
  initializePasskeyStore(sourceFile, "localhost");
  // The restored passkey must be part of a verified, matched five-file
  // private snapshot, not an unverified individual passkeys.json copy.
  for (const [name, value] of Object.entries({
    "mfa.json": JSON.stringify({ schema: 1, accounts: { "7": "A".repeat(32) } }) + "\n",
    "rate-limits.json": JSON.stringify({ schema: 1, buckets: [] }) + "\n",
    "replay.jsonl": "", "audit.jsonl": "",
  })) writeFileSync(join(source, name), value, { mode: 0o600, flag: "wx" });
  let store = openPasskeyStore(sourceFile, "localhost");
  let origin = "";
  let ceremonies!: ReturnType<typeof passkeyCeremonies>;
  let enrollmentChallenge = "", loginChallenge = "";
  const responses: Array<{ path: string; status: number }> = [];

  const server = createServer({ key: readFileSync(key), cert: readFileSync(cert) }, async (req, res) => {
    const respond = (status: number, value: unknown) => {
      responses.push({ path: req.url ?? "", status });
      res.writeHead(status, { "content-type": "application/json", "cache-control": "no-store" });
      res.end(JSON.stringify(value));
    };
    if (req.method === "GET" && req.url === "/") {
      res.writeHead(200, { "content-type": "text/html", "cache-control": "no-store" });
      res.end("<!doctype html><title>Isolated WebAuthn fixture</title>"); return;
    }
    if (req.method !== "POST") { respond(404, { error: "not_found" }); return; }
    try {
      let payload = "";
      for await (const chunk of req) {
        payload += chunk.toString("utf8");
        if (payload.length > 32768) throw Error("oversize");
      }
      const fields = JSON.parse(payload) as Record<string, unknown>;
      if (fields.actor !== "7") throw Error("actor_refused");
      if (req.url === "/enroll/options") {
        const options = await ceremonies.beginEnrollment("7");
        enrollmentChallenge = options.challenge;
        respond(200, options); return;
      }
      if (req.url === "/enroll/finish") {
        const challenge = enrollmentChallenge; enrollmentChallenge = "";
        if (!challenge || !fields.response || !await ceremonies.finishEnrollment(
          "7", challenge, fields.response as Parameters<typeof ceremonies.finishEnrollment>[2], Date.now())) throw Error();
        respond(200, { enrolled: true }); return;
      }
      if (req.url === "/login/options") {
        if (fields.password !== "synthetic-password") throw Error("password_required");
        const options = await ceremonies.beginLogin("7");
        if (!options) throw Error("no_credentials");
        loginChallenge = options.challenge;
        respond(200, options); return;
      }
      if (req.url === "/login/finish") {
        const challenge = loginChallenge; loginChallenge = "";
        if (!challenge || !fields.response || !await ceremonies.finishLogin(
          "7", challenge, fields.response as Parameters<typeof ceremonies.finishLogin>[2], Date.now())) throw Error();
        respond(200, { authenticated: true }); return;
      }
      respond(404, { error: "not_found" });
    } catch {
      respond(403, { error: "refused" });
    }
  });
  try {
    await new Promise<void>((resolve, reject) => {
      server.once("error", reject);
      server.listen(0, "localhost", resolve);
    });
    origin = `https://localhost:${(server.address() as AddressInfo).port}`;
    ceremonies = passkeyCeremonies(store, origin, "localhost");
    await page.goto(origin);
    const cdp = await page.context().newCDPSession(page);
    await cdp.send("WebAuthn.enable");
    const { authenticatorId } = await cdp.send("WebAuthn.addVirtualAuthenticator", {
      options: { protocol: "ctap2", transport: "internal", hasResidentKey: true,
        hasUserVerification: true, isUserVerified: true },
    });

    const enroll = await page.evaluate(async () => {
      const bytes = (s: string) => Uint8Array.from(atob(s.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(s.length / 4) * 4, "=")), c => c.charCodeAt(0));
      const options = await (await fetch("/enroll/options", { method: "POST", body: JSON.stringify({ actor: "7" }) })).json();
      const credential = await navigator.credentials.create({ publicKey: {
        ...options, challenge: bytes(options.challenge), user: { ...options.user, id: bytes(options.user.id) },
        excludeCredentials: (options.excludeCredentials ?? []).map((c: { id: string }) => ({ ...c, id: bytes(c.id) })),
      } });
      if (!credential || !("toJSON" in credential)) throw Error("Browser WebAuthn JSON missing");
      return (credential as PublicKeyCredential & { toJSON(): unknown }).toJSON();
    });
    const enrolled = await page.evaluate(async response => {
      const answer = await fetch("/enroll/finish", { method: "POST", body: JSON.stringify({ actor: "7", response }) });
      return answer.status;
    }, enroll);
    expect(enrolled).toBe(200);
    expect(store.forActor("7")).toHaveLength(1);

    // Close the actual HTTPS writer before asserting --ack-stopped. The
    // authenticator retains its key in Chromium while we snapshot offline.
    await new Promise<void>(resolve => server.close(() => resolve()));
    // The same recovery tool as the Ubuntu VM tests copies and verifies all
    // five stores, creating fresh directories with owner-only permissions.
    const recovery = (action: "snapshot" | "restore", from: string, to: string) =>
      spawnSync("python3", [recoveryScript, action, "--source", from,
        "--destination", to, "--ack-stopped", "--ack-isolated"],
      { stdio: "pipe", timeout: 15000 });
    if (recovery("snapshot", source, snapshot).status !== 0 ||
        recovery("restore", snapshot, restored).status !== 0)
      throw Error("Matched private-state browser recovery refused");
    for (const name of ["mfa.json", "replay.jsonl", "audit.jsonl", "passkeys.json", "rate-limits.json"]) {
      expect(readFileSync(join(restored, name))).toEqual(readFileSync(join(source, name)));
      expect(statSync(join(restored, name)).mode & 0o077).toBe(0);
    }
    store = openPasskeyStore(restoredFile, "localhost");
    ceremonies = passkeyCeremonies(store, origin, "localhost");
    await new Promise<void>((resolve, reject) => {
      server.once("error", reject);
      server.listen(Number(new URL(origin).port), "localhost", resolve);
    });
    const assertion = await page.evaluate(async () => {
      const bytes = (s: string) => Uint8Array.from(atob(s.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(s.length / 4) * 4, "=")), c => c.charCodeAt(0));
      const optionsResponse = await fetch("/login/options", { method: "POST", body: JSON.stringify({ actor: "7", password: "synthetic-password" }) });
      if (optionsResponse.status !== 200) throw Error("Login option refusal");
      const options = await optionsResponse.json();
      const credential = await navigator.credentials.get({ publicKey: {
        ...options, challenge: bytes(options.challenge),
        allowCredentials: (options.allowCredentials ?? []).map((c: { id: string }) => ({ ...c, id: bytes(c.id) })),
      } });
      if (!credential || !("toJSON" in credential)) throw Error("Browser WebAuthn JSON missing");
      return (credential as PublicKeyCredential & { toJSON(): unknown }).toJSON();
    });
    const finish = async (response: unknown) => page.evaluate(async r => {
      const answer = await fetch("/login/finish", { method: "POST", body: JSON.stringify({ actor: "7", response: r }) });
      return answer.status;
    }, response);
    expect(await finish(assertion)).toBe(200);
    expect(await finish(assertion)).toBe(403); // One-time challenge consumed.
    expect(store.get("7", (assertion as { id: string }).id)?.lastUsedAt).not.toBeNull();
    // A browser-origin credential cannot be accepted by a restored verifier
    // accidentally configured for a different HTTPS origin/port.
    ceremonies = passkeyCeremonies(store, origin.replace(/:\d+$/, ":65535"), "localhost");
    const wrongOriginAssertion = await page.evaluate(async () => {
      const bytes = (s: string) => Uint8Array.from(atob(s.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(s.length / 4) * 4, "=")), c => c.charCodeAt(0));
      const options = await (await fetch("/login/options", { method: "POST",
        body: JSON.stringify({ actor: "7", password: "synthetic-password" }) })).json();
      const credential = await navigator.credentials.get({ publicKey: {
        ...options, challenge: bytes(options.challenge),
        allowCredentials: (options.allowCredentials ?? []).map((c: { id: string }) => ({ ...c, id: bytes(c.id) })),
      } });
      if (!credential || !("toJSON" in credential)) throw Error("Browser WebAuthn JSON missing");
      return (credential as PublicKeyCredential & { toJSON(): unknown }).toJSON();
    });
    expect(await finish(wrongOriginAssertion)).toBe(403);
    ceremonies = passkeyCeremonies(store, origin, "localhost");
    expect(store.revoke("7", (assertion as { id: string }).id, Date.now())).toBe(true);
    const afterRevoke = await page.evaluate(async () => (await fetch("/login/options", {
      method: "POST", body: JSON.stringify({ actor: "7", password: "synthetic-password" }),
    })).status);
    expect(afterRevoke).toBe(403);
    expect(responses.filter(r => r.path === "/login/finish").map(r => r.status)).toEqual([200, 403, 403]);
    await cdp.send("WebAuthn.removeVirtualAuthenticator", { authenticatorId });
  } finally {
    if (server.listening) await new Promise<void>(resolve => server.close(() => resolve()));
    rmSync(base, { recursive: true, force: true });
  }
});
