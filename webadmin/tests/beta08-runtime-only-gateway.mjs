// Disposable hosted Ubuntu smoke: the compiled Node gateway, production npm
// dependencies only, private synthetic catalogs, and a local-only bind.
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtempSync, mkdirSync, chmodSync, existsSync, rmSync, writeFileSync } from "node:fs";
import { request } from "node:http";
import { createServer } from "node:net";
import { tmpdir } from "node:os";
import { dirname, isAbsolute, join } from "node:path";
import { once } from "node:events";

async function bounded(promise, milliseconds, message) {
  let timer;
  try {
    return await Promise.race([promise, new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error(message)), milliseconds);
    })]);
  } finally { clearTimeout(timer); }
}

async function smoke(bundle) {
  assert.ok(isAbsolute(bundle) && bundle.endsWith("/dist/remote-main.mjs"));
  const install = dirname(dirname(bundle));
  assert.ok(existsSync(bundle));
  assert.equal(existsSync(join(install, "node_modules", "tsx")), false);
  assert.equal(existsSync(join(install, "node_modules", "typescript")), false);
  assert.equal(existsSync(join(install, "node_modules", "esbuild")), false);
  const root = mkdtempSync(join(tmpdir(), "beta08-runtime-"));
  let upstream;
  try {
    const data = join(root, "state"); mkdirSync(data, { mode: 0o700 });
    const records = {
      "mfa.json": { schema: 1, accounts: { "7": "A".repeat(32) } },
      "passkeys.json": { schema: 1, rpID: "localhost", users: [], credentials: [] },
      "rate-limits.json": { schema: 1, buckets: [] },
    };
    for (const [name, value] of Object.entries(records))
      writeFileSync(join(data, name), JSON.stringify(value) + "\n", { mode: 0o600 });
    for (const name of ["audit.jsonl", "replay.jsonl"])
      writeFileSync(join(data, name), "", { mode: 0o600 });
    const socket = join(root, "api.sock");
    upstream = createServer(stream => stream.end());
    upstream.listen(socket);
    await once(upstream, "listening");
    chmodSync(socket, 0o600);
    const token = join(root, "token");
    writeFileSync(token, "a".repeat(64) + "\n", { mode: 0o600 });
    const reserved = createServer();
    reserved.listen(0, "127.0.0.1");
    await once(reserved, "listening");
    const address = reserved.address();
    assert.ok(address && typeof address !== "string");
    const port = address.port;
    await new Promise((resolve, reject) => reserved.close(error => error ? reject(error) : resolve()));
    const env = {
      ...process.env,
      DC24H_API_SOCKET: socket,
      DC24H_API_TOKEN_FILE: token,
      DC24H_REMOTE_ORIGIN: "https://localhost:8443",
      DC24H_REMOTE_PORT: String(port),
      DC24H_REMOTE_MFA_FILE: join(data, "mfa.json"),
      DC24H_REMOTE_MFA_REPLAY_FILE: join(data, "replay.jsonl"),
      DC24H_REMOTE_AUDIT_FILE: join(data, "audit.jsonl"),
      DC24H_REMOTE_RP_ID: "localhost",
      DC24H_REMOTE_PASSKEY_FILE: join(data, "passkeys.json"),
      DC24H_REMOTE_RATE_LIMIT_FILE: join(data, "rate-limits.json"),
    };
    delete env.DC24H_REMOTE_PUBLIC;
    delete env.DC24H_REMOTE_BIND;
    for (let iteration = 0; iteration < 2; iteration++) {
      const child = spawn(process.execPath, [bundle], { cwd: install, env, stdio: ["ignore", "pipe", "pipe"] });
      let stderr = "";
      child.stderr.on("data", chunk => { stderr = (stderr + chunk.toString()).slice(-2000); });
      // Register before readiness: a fast exit after SIGTERM must never be
      // missed by attaching an 'exit' listener after kill() returns.
      const stopped = new Promise(resolve => {
        child.once("exit", (code, signal) => resolve({ code, signal }));
        child.once("error", () => resolve({ code: -1, signal: null }));
      });
      try {
        await bounded(new Promise((resolve, reject) => {
          let output = "";
          child.stdout.on("data", chunk => {
            output = (output + chunk.toString()).slice(-256);
            if (output.includes("local-only listener ready")) resolve();
          });
          child.once("exit", () => { reject(new Error("Compiled gateway exited before readiness: " +
            (/ERR_MODULE_NOT_FOUND|Cannot find package/.test(stderr) ? "missing_runtime_dependency" :
              /Invalid local API adapter/.test(stderr) ? "invalid_synthetic_api_fixture" : "startup_refusal"))); });
          child.once("error", reject);
        }), 8000, "Compiled gateway startup timeout");
        const status = await new Promise((resolve, reject) => {
          const req = request({ hostname: "127.0.0.1", port, path: "/__synthetic_probe__", method: "GET" }, res => {
            res.resume(); res.on("end", () => resolve(res.statusCode));
          });
          req.on("error", reject); req.setTimeout(2000, () => req.destroy()); req.end();
        });
        assert.ok(Number.isInteger(status) && status >= 400 && status < 600);
      } finally {
        child.kill("SIGTERM");
        await bounded(stopped, 4000, "Gateway shutdown timeout");
      }
    }
  } finally {
    if (upstream) await new Promise(resolve => upstream.close(() => resolve()));
    rmSync(root, { recursive: true, force: true });
  }
}

smoke(process.argv[2] ?? "").then(() => {
  process.stdout.write("Compiled runtime-only gateway start/restart and loopback smoke passed\n");
}).catch(error => {
  const reason = error instanceof Error && error.message.includes("EPERM") ? "synthetic_socket_not_permitted" :
    error instanceof Error && /missing_runtime_dependency|invalid_synthetic_api_fixture|startup_refusal|timeout|shutdown|EADDRINUSE/.test(error.message)
      ? error.message.replace(/[^a-z_ ]/g, "").slice(0, 90) : "fixture_assertion_failed";
  process.stderr.write("Compiled runtime-only gateway smoke refused: " + reason + "\n");
  process.exitCode = 1;
});
