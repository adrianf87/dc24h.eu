"""Real C++ -> Node -> Apache HTTPS authenticated API measurements, Ubuntu CI only.

All accounts, TOTP factors, SQL data, TLS certificates and gateway catalogs are
ephemeral. Results contain aggregate timings only; raw responses and cookies
are never written to artifacts. This is not a capacity or deployment test.
"""

import base64
from concurrent.futures import ThreadPoolExecutor
import grp
import hashlib
import hmac
import http.client
import json
import os
from pathlib import Path
import platform
import pwd
import re
import secrets
import shutil
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import time

import stage1_matched_recovery_tests as matched
from api_support import enable_api

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import stage4_endurance_gate as gates

REQUESTS = 24
CONCURRENCY = 2
MAX_P99_MS = 2000
SEED = "JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP"  # Synthetic fixture only.


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def wait_tcp(port, process, seconds=20):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        if process.poll() is not None:
            raise AssertionError("Disposable service stopped unexpectedly")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.15):
                return
        except OSError:
            time.sleep(0.05)
    raise AssertionError("Disposable service readiness timeout")


def terminate(process):
    if process is None:
        return
    if process.poll() is None:
        process.terminate()
    try:
        process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.communicate(timeout=5)


def code_now():
    key = base64.b32decode(SEED)
    step = int(time.time() // 30)
    digest = hmac.new(key, step.to_bytes(8, "big"), hashlib.sha1).digest()
    offset = digest[-1] & 15
    return f"{(int.from_bytes(digest[offset:offset + 4], 'big') & 0x7fffffff) % 1000000:06d}"


def percentile(samples, percentile_value):
    at = max(0, (len(samples) * percentile_value + 99) // 100 - 1)
    return round(sorted(samples)[at], 3)


def run(binary, seeder):
    if not (os.environ.get("GITHUB_ACTIONS") == "true" and
            os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted" and
            os.environ.get("RUNNER_OS") == "Linux"):
        raise AssertionError("Authenticated measurements require GitHub-hosted Ubuntu CI")
    version = platform.freedesktop_os_release().get("VERSION_ID")
    if version not in ("22.04", "24.04", "26.04"):
        raise AssertionError("Unsupported measurement OS")
    sha = gates.required_source_sha(os.environ.get("SOURCE_SHA"))
    if not Path(binary).is_file() or not Path(seeder).is_file():
        raise AssertionError("Built C++ executable and real database fixture required")
    if not Path("webadmin/node_modules/tsx").is_dir():
        raise AssertionError("Installed Node gateway dependencies required")

    with tempfile.TemporaryDirectory(prefix="dc24h-beta06-auth-") as temporary:
        base = Path(temporary)
        # Apache serves only a synthetic public directory; private files live
        # in owner-only subdirectories outside DocumentRoot.
        base.chmod(0o755)
        db = matched.IsolatedDatabase(base / "sql")
        gateway_dir = base / "private"
        gateway_dir.mkdir(mode=0o700)
        apache_dir = base / "apache"
        apache_dir.mkdir()
        public = apache_dir / "public"
        public.mkdir()
        (public / "remote.html").write_text("<title>Disposable remote fixture</title>")
        (apache_dir / "mime.types").write_text("text/html html\napplication/json json\n")
        cpp = node = apache = None
        try:
            db.initialize()
            db.start()
            config = db.setup_schema()
            matched.command([str(binary), "--config", str(config), "--migrate-database"])
            actor_seed = subprocess.run([str(seeder), str(db.socket), str(db.root / "password")],
                                        capture_output=True, text=True, timeout=60)
            if actor_seed.returncode != 0:
                raise AssertionError("Synthetic C++ account seed failed")
            actors = re.search(r"actor=(\d+) target=(\d+)", actor_seed.stdout)
            if not actors:
                raise AssertionError("Synthetic C++ account seed missing")
            actor = actors.group(1)
            api_socket, api_token = enable_api(config, db.root)
            cpp = subprocess.Popen([str(binary), "--config", str(config)],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   start_new_session=True)
            until = time.monotonic() + 15
            while not api_socket.exists():
                if cpp.poll() is not None or time.monotonic() > until:
                    raise AssertionError("Real C++ maintenance API did not start")
                time.sleep(0.05)

            enrollment = gateway_dir / "mfa.json"
            fd = os.open(enrollment, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            with os.fdopen(fd, "w", encoding="ascii") as output:
                json.dump({"schema": 1, "accounts": {actor: SEED}}, output)
                output.write("\n")
            initialized = subprocess.run([
                "node", "--import", "tsx", "server/remote-private-init.ts",
                "--directory", str(gateway_dir), "--rp-id", "localhost",
                "--offline-ack", "--first-install-ack",
            ], cwd=ROOT / "webadmin", capture_output=True, timeout=20)
            if initialized.returncode:
                raise AssertionError("Synthetic private-state initialization failed")

            apache_port, node_port = free_port(), free_port()
            if apache_port == node_port:
                node_port = free_port()
            origin = f"https://localhost:{apache_port}"
            env = {**os.environ, "DC24H_API_SOCKET": str(api_socket),
                   "DC24H_API_TOKEN_FILE": str(db.root / "admin.token"),
                   "DC24H_REMOTE_ORIGIN": origin, "DC24H_REMOTE_PORT": str(node_port),
                   "DC24H_REMOTE_MFA_FILE": str(enrollment),
                   "DC24H_REMOTE_MFA_REPLAY_FILE": str(gateway_dir / "replay.jsonl"),
                   "DC24H_REMOTE_AUDIT_FILE": str(gateway_dir / "audit.jsonl"),
                   "DC24H_REMOTE_PASSKEY_FILE": str(gateway_dir / "passkeys.json"),
                   "DC24H_REMOTE_RATE_LIMIT_FILE": str(gateway_dir / "rate-limits.json"),
                   "DC24H_REMOTE_RP_ID": "localhost"}
            node = subprocess.Popen(["node", "--import", "tsx", "server/remote-main.ts"],
                                    cwd=ROOT / "webadmin", env=env,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                    start_new_session=True)
            wait_tcp(node_port, node)

            cert, key = apache_dir / "cert.pem", apache_dir / "key.pem"
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                            "-days", "1", "-subj", "/CN=localhost",
                            "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1",
                            "-keyout", str(key), "-out", str(cert)],
                           capture_output=True, check=True, timeout=20)
            key.chmod(0o600)
            module_root = Path("/usr/lib/apache2/modules")
            modules = ("mpm_event", "authz_core", "authz_host", "headers", "rewrite",
                       "dir", "mime", "alias", "socache_shmcb", "ssl", "proxy", "proxy_http")
            apache_config = ('ServerRoot "' + str(apache_dir) + '"\nServerName localhost\n'
                             + "".join(f'LoadModule {module}_module "{module_root}/mod_{module}.so"\n'
                                       for module in modules))
            user = pwd.getpwuid(os.geteuid())
            apache_config += (f'PidFile "{apache_dir / "pid"}"\n'
                              f'ErrorLog "{apache_dir / "error.log"}"\n'
                              f'DefaultRuntimeDir "{apache_dir}"\n'
                              f'User {user.pw_name}\nGroup {grp.getgrgid(user.pw_gid).gr_name}\n'
                              'StartServers 1\nServerLimit 1\nThreadsPerChild 8\n'
                              'MaxRequestWorkers 8\nTimeout 5\n'
                              f'SSLSessionCache "shmcb:{apache_dir / "ssl-cache"}(512000)"\n'
                              f'TypesConfig "{apache_dir / "mime.types"}"\n')
            template = (ROOT / "config/apache2-webadmin-remote.conf.example").read_text()
            template = (template.replace("127.0.0.1:8443", f"127.0.0.1:{apache_port}")
                        .replace("localhost:8443", f"localhost:{apache_port}")
                        .replace("/usr/local/share/dc24h/webadmin", str(public))
                        .replace("/etc/dc24h/remote-tls/fullchain.pem", str(cert))
                        .replace("/etc/dc24h/remote-tls/privkey.pem", str(key))
                        .replace("http://127.0.0.1:4182", f"http://127.0.0.1:{node_port}"))
            conf = apache_dir / "httpd.conf"
            conf.write_text(apache_config + template)
            apache_bin = shutil.which("apache2") or "/usr/sbin/apache2"
            if subprocess.run([apache_bin, "-t", "-f", str(conf)], capture_output=True,
                              timeout=10).returncode:
                raise AssertionError("Isolated private Apache config rejected")
            apache = subprocess.Popen([apache_bin, "-f", str(conf), "-DFOREGROUND"],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                      start_new_session=True)
            wait_tcp(apache_port, apache)

            context = ssl.create_default_context(cafile=str(cert))

            def fetch(route, method="GET", data=None, headers=None):
                conn = http.client.HTTPSConnection("localhost", apache_port,
                                                   context=context, timeout=5)
                try:
                    start = time.monotonic_ns()
                    conn.request(method, route, body=data, headers=headers or {})
                    response = conn.getresponse()
                    encoded = response.read(65537)
                    elapsed = (time.monotonic_ns() - start) / 1_000_000
                    return response.status, encoded, response.getheaders(), elapsed
                finally:
                    conn.close()

            status, payload, headers, _ = fetch("/remote/v1/challenge")
            if status != 200:
                raise AssertionError("Actual Apache/Node challenge unavailable")
            cookie = next(value.split(";", 1)[0] for name, value in headers
                          if name.lower() == "set-cookie")
            challenge = json.loads(payload)["csrf"]
            body = json.dumps({"actor_id": actor, "password": "WebAdmin operator password!",
                               "otp": code_now()}).encode()
            status, payload, headers, _ = fetch("/remote/v1/login", "POST", body, {
                "Origin": origin, "Content-Type": "application/json", "Cookie": cookie,
                "X-DC24H-Remote-CSRF": challenge})
            if status != 200 or json.loads(payload).get("authenticated") is not True:
                raise AssertionError("Real C++ password + synthetic TOTP login failed")
            cookie = next(value.split(";", 1)[0] for name, value in headers
                          if name.lower() == "set-cookie")

            def measure(_):
                status, payload, _headers, elapsed = fetch("/remote/v1/summary", headers={"Cookie": cookie})
                try:
                    value = json.loads(payload)
                    valid = (status == 200 and value.get("schema") == 1 and
                             value.get("mode") == "read-only" and value.get("server", {}).get("running") is True)
                except (ValueError, AttributeError):
                    valid = False
                return valid, elapsed

            started = time.monotonic()
            with ThreadPoolExecutor(max_workers=CONCURRENCY) as pool:
                readings = list(pool.map(measure, range(REQUESTS)))
            elapsed = time.monotonic() - started
            success = [duration for valid, duration in readings if valid]
            if len(success) != REQUESTS or not elapsed > 0:
                raise AssertionError("Real authenticated API workload had failed requests")
            p99 = percentile(success, 99)
            if p99 > MAX_P99_MS:
                raise AssertionError("Real authenticated API p99 exceeded CI regression ceiling")
            report = {
                "schema": 1, "source_sha": sha, "os_image": f"ubuntu-{version}",
                "evidence": "real-cpp-node-apache-authenticated-totp",
                "requests": REQUESTS, "success": len(success), "failure": 0,
                "concurrency": CONCURRENCY, "elapsed_seconds": round(elapsed, 3),
                "successful_requests_per_second": round(len(success) / elapsed, 3),
                "p50_ms": percentile(success, 50), "p95_ms": percentile(success, 95),
                "p99_ms": p99, "max_ci_p99_ms": MAX_P99_MS,
                "synthetic_credentials_only": True,
                "passkey_browser_flow_exercised": False,
                "capacity_claim": False, "production_sla_claim": False,
            }
            if os.environ.get("DC24H_VM_REPORT_DIR"):
                directory = Path(os.environ["DC24H_VM_REPORT_DIR"])
                if not directory.is_dir():
                    raise AssertionError("Expected isolated evidence directory is missing")
                with (directory / "beta06-authenticated-https.json").open("x", encoding="utf-8") as file:
                    json.dump(report, file, sort_keys=True, indent=2)
                    file.write("\n")
            print(f"Authenticated HTTPS: {len(success)}/{REQUESTS} success, p99={p99} ms (CI fixture)")
        finally:
            terminate(apache)
            terminate(node)
            terminate(cpp)
            db.stop()


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: beta06_authenticated_api_process_tests.py HUB ACCOUNT_WEBADMIN_SEED")
    run(str(Path(sys.argv[1]).resolve()), str(Path(sys.argv[2]).resolve()))
