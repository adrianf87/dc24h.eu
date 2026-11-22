import assert from "node:assert/strict";
import { test } from "node:test";
import type { IncomingMessage, ServerResponse } from "node:http";
import { remoteGateway } from "../server/remote-gateway.ts";

const origin = "https://localhost:8443";

// In-memory HTTP stubs exercise the handler without opening a network socket.
function harness(audit?: (event: string) => void) {
  const handle = remoteGateway(null, { origin, audit });
  return async (path: string, method: string, body = "", extra: Record<string, string> = {},
    chunks: Buffer[] = body ? [Buffer.from(body)] : [], interrupted = false) => {
    const headers: Record<string, string> = { host: "localhost", ...extra };
    const req = {
      url: path, method, headers, socket: { remoteAddress: "127.0.0.1" },
      async *[Symbol.asyncIterator]() {
        for (const chunk of chunks) yield chunk;
        if (interrupted) throw new Error("request_aborted");
      },
    } as unknown as IncomingMessage;
    const result: { status: number; body: string; cookies: string[] } = { status: 0, body: "", cookies: [] };
    const res = {
      writableEnded: false, destroyed: false,
      writeHead(status: number, responseHeaders: Record<string, unknown>) {
        result.status = status;
        const cookies = responseHeaders["Set-Cookie"];
        if (Array.isArray(cookies)) result.cookies = cookies as string[];
      },
      end(data: string) { result.body = data; },
    } as unknown as ServerResponse;
    await handle(req, res);
    return result;
  };
}

test("malformed login consumes its one-time challenge", async () => {
  const call = harness();
  const issued = await call("/remote/v1/challenge", "GET");
  assert.equal(issued.status, 200);
  const cookie = issued.cookies[0]?.split(";")[0];
  const challenge = JSON.parse(issued.body).csrf as string;
  assert.ok(cookie && challenge);
  const headers = {
    origin, cookie, "content-type": "application/json", "x-dc24h-remote-csrf": challenge,
  };
  const malformed = await call("/remote/v1/login", "POST", "{oops", {
    ...headers, "content-length": "5",
  });
  assert.equal(malformed.status, 400);
  assert.ok(malformed.cookies.some((item) => item.startsWith("dc24h_remote_init=;")));

  const valid = JSON.stringify({ actor_id: "7", password: "correct" });
  const replay = await call("/remote/v1/login", "POST", valid, {
    ...headers, "content-length": String(Buffer.byteLength(valid)),
  });
  assert.equal(replay.status, 403, "a used challenge must be rejected before contacting the API");
  assert.equal(JSON.parse(replay.body).error, "login_refused");
});

test("oversized streamed login scrubs buffered proof bytes on rejection", async () => {
  const call = harness();
  const chunks = [Buffer.alloc(1024, 0x61), Buffer.alloc(1025, 0x62)];
  const response = await call("/remote/v1/login", "POST", "", {
    origin, "content-type": "application/json", "content-length": "2048",
  }, chunks);
  assert.equal(response.status, 400);
  assert.equal(response.body, '{"error":"bad_request"}');
  for (const chunk of chunks) assert.ok(chunk.every((byte) => byte === 0), "rejected body remains in memory");
});

test("interrupted login scrubs partially received proof", async () => {
  const call = harness();
  const chunk = Buffer.alloc(120, 0x63);
  const response = await call("/remote/v1/login", "POST", "", {
    origin, "content-type": "application/json", "content-length": "150",
  }, [chunk], true);
  assert.equal(response.status, 400);
  assert.ok(chunk.every((byte) => byte === 0), "aborted request proof remains in memory");
});

test("login scrubs the assembled proof buffer after parsing", async () => {
  const call = harness();
  const originalConcat = Buffer.concat;
  const proofCopies: Buffer[] = [];
  try {
    Buffer.concat = ((parts: readonly Uint8Array[], length?: number) => {
      const assembled = originalConcat(parts, length);
      if (assembled.includes("correct")) proofCopies.push(assembled);
      return assembled;
    }) as typeof Buffer.concat;
    const body = JSON.stringify({ actor_id: "7", password: "correct" });
    const response = await call("/remote/v1/login", "POST", body, {
      origin, "content-type": "application/json", "content-length": String(Buffer.byteLength(body)),
    });
    assert.equal(response.status, 403, "a request without a challenge must be rejected after parsing");
    const malformed = "{correct";
    const rejected = await call("/remote/v1/login", "POST", malformed, {
      origin, "content-type": "application/json", "content-length": String(Buffer.byteLength(malformed)),
    });
    assert.equal(rejected.status, 400, "a malformed JSON proof must be rejected");
  } finally {
    Buffer.concat = originalConcat;
  }
  assert.equal(proofCopies.length, 2, "both valid and invalid JSON bodies must be assembled");
  for (const copy of proofCopies) {
    assert.ok(copy.every((byte) => byte === 0), "copied password bytes remain in memory");
  }
});

test("failed audit sink cannot interrupt a rejected gateway request", async () => {
  const call = harness(() => { throw new Error("audit_sink_unavailable"); });
  const refused = await call("/remote/v1/private", "GET");
  assert.equal(refused.status, 404);
  assert.equal(refused.body, '{"error":"not_found"}');
});

test("rejected asynchronous audit sink does not leak an unhandled rejection", async () => {
  const call = harness(async () => { throw new Error("audit_sink_rejected"); });
  const refused = await call("/remote/v1/other", "POST");
  assert.equal(refused.status, 404);
  await new Promise<void>((resolve) => setImmediate(resolve));
});
