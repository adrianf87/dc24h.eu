import assert from "node:assert/strict";
import { test } from "node:test";
import { createHash, createSign, generateKeyPairSync, randomBytes } from "node:crypto";
import { mkdtempSync, chmodSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { isoCBOR } from "@simplewebauthn/server/helpers";
import { initializePasskeyStore, openPasskeyStore } from "../server/remote-passkeys.ts";
import { passkeyCeremonies, relyingParty } from "../server/remote-webauthn.ts";

const origin = "https://localhost:8443", rpID = "localhost";
const toBase64 = (bytes: Uint8Array) => Buffer.from(bytes).toString("base64url");
const client = (type: string, challenge: string, from = origin) =>
  Buffer.from(JSON.stringify({ type, challenge, origin: from, crossOrigin: false }));
const sha = (data: string | Uint8Array) => createHash("sha256").update(data).digest();

function authenticator() {
  const { publicKey, privateKey } = generateKeyPairSync("ec", { namedCurve: "prime256v1" });
  const jwk = publicKey.export({ format: "jwk" });
  const cose = isoCBOR.encode(new Map<number, number | Uint8Array>([
    [1, 2], [3, -7], [-1, 1], [-2, Buffer.from(jwk.x!, "base64url")], [-3, Buffer.from(jwk.y!, "base64url")],
  ]));
  const id = randomBytes(32);
  const counter = (num: number) => { const b = Buffer.alloc(4); b.writeUInt32BE(num); return b; };
  const registration = (challenge: string, from = origin, flags = 0x45, rp = rpID) => {
    const data = Buffer.concat([sha(rp), Buffer.from([flags]), counter(0), Buffer.alloc(16),
      Buffer.from([0, id.length]), id, Buffer.from(cose)]);
    const attestationObject = isoCBOR.encode(new Map<string, string | Uint8Array | Map<string, string>>([
      ["fmt", "none"], ["attStmt", new Map<string, string>()], ["authData", data],
    ]));
    return { id: toBase64(id), rawId: toBase64(id), type: "public-key" as const,
      response: { clientDataJSON: toBase64(client("webauthn.create", challenge, from)),
        attestationObject: toBase64(attestationObject), transports: ["internal"] },
      clientExtensionResults: {}, };
  };
  const assertion = (challenge: string, userHandle: string, num = 1, from = origin,
    flags = 0x05, rp = rpID, type = "webauthn.get") => {
    const data = Buffer.concat([sha(rp), Buffer.from([flags]), counter(num)]);
    const json = client(type, challenge, from);
    const signature = createSign("SHA256").update(Buffer.concat([data, sha(json)])).sign(privateKey);
    return { id: toBase64(id), rawId: toBase64(id), type: "public-key" as const,
      response: { clientDataJSON: toBase64(json), authenticatorData: toBase64(data),
        signature: toBase64(signature), userHandle }, clientExtensionResults: {} };
  };
  return { registration, assertion };
}

test("genuine ES256 WebAuthn registration and assertion reject wrong Origin, challenge, replay and account", async () => {
  const dir = mkdtempSync(join(tmpdir(), "dc24h-webauthn-")); chmodSync(dir, 0o700);
  try {
    initializePasskeyStore(join(dir, "passkeys.json"), rpID);
    const store = openPasskeyStore(join(dir, "passkeys.json"), rpID);
    const service = passkeyCeremonies(store, origin, rpID);
    const device = authenticator();
    const opts = await service.beginEnrollment("7");
    assert.equal(opts.rp.id, rpID);
    assert.equal(opts.authenticatorSelection?.userVerification, "required");
    await assert.rejects(() => service.finishEnrollment("7", opts.challenge,
      device.registration(opts.challenge, "https://evil.example"), 1000));
    for (const [label, response] of [
      ["RP ID hash", device.registration(opts.challenge, origin, 0x45, "evil.example")],
      ["missing user verification", device.registration(opts.challenge, origin, 0x41)],
      ["missing user presence", device.registration(opts.challenge, origin, 0x44)],
      ["wrong challenge", device.registration("replayed-registration-challenge")],
    ] as const) {
      await assert.rejects(() => service.finishEnrollment("7", opts.challenge, response, 1000), label);
      assert.equal(store.forActor("7").length, 0, label + " must not persist a credential");
    }
    assert.equal(store.forActor("7").length, 0);
    assert.equal(await service.finishEnrollment("7", opts.challenge, device.registration(opts.challenge), 1000), true);
    assert.equal(store.forActor("7").length, 1);
    const login = await service.beginLogin("7");
    assert.ok(login);
    assert.equal(login!.allowCredentials?.[0]?.id, store.forActor("7")[0]!.id);
    const handle = store.userHandle("7");
    await assert.rejects(() => service.finishLogin("7", login!.challenge,
      device.assertion(login!.challenge, handle, 1, "https://evil.example"), 2000));
    for (const [label, response] of [
      ["RP ID hash", device.assertion(login!.challenge, handle, 1, origin, 0x05, "evil.example")],
      ["missing user verification", device.assertion(login!.challenge, handle, 1, origin, 0x01)],
      ["missing user presence", device.assertion(login!.challenge, handle, 1, origin, 0x04)],
      ["wrong ceremony type", device.assertion(login!.challenge, handle, 1, origin, 0x05, rpID, "webauthn.create")],
    ] as const) {
      await assert.rejects(() => service.finishLogin("7", login!.challenge, response, 2000), label);
      assert.equal(store.forActor("7")[0]!.counter, 0, label + " must not advance the counter");
    }
    assert.equal(await service.finishLogin("7", login!.challenge, device.assertion(login!.challenge, handle), 2000), true);
    await assert.rejects(() => service.finishLogin("7", login!.challenge, device.assertion(login!.challenge, handle), 2001));
    const otherHandle = store.userHandle("8");
    assert.equal(await service.finishLogin("7", login!.challenge,
      device.assertion(login!.challenge, otherHandle, 2), 2002), false,
      "a genuine signature with another account's user handle cannot cross the actor boundary");
    assert.equal(store.get("7", store.forActor("7")[0]!.id)?.counter, 1);
    assert.equal(await service.beginLogin("8"), null,
      "an account without enrolled credentials cannot start passkey-only login");
    assert.equal(await service.finishLogin("8", login!.challenge, device.assertion(login!.challenge, handle, 2), 2002), false);
    await assert.rejects(() => service.finishLogin("7", "wrong-challenge", device.assertion(login!.challenge, handle, 2), 2002));
    store.revoke("7", store.forActor("7")[0]!.id, 3000);
    assert.equal(await service.finishLogin("7", login!.challenge, device.assertion(login!.challenge, handle, 3), 3001), false);
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test("relying party identity is pinned to a verified HTTPS origin, never request headers", () => {
  assert.deepEqual(relyingParty(origin, rpID), { origin, rpID });
  for (const [candidateOrigin, candidateRP] of [
    ["http://localhost:8443", "localhost"],
    ["https://evil.example:8443", "localhost"],
    ["https://localhost:8443/path", "localhost"],
    ["https://localhost:8443?rp=evil", "localhost"],
    ["https://localhost:8443", "evil.example"],
  ]) assert.throws(() => relyingParty(candidateOrigin!, candidateRP!));
});
