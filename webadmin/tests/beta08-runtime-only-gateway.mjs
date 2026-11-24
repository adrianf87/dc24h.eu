// Disposable hosted Ubuntu smoke: the compiled Node gateway, production npm
// dependencies only, private synthetic catalogs, and a local-only bind.
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtempSync, mkdirSync, chmodSync, existsSync, rmSync, writeFileSync, readFileSync } from "node:fs";
import { request } from "node:http";
import { createServer } from "node:net";
import { tmpdir } from "node:os";
import { dirname, isAbsolute, join } from "node:path";
import { once } from "node:events";
import { execFileSync } from "node:child_process";

async function bounded(promise, milliseconds, message) {
  let timer;
  try {
    return await Promise.race([promise, new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error(message)), milliseconds);
    })]);
  } finally { clearTimeout(timer); }
}

function systemdCommand(command, args, expectedSuccess = true) {
  try {
    return execFileSync(command, args, { encoding: "utf8", timeout: 25000, stdio: ["ignore", "pipe", "ignore"] }).trim();
  } catch {
    if (expectedSuccess) throw new Error("systemd_local_service_refused");
    return null;
  }
}

async function systemdGateway(bundle, install, root, port, env) {
  assert.equal(process.env.GITHUB_ACTIONS, "true");
  assert.equal(process.env.RUNNER_ENVIRONMENT, "github-hosted");
  assert.equal(process.env.RUNNER_OS, "Linux");
  const unit = `dc24h-beta09-node-${process.pid}.service`;
  const runner = systemdCommand("id", ["-un"]);
  const environmentFile = join(root, "gateway.env");
  const variables = Object.entries(env).filter(([name]) => name.startsWith("DC24H_"));
  for (const [name, value] of variables) {
    assert.match(name, /^DC24H_[A-Z_]+$/);
    assert.match(value, /^[\w./:-]+$/);
  }
  writeFileSync(environmentFile, variables.map(([key, value]) => `${key}=${value}\n`).join(""), { mode: 0o600, flag: "wx" });
  let started = false;
  try {
    systemdCommand("sudo", ["systemd-run", "--collect", `--unit=${unit}`,
      "--property=Type=exec", `--property=User=${runner}`, `--property=WorkingDirectory=${install}`,
      `--property=EnvironmentFile=${environmentFile}`, "--property=NoNewPrivileges=yes",
      "--property=ProtectSystem=strict", "--property=ProtectHome=read-only",
      "--property=PrivateDevices=yes", "--property=ProtectKernelModules=yes",
      "--property=ProtectKernelTunables=yes", "--property=ProtectControlGroups=yes",
      "--property=CapabilityBoundingSet=", "--property=RestrictAddressFamilies=AF_UNIX AF_INET",
      `--property=ReadWritePaths=${root}`, process.execPath, bundle]);
    started = true;
    let firstPid = null;
    for (let iteration = 0; iteration < 2; iteration++) {
      const until = Date.now() + 14000;
      let ready = false;
      while (Date.now() < until) {
        if (systemdCommand("systemctl", ["is-active", "--quiet", unit], false) === null) {
          throw new Error("systemd_local_service_exited");
        }
        try {
          const status = await bounded(new Promise((resolve, reject) => {
            const req = request({ hostname: "127.0.0.1", port, path: "/__synthetic_probe__" }, res => {
              res.resume(); res.on("end", () => resolve(res.statusCode));
            });
            req.on("error", reject); req.end();
          }), 800, "systemd_probe_timeout");
          if (status >= 400 && status < 600) { ready = true; break; }
        } catch { await new Promise(resolve => setTimeout(resolve, 100)); }
      }
      assert.ok(ready, "systemd_gateway_not_ready");
      const pid = Number(systemdCommand("systemctl", ["show", "--property=MainPID", "--value", unit]));
      assert.ok(Number.isInteger(pid) && pid > 1);
      if (firstPid !== null) assert.notEqual(pid, firstPid, "systemd_restart_did_not_replace_process");
      firstPid = pid;
      for (const [property, value] of [["User", runner], ["NoNewPrivileges", "yes"],
        ["ProtectSystem", "strict"], ["ProtectHome", "read-only"],
        ["PrivateDevices", "yes"], ["ProtectKernelModules", "yes"],
        ["ProtectKernelTunables", "yes"], ["ProtectControlGroups", "yes"]]) {
        assert.equal(systemdCommand("systemctl", ["show", `--property=${property}`, "--value", unit]), value);
      }
      assert.deepEqual(new Set(systemdCommand("systemctl", ["show", "--property=RestrictAddressFamilies",
        "--value", unit]).split(/\s+/)), new Set(["AF_UNIX", "AF_INET"]));
      // The gateway must have a *single loopback-only* TCP listener on the
      // selected port, never a wildcard or public-interface binding.
      const hexPort = port.toString(16).toUpperCase().padStart(4, "0");
      const tcp = readFileSync("/proc/net/tcp", "utf8").trim().split("\n").slice(1)
        .map(line => line.trim().split(/\s+/)).filter(fields => fields[1]?.endsWith(`:${hexPort}`) && fields[3] === "0A");
      assert.deepEqual(tcp.map(fields => fields[1]), [`0100007F:${hexPort}`]);
      const tcp6 = readFileSync("/proc/net/tcp6", "utf8").trim().split("\n").slice(1)
        .map(line => line.trim().split(/\s+/)).filter(fields => fields[1]?.endsWith(`:${hexPort}`) && fields[3] === "0A");
      assert.deepEqual(tcp6, [], "systemd gateway must not expose an IPv6 listener");
      if (iteration === 0) systemdCommand("sudo", ["systemctl", "restart", unit]);
    }
    systemdCommand("sudo", ["systemctl", "stop", unit]);
    started = false;
    assert.equal(systemdCommand("systemctl", ["is-active", "--quiet", unit], false), null);
  } finally {
    if (started) systemdCommand("sudo", ["systemctl", "stop", unit], false);
    systemdCommand("sudo", ["systemctl", "reset-failed", unit], false);
  }
}

