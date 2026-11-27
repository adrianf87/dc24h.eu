"""Real maintenance C++ -> Node -> Apache HTTPS authenticated measurements, Ubuntu CI only.

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
import select
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

import stage1_matched_recovery_tests as matched
import beta06_previous_version_rollback as previous
from api_support import enable_api, fetch_status
from tls_support import secure_client_context

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import stage4_endurance_gate as gates
from stage4_perf_probe import process_cpu_seconds, rss_kib

REQUESTS = 24
CONCURRENCY = 2
MAX_P99_MS = 2000
BATCH_SPACING_SECONDS = 2.2
SEED = "JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP"  # Synthetic fixture only.
# Immutable historical status parser from the pinned beta-0.5.16 checkout.
# This is used exclusively in a disposable rollback test copy of WebAdmin.
HISTORICAL_STATUS_BLOB = "7bd1abc9c38d8f9f62f990fa0aec12c6f992bfab"


def rollback_gateway_fixture(historical_checkout, directory):
    """Pair the installed old C++ maintenance binary with its *old* status contract.

    The production WebAdmin remains strict about dc24h.eu-twilight. Replacing
    only the status parser in a disposable copy lets the historical C++ binary
    exercise real authentication, browser passkeys and matched recovery without
    claiming mixed-version deployment compatibility.
    """
    historical_checkout = Path(historical_checkout).resolve(strict=True)
    head = subprocess.run(["git", "-C", str(historical_checkout), "rev-parse", "HEAD"],
                          check=True, capture_output=True, text=True, timeout=10).stdout.strip()
    if head != previous.PREVIOUS_SHA:
        raise AssertionError("Unpinned historical gateway source refused")
    old_status = historical_checkout / "webadmin/src/lib/status.ts"
    if not old_status.is_file() or old_status.is_symlink():
        raise AssertionError("Historical status parser source unavailable")
    blob = subprocess.run(["git", "-C", str(historical_checkout), "hash-object", str(old_status)],
                          check=True, capture_output=True, text=True, timeout=10).stdout.strip()
    if blob != HISTORICAL_STATUS_BLOB:
        raise AssertionError("Historical status parser differs from immutable source")
    webadmin = ROOT / "webadmin"
    node_modules = webadmin / "node_modules"
    if not (node_modules / "tsx").is_dir():
        raise AssertionError("Installed Node gateway dependencies required")
    fixture = directory / "historical-identity-gateway"
    shutil.copytree(webadmin, fixture,
                    ignore=shutil.ignore_patterns("node_modules", "dist", "coverage", "test-results",
                                                   "playwright-report"))
    shutil.copyfile(old_status, fixture / "src/lib/status.ts")
    (fixture / "node_modules").symlink_to(node_modules.resolve(strict=True), target_is_directory=True)
    return fixture


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


def browser_phase(process, expected, timeout=60):
    """Wait for a fixed success word, never serialize browser credentials."""
    if process.stdout is None:
        raise AssertionError("Browser client protocol unavailable")
    ready, _, _ = select.select([process.stdout], [], [], timeout)
    result = process.stdout.readline().strip() if ready else ""
    if result != expected:
        diagnostic = result if re.fullmatch(r"client_error:[a-z_0-9]{1,48}", result) else "no_verified_result"
        raise AssertionError("Real Chromium passkey " + expected + " phase failed: " + diagnostic)


def browser_command(process, instruction):
    if process.poll() is not None or process.stdin is None:
        raise AssertionError("Real Chromium helper stopped")
    process.stdin.write(json.dumps(instruction, separators=(",", ":")) + "\n")
    process.stdin.flush()


def code_now():
    key = base64.b32decode(SEED)
    step = int(time.time() // 30)
    digest = hmac.new(key, step.to_bytes(8, "big"), hashlib.sha1).digest()
    offset = digest[-1] & 15
    return f"{(int.from_bytes(digest[offset:offset + 4], 'big') & 0x7fffffff) % 1000000:06d}"


def synthetic_password_proof_status(endpoint, token, actor):
    """Return only the status of the disposable C++ password proof, never its grant."""
    body = ("actor_id=" + actor + "&proof=WebAdmin+operator+password!").encode("ascii")
    headers = ("POST /api/v1/admin/access HTTP/1.1\r\nHost: localhost\r\n"
               "Authorization: Bearer " + token + "\r\n"
               "Content-Type: application/x-www-form-urlencoded\r\n"
               "Content-Length: " + str(len(body)) + "\r\nConnection: close\r\n\r\n").encode("ascii")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
        peer.settimeout(4)
        peer.connect(str(endpoint))
        peer.sendall(headers + body)
        return int(peer.recv(64).split(b" ", 2)[1])


def restored_login_budget_delay(catalog, service_token, actor, now_ms):
    """Wait for the *preserved* private limiter, without deleting a bucket.

    The recovery test can perform an additional deliberately refused TOTP
    replay before its fresh TOTP and Chromium passkey proofs. Those independent
    password attempts share the gateway's four-per-minute actor bucket. A
    fast hosted runner can reach the valid passkey proof while that bucket is
    full, producing HTTP 429 rather than testing the restored credential.
    Read only the synthetic recovered catalog; never reset its state or raise
    the production rate limits to make the test pass.
    """
    if (not isinstance(catalog, dict) or catalog.get("schema") != 1 or
            not isinstance(catalog.get("buckets"), list) or
            not re.fullmatch(r"[0-9a-f]{64}", service_token) or
            not re.fullmatch(r"[1-9][0-9]{0,18}", actor) or
            type(now_ms) is not int or now_ms < 0):
        raise AssertionError("Invalid disposable restored limiter state")
    secret = bytes.fromhex(service_token)
    deadlines = []
    for name, limit in (("login:global", 12), ("login:" + actor, 4)):
        key = hmac.new(secret, name.encode("ascii"), hashlib.sha256).hexdigest()
        matching = [entry for entry in catalog["buckets"]
                    if isinstance(entry, dict) and entry.get("key") == key]
        if len(matching) != 1:
            raise AssertionError("Restored private login-budget continuity missing")
        entry = matching[0]
        count, started = entry.get("count"), entry.get("starts")
        if (type(count) is not int or count < 1 or type(started) is not int or
                started < 0 or started > now_ms):
            raise AssertionError("Invalid restored login-budget window")
        if count >= limit and started + 60_000 > now_ms:
            deadlines.append(started + 60_000)
    # A 1.5-second safety margin allows different event-loop scheduling on
    # Ubuntu 22.04/24.04/26.04; never bypasses a 60-second bucket window.
    return max(0.0, (max(deadlines, default=now_ms) - now_ms + 1500) / 1000) if deadlines else 0.0


def percentile(samples, percentile_value):
    at = max(0, (len(samples) * percentile_value + 99) // 100 - 1)
    return round(sorted(samples)[at], 3)


def run(binary, seeder, account_admin, historical_checkout=None):
    if not (os.environ.get("GITHUB_ACTIONS") == "true" and
            os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted" and
            os.environ.get("RUNNER_OS") == "Linux"):
        raise AssertionError("Authenticated measurements require GitHub-hosted Ubuntu CI")
    version = platform.freedesktop_os_release().get("VERSION_ID")
    if version not in ("22.04", "24.04", "26.04"):
        raise AssertionError("Unsupported measurement OS")
    sha = gates.required_source_sha(os.environ.get("SOURCE_SHA"))
    if any(not Path(path).is_file() for path in (binary, seeder, account_admin)):
        raise AssertionError("Built C++ hub, maintenance authority and real database fixture required")
    if not Path("webadmin/node_modules/tsx").is_dir():
        raise AssertionError("Installed Node gateway dependencies required")

    with tempfile.TemporaryDirectory(prefix="dc24h-beta06-auth-") as temporary:
        base = Path(temporary)
        gateway_workdir = (rollback_gateway_fixture(historical_checkout, base)
                           if historical_checkout is not None else ROOT / "webadmin")
        # Apache serves only a synthetic public directory; private files live
        # in owner-only subdirectories outside DocumentRoot.
        base.chmod(0o755)
        recovery_root = base / "recovery-private"
        recovery_root.mkdir(mode=0o700)
        db = matched.IsolatedDatabase(base / "sql")
        gateway_dir = base / "private"
        gateway_dir.mkdir(mode=0o700)
        apache_dir = base / "apache"
        apache_dir.mkdir()
        public = apache_dir / "public"
        public.mkdir()
        (public / "remote.html").write_text("<title>Disposable remote fixture</title>")
        (apache_dir / "mime.types").write_text("text/html html\napplication/json json\n")
        cpp = node = apache = browser = None
        recovered_db = None
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
            # The serving hub stays historically read-only WITHOUT the
            # explicit OWNER opt-in. The new password grant cannot silently
            # become live just because the binary was upgraded.
            cpp = subprocess.Popen([str(binary), "--config", str(config)],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   start_new_session=True)
            until = time.monotonic() + 15
            while not api_socket.exists():
                if cpp.poll() is not None or time.monotonic() > until:
                    raise AssertionError("Serving hub read-only API did not start")
                time.sleep(0.05)
            until = time.monotonic() + 15
            while fetch_status(api_socket, api_token)["server"]["core_running"] is not True:
                if cpp.poll() is not None or time.monotonic() > until:
                    raise AssertionError("Serving hub read-only API preflight failed")
                time.sleep(0.05)
            if synthetic_password_proof_status(api_socket, api_token, actor) != 400:
                raise AssertionError("Serving hub unexpectedly enabled Stage IV password proof")
            terminate(cpp); cpp = None
            if api_socket.exists():
                raise AssertionError("Serving hub did not release private API socket")
            # Exact OWNER opt-in permits ONLY the private read-only Stage IV
            # password and role-discovery paths, not maintenance writes.
            cpp = subprocess.Popen([str(binary), "--config", str(config)],
                                   env={**os.environ, "DC24H_STAGE4_PRIVATE_AUTH_ACK":
                                        "OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO"},
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   start_new_session=True)
            until = time.monotonic() + 15
            while not api_socket.exists():
                if cpp.poll() is not None or time.monotonic() > until:
                    raise AssertionError("Stage IV private-auth serving hub did not start")
                time.sleep(0.05)
            if synthetic_password_proof_status(api_socket, api_token, actor) != 200:
                raise AssertionError("Stage IV private C++ account password proof unavailable")
            terminate(cpp); cpp = None
            if api_socket.exists():
                raise AssertionError("Stage IV private-auth serving hub did not release socket")
            cpp = subprocess.Popen([str(account_admin), "--config", str(config)],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   start_new_session=True)
            until = time.monotonic() + 15
            while not api_socket.exists():
                if cpp.poll() is not None or time.monotonic() > until:
                    raise AssertionError("Real C++ account maintenance API did not start")
                time.sleep(0.05)
            if fetch_status(api_socket, api_token)["server"]["core_running"] is not False:
                raise AssertionError("Maintenance fixture unexpectedly has serving hub")
            if synthetic_password_proof_status(api_socket, api_token, actor) != 200:
                raise AssertionError("Real C++ account-maintenance password proof unavailable")
            # Account authority intentionally applies a 1-second global POST
            # budget. A diagnostic proof must not throttle the actual TOTP
            # login whose latency and success are being measured.
            time.sleep(1.1)

            enrollment = gateway_dir / "mfa.json"
            fd = os.open(enrollment, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            with os.fdopen(fd, "w", encoding="ascii") as output:
                json.dump({"schema": 1, "accounts": {actor: SEED}}, output)
                output.write("\n")
            initialized = subprocess.run([
                "node", "--import", "tsx", "server/remote-private-init.ts",
                "--directory", str(gateway_dir), "--rp-id", "localhost",
                "--offline-ack", "--first-install-ack",
            ], cwd=gateway_workdir, capture_output=True, timeout=20)
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
                                    cwd=gateway_workdir, env=env,
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

            context = secure_client_context(cert)

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
                # CI may report only fixed, non-personal categories. The audit
                # format is allowlisted and contains no actors, token, cookie,
                # grant, password or TOTP. Never serialize raw response bytes.
                allowed = {"login_refused", "mfa_refused", "api_unavailable",
                           "rate_limiter_unavailable", "session_revoked",
                           "login_ok", "mfa_ok"}
                events = []
                try:
                    for line in (gateway_dir / "audit.jsonl").read_text(encoding="utf-8").splitlines():
                        event = json.loads(line).get("event")
                        if event in allowed:
                            events.append(event)
                except (OSError, ValueError, TypeError):
                    events = ["audit_unavailable"]
                try:
                    category = json.loads(payload).get("error")
                except (ValueError, TypeError, AttributeError):
                    category = None
                if category not in {"login_refused", "unavailable", "rate_limited", "forbidden", "bad_request"}:
                    category = "unrecognized_denial"
                # Probe the actual C++ password authority *after* the failed
                # browser login, separated from the 1-second POST budget. Only
                # retain the HTTP status; the grant body is never logged.
                upstream_status = "unavailable"
                try:
                    time.sleep(1.1)
                    upstream_status = str(synthetic_password_proof_status(api_socket, api_token, actor))
                except (OSError, ValueError, IndexError):
                    upstream_status = "unavailable"
                raise AssertionError(f"Real synthetic TOTP login refused: HTTP={status} "
                                     f"category={category} audit_categories={events[-5:]} "
                                     f"cpp_password_http={upstream_status}")
            cookie = next(value.split(";", 1)[0] for name, value in headers
                          if name.lower() == "set-cookie")

            def measure(index):
                # Each remote summary performs TWO C++ grant rechecks, plus
                # the login's initial recheck. The authority permits only 24
                # checks per 10 seconds; a burst of 24 summaries would be a
                # rate-limit test, not an authenticated success workload.
                # Start exactly two workers together every 2.2 seconds: the
                # fixture retains real concurrency=2 without grant flooding.
                due = started + (index // CONCURRENCY) * BATCH_SPACING_SECONDS
                remaining = due - time.monotonic()
                if remaining > 0:
                    time.sleep(remaining)
                status, payload, _headers, elapsed = fetch("/remote/v1/summary", headers={"Cookie": cookie})
                try:
                    value = json.loads(payload)
                    valid = (status == 200 and value.get("schema") == 1 and
                             value.get("mode") == "read-only" and value.get("server", {}).get("running") is False)
                except (ValueError, AttributeError):
                    valid = False
                return valid, elapsed

            processes = {"account_admin": cpp.pid, "gateway": node.pid, "apache": apache.pid}
            windows = []
            readings = []
            all_started = time.monotonic()
            for window in range(3):
                if window:
                    # Preserve the C++ grant recheck budget between windows.
                    time.sleep(BATCH_SPACING_SECONDS)
                window_cpu_before = {name: process_cpu_seconds(pid) for name, pid in processes.items()}
                window_rss_before = {name: rss_kib(pid) for name, pid in processes.items()}
                if any(value is None or value <= 0 for value in window_rss_before.values()) or (
                        any(value is None for value in window_cpu_before.values())):
                    raise AssertionError("Missing process CPU/RSS baseline")
                started = time.monotonic()
                with ThreadPoolExecutor(max_workers=CONCURRENCY) as pool:
                    observed = list(pool.map(measure, range(REQUESTS // 3)))
                window_elapsed = time.monotonic() - started
                window_cpu_after = {name: process_cpu_seconds(pid) for name, pid in processes.items()}
                window_rss_after = {name: rss_kib(pid) for name, pid in processes.items()}
                if any(value is None or value <= 0 for value in window_rss_after.values()) or (
                        any(value is None or value < window_cpu_before[name] for name, value in window_cpu_after.items())):
                    raise AssertionError("Missing or regressed process CPU/RSS observation")
                successes = [duration for valid, duration in observed if valid]
                windows.append({"schema": 1, "window": window + 1,
                                "requests": len(observed), "success": len(successes),
                                "failure": len(observed) - len(successes),
                                "rate_limit_safe_batch_spacing_ms": int(BATCH_SPACING_SECONDS * 1000),
                                "concurrency": CONCURRENCY, "elapsed_seconds": round(window_elapsed, 3),
                                "successful_requests_per_second": round(len(successes) / window_elapsed, 3),
                                "p50_ms": percentile(successes, 50) if successes else None,
                                "p95_ms": percentile(successes, 95) if successes else None,
                                "p99_ms": percentile(successes, 99) if successes else None,
                                "latency_samples_ms": sorted(round(value, 3) for value in successes),
                                "rss_before_kib": window_rss_before,
                                "rss_after_kib": window_rss_after,
                                "cpu_mean_percent_one_core_by_process": {
                                    name: round(100 * (window_cpu_after[name] - window_cpu_before[name]) /
                                                window_elapsed, 3) for name in processes
                                },
                                "cpu_mean_percent_one_core": round(100 * sum(
                                    window_cpu_after[name] - window_cpu_before[name] for name in processes
                                ) / window_elapsed, 3), "production_sla_claim": False})
                readings.extend(observed)
                if window == 0:
                    cpu_before, rss_before = window_cpu_before, window_rss_before
                cpu_after, rss_after = window_cpu_after, window_rss_after
            elapsed = time.monotonic() - all_started
            success = [duration for valid, duration in readings if valid]
            if len(success) != REQUESTS or not elapsed > 0:
                raise AssertionError("Real authenticated API workload had failed requests")
            p99 = percentile(success, 99)
            if p99 > MAX_P99_MS:
                raise AssertionError("Real authenticated API p99 exceeded CI regression ceiling")
            report = {
                "schema": 1, "source_sha": sha, "os_image": f"ubuntu-{version}",
                "evidence": "real-account-admin-node-apache-authenticated-totp",
                "maintenance_mode": True, "hub_readonly_boundary_verified": True,
                "requests": REQUESTS, "success": len(success), "failure": 0,
                "concurrency": CONCURRENCY, "elapsed_seconds": round(elapsed, 3),
                "request_spacing_ms": int(BATCH_SPACING_SECONDS * 1000 / CONCURRENCY),
                "batch_spacing_ms": int(BATCH_SPACING_SECONDS * 1000),
                "successful_requests_per_second": round(len(success) / elapsed, 3),
                "p50_ms": percentile(success, 50), "p95_ms": percentile(success, 95),
                "p99_ms": p99, "max_ci_p99_ms": MAX_P99_MS,
                "max_ms": round(max(success), 3),
                "rss_before_kib": rss_before, "rss_after_kib": rss_after,
                "beta07_three_windows": windows,
                "cpu_mean_percent_one_core": round(
                    100 * sum(cpu_after[name] - cpu_before[name] for name in processes) / elapsed, 3),
                "synthetic_credentials_only": True,
                # This report measures password+TOTP only. The real browser
                # passkey recovery appears in the separate restore report.
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

            # A passkey enrollment is allowed only during the first 60 seconds
            # of a freshly authenticated gateway session. The three timed
            # windows above deliberately exercise a long-lived session, which
            # must never be reused for a credential mutation. Get an entirely
            # new C++ password+TOTP grant and CSRF challenge before the real
            # browser enrollment; preserve the consumed OTP replay boundary.
            original_otp_step = int(time.time() // 30)
            until = time.monotonic() + 35
            while int(time.time() // 30) <= original_otp_step:
                if time.monotonic() > until:
                    raise AssertionError("Fresh enrollment TOTP window did not arrive")
                time.sleep(0.25)
            time.sleep(1.1)  # C++ maintenance authority's global POST budget
            status, payload, headers, _ = fetch("/remote/v1/challenge")
            if status != 200:
                raise AssertionError("Fresh passkey enrollment challenge unavailable")
            fresh_cookie = next(value.split(";", 1)[0] for name, value in headers
                                if name.lower() == "set-cookie")
            fresh_login = json.dumps({"actor_id": actor, "password": "WebAdmin operator password!",
                                      "otp": code_now()}).encode()
            status, payload, headers, _ = fetch("/remote/v1/login", "POST", fresh_login, {
                "Origin": origin, "Content-Type": "application/json", "Cookie": fresh_cookie,
                "X-DC24H-Remote-CSRF": json.loads(payload)["csrf"]})
            if status != 200 or json.loads(payload).get("authenticated") is not True:
                raise AssertionError("Fresh C++ password+TOTP grant for browser enrollment refused")
            cookie = next(value.split(";", 1)[0] for name, value in headers
                          if name.lower() == "set-cookie")

            # Enroll a genuine Chromium virtual authenticator through the
            # live private HTTPS gateway with an actual C++ password+TOTP
            # grant. Keep Chromium and its credential across the MariaDB and
            # five-store offline checkpoint; do not export the private key.
            browser = subprocess.Popen([
                "node", "tests/beta07-live-passkey-client.mjs",
            ], cwd=gateway_workdir, stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
                bufsize=1, start_new_session=True)
            browser_command(browser, {"origin": origin, "actor": actor,
                                      "sessionCookie": cookie, "csrf": json.loads(payload)["csrf"]})
            browser_phase(browser, "enrolled")

            # The prior beta-0.5 recovery test checked SQL and file bytes but
            # did not reauthenticate through a *restored* C++/Node instance.
            # Stop all application writers and checkpoint the actual synthetic
            # gateway session, MFA replay, audit, limiter and passkey files.
            previous_node, previous_cpp = node, cpp
            terminate(previous_node); node = None
            terminate(previous_cpp); cpp = None
            if previous_node.poll() is None or previous_cpp.poll() is None:
                raise AssertionError("Application writers were not stopped before capture")
            # The recovery library refuses even synthetic backups under a
            # traversable parent. Apache needs the outer fixture root at 0755,
            # so keep SQL, manifest and restore staging under this separate
            # owner-only 0700 directory instead of weakening its checks.
            snapshot = recovery_root / "snapshot"
            dump = recovery_root / "checkpoint.sql"
            pair = recovery_root / "pair"
            pair.mkdir(mode=0o700)
            with db.locked_offline_capture():
                before = {name: matched.bundle.private_hash(gateway_dir / name, maximum)
                          for name, maximum in matched.private.FILES.items()}
                db.dump(dump)
                if matched.offline_preflight.sql_migration_row_count(dump) != 11:
                    raise AssertionError("Stopped-writer SQL export lacks migration v11 proof")
                matched.private.snapshot(gateway_dir, snapshot)
                matched.bundle.create_pair(dump, snapshot, pair, sha)
                matched.bundle.verified_pair(dump, snapshot, pair / matched.bundle.NAME, sha)
                after = {name: matched.bundle.private_hash(gateway_dir / name, maximum)
                         for name, maximum in matched.private.FILES.items()}
                if before != after:
                    raise AssertionError("Private gateway changed while application writers were stopped")
            db.stop()

            recovered_db = matched.IsolatedDatabase(base / "recovered-sql")
            recovered_db.initialize()
            recovered_db.start()
            recovered_config = recovered_db.setup_schema()
            recovered_db.restore_sql(dump)
            if recovered_db.sql("SELECT MAX(version) FROM dc24h.dc24h_schema_migrations;") != "11":
                raise AssertionError("Recovered MariaDB is not schema v11")
            if recovered_db.sql("SELECT role FROM dc24h.dc24h_accounts WHERE id=" + actor + ";") != "administrator":
                raise AssertionError("Recovered C++ actor authority changed")
            restored_private = recovery_root / "restored-private"
            matched.private.restore_to_fresh_directory(snapshot, restored_private)
            restored_api, fresh_token = enable_api(recovered_config, recovered_db.root)
            # Preserve the HMAC limiter key across a matched state recovery;
            # minting a new gateway key would silently reset old rate budgets.
            if len(fresh_token) != 64:
                raise AssertionError("Invalid disposable API token")
            (recovered_db.root / "admin.token").write_text(api_token + "\n", encoding="ascii")
            (recovered_db.root / "admin.token").chmod(0o600)
            cpp = subprocess.Popen([str(account_admin), "--config", str(recovered_config)],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   start_new_session=True)
            until = time.monotonic() + 15
            while not restored_api.exists():
                if cpp.poll() is not None or time.monotonic() > until:
                    raise AssertionError("Recovered C++ API did not start")
                time.sleep(0.05)
            restored_env = {**env, "DC24H_API_SOCKET": str(restored_api),
                            "DC24H_API_TOKEN_FILE": str(recovered_db.root / "admin.token"),
                            "DC24H_REMOTE_MFA_FILE": str(restored_private / "mfa.json"),
                            "DC24H_REMOTE_MFA_REPLAY_FILE": str(restored_private / "replay.jsonl"),
                            "DC24H_REMOTE_AUDIT_FILE": str(restored_private / "audit.jsonl"),
                            "DC24H_REMOTE_PASSKEY_FILE": str(restored_private / "passkeys.json"),
                            "DC24H_REMOTE_RATE_LIMIT_FILE": str(restored_private / "rate-limits.json")}
            node = subprocess.Popen(["node", "--import", "tsx", "server/remote-main.ts"],
                                    cwd=gateway_workdir, env=restored_env,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                    start_new_session=True)
            wait_tcp(node_port, node)
            if fetch("/remote/v1/session", headers={"Cookie": cookie})[0] != 401:
                raise AssertionError("Pre-restore browser cookie must not survive process restart")

            # A consumed OTP step is still in the restored replay ledger.
            original_code = json.loads((restored_private / "replay.jsonl").read_text().splitlines()[-1])["step"]
            if type(original_code) is not int:
                raise AssertionError("Invalid restored MFA replay step")
            status, payload, headers, _ = fetch("/remote/v1/challenge")
            if status != 200:
                raise AssertionError("Recovered browser challenge unavailable")
            init_cookie = next(v.split(";", 1)[0] for name, v in headers if name.lower() == "set-cookie")
            csrf = json.loads(payload)["csrf"]
            replay_code = f"{int(code_now()):06d}"
            # If the restored step is already older, this check cannot prove
            # replay of the original OTP; explicitly require its time window.
            if int(time.time() // 30) == original_code:
                denied = json.dumps({"actor_id": actor, "password": "WebAdmin operator password!",
                                     "otp": replay_code}).encode()
                replay_status, _, _, _ = fetch("/remote/v1/login", "POST", denied, {
                    "Origin": origin, "Content-Type": "application/json", "Cookie": init_cookie,
                    "X-DC24H-Remote-CSRF": csrf})
                if replay_status != 403:
                    raise AssertionError("Restored MFA replay ledger failed to reject consumed OTP")

            # Wait for a fresh TOTP step so restoring the replay ledger cannot
            # be mistaken for a password/second-factor lockout.
            until = time.monotonic() + 35
            while int(time.time() // 30) <= original_code:
                if time.monotonic() > until:
                    raise AssertionError("Fresh restored TOTP window did not arrive")
                time.sleep(0.25)
            status, payload, headers, _ = fetch("/remote/v1/challenge")
            if status != 200:
                raise AssertionError("Restored fresh challenge unavailable")
            init_cookie = next(v.split(";", 1)[0] for name, v in headers if name.lower() == "set-cookie")
            fresh_csrf = json.loads(payload)["csrf"]
            login_body = json.dumps({"actor_id": actor, "password": "WebAdmin operator password!",
                                     "otp": code_now()}).encode()
            login_status, response, headers, _ = fetch("/remote/v1/login", "POST", login_body, {
                "Origin": origin, "Content-Type": "application/json", "Cookie": init_cookie,
                "X-DC24H-Remote-CSRF": fresh_csrf})
            if login_status != 200 or json.loads(response).get("authenticated") is not True:
                raise AssertionError("Fresh restored C++/Node login failed")
            restored_cookie = next(v.split(";", 1)[0] for name, v in headers
                                   if name.lower() == "set-cookie")
            if fetch("/remote/v1/summary", headers={"Cookie": restored_cookie})[0] != 200:
                raise AssertionError("Fresh restored read-only grant not usable")

            # A restored WebAuthn signature must now pass the actual password
            # maintenance authority, checked grant and read-only gateway.
            # The same device subsequently revokes itself via the authorized
            # CSRF-protected route, invalidating its live session.
            # The restored limiter deliberately retains previous TOTP logins
            # and the optional denied replay. A faster image (Ubuntu 26.04)
            # can reach the next legitimate passkey password proof after four
            # actor hits in the same 60-second window, yielding HTTP 429.
            # Let that actual persisted budget expire before the browser's
            # independent proof; never clear the bucket or relax the gateway.
            budget = json.loads((restored_private / "rate-limits.json").read_text(encoding="utf-8"))
            cooldown = restored_login_budget_delay(budget, api_token, actor, int(time.time() * 1000))
            if cooldown > 61.5:
                raise AssertionError("Unexpected recovered login-budget duration")
            # The C++ maintenance authority additionally has a 1-second
            # global POST budget, independent of Node's persisted limiter.
            time.sleep(max(2.2, cooldown))
            browser_command(browser, {"action": "restore"})
            browser_phase(browser, "authenticated_and_revoked")
            if browser.wait(timeout=10) != 0:
                raise AssertionError("Chromium restored credential helper failed")
            browser = None

            # Restart Node to verify the *real* revoked key persists across a
            # second cold opening of the restored private catalog.
            terminate(node); node = None
            node = subprocess.Popen(["node", "--import", "tsx", "server/remote-main.ts"],
                                    cwd=gateway_workdir, env=restored_env,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                    start_new_session=True)
            wait_tcp(node_port, node)
            # A valid independent password proof is required for this refusal;
            # a C++ maintenance POST rate limit is not a passkey negative test.
            time.sleep(1.1)
            # A revoked device in the restored catalog must never yield a
            # passkey challenge, even with the correct C++ password.
            status, payload, headers, _ = fetch("/remote/v1/challenge")
            if status != 200:
                raise AssertionError("Passkey negative challenge unavailable")
            passkey_init = next(v.split(";", 1)[0] for name, v in headers
                                if name.lower() == "set-cookie")
            denied_options, _, _, _ = fetch("/remote/v1/passkey/login/options", "POST",
                json.dumps({"actor_id": actor, "password": "WebAdmin operator password!"}).encode(), {
                    "Origin": origin, "Content-Type": "application/json", "Cookie": passkey_init,
                    "X-DC24H-Remote-CSRF": json.loads(payload)["csrf"]})
            if denied_options != 403:
                raise AssertionError("Revoked restored passkey unexpectedly began authentication")
            if os.environ.get("DC24H_VM_REPORT_DIR"):
                with (Path(os.environ["DC24H_VM_REPORT_DIR"]) / "beta06-restored-login.json").open(
                    "x", encoding="utf-8") as file:
                    json.dump({"schema": 1, "source_sha": sha, "os_image": f"ubuntu-{version}",
                               "evidence": "fresh-mariadb-five-store-real-reauth",
                               "old_browser_cookie_refused": True,
                               "fresh_password_totp_login_accepted": True,
                               "fresh_private_summary_accepted": True,
                               "replay_ledger_preserved": True,
                               "revoked_passkey_catalog_restored": True,
                               "revoked_passkey_login_options_refused": True,
                               "restored_browser_password_passkey_accepted": True,
                               "restored_browser_summary_accepted": True,
                               "restored_browser_assertion_replay_refused": True,
                               "restored_browser_revoke_invalidated_session": True,
                               "revoked_passkey_persisted_after_restart": True,
                               "production_recovery_claim": False,
                               "real_browser_webauthn_exercised": True}, file, indent=2, sort_keys=True)
                    file.write("\n")
        finally:
            if browser is not None and browser.poll() is None:
                # Chromium children share the disposable helper's private
                # process group. Reap them even if the browser driver failed.
                try:
                    os.killpg(browser.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
            terminate(browser)
            terminate(apache)
            terminate(node)
            terminate(cpp)
            db.stop()
            if recovered_db is not None:
                recovered_db.stop()


if __name__ == "__main__":
    if len(sys.argv) not in (4, 5):
        raise SystemExit("Usage: beta06_authenticated_api_process_tests.py "
                         "HUB ACCOUNT_WEBADMIN_SEED ACCOUNT_ADMIN [PINNED-HISTORICAL-CHECKOUT]")
    run(*(str(Path(value).resolve()) for value in sys.argv[1:]))