async function smoke(bundle, viaSystemd = false) {
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
    if (viaSystemd) {
      await systemdGateway(bundle, install, root, port, env);
      return;
    }
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

const viaSystemd = process.argv[3] === "--systemd";
smoke(process.argv[2] ?? "", viaSystemd).then(() => {
  if (viaSystemd && process.env.DC24H_VM_REPORT_DIR) {
    assert.match(process.env.SOURCE_SHA ?? "", /^[0-9a-f]{40}$/);
    assert.match(process.env.DC24H_OS_IMAGE ?? "", /^ubuntu-(22\.04|24\.04|26\.04)$/);
    writeFileSync(join(process.env.DC24H_VM_REPORT_DIR, "beta09-systemd-node.json"),
      JSON.stringify({ schema: 1, source_sha: process.env.SOURCE_SHA, os_image: process.env.DC24H_OS_IMAGE,
        compiled_node_production_only_dependencies: true,
        actual_systemd_start_restart_stop: true, unprivileged_systemd_service: true,
        loopback_only_listener: true, private_owner_only_five_file_state: true,
        hardened_systemd_unit_properties: true, production_service_changed: false }) + "\n",
      { flag: "wx", mode: 0o600 });
  }
  process.stdout.write("Compiled runtime-only gateway start/restart and loopback smoke passed\n");
}).catch(error => {
  const reason = error instanceof Error && error.message.includes("EPERM") ? "synthetic_socket_not_permitted" :
    error instanceof Error && /missing_runtime_dependency|invalid_synthetic_api_fixture|startup_refusal|timeout|shutdown|EADDRINUSE/.test(error.message)
      ? error.message.replace(/[^a-z_ ]/g, "").slice(0, 90) : "fixture_assertion_failed";
  process.stderr.write("Compiled runtime-only gateway smoke refused: " + reason + "\n");
  process.exitCode = 1;
});
