"""Exercise Certbot, Pebble, Apache2 and live ADCS in a loopback-only namespace."""

from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from http.client import HTTPSConnection
import json
import os
from pathlib import Path
import shlex
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import unittest
from urllib.request import urlopen

from network_process_tests import Hub
from tls_support import TLS_READY, tls_endpoint

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from stage3.fixtures import authority, issue, loopback_context, write_config
from stage4_webadmin_bridge import serving_tls
from tls_renewal import (APACHE, APPROVAL, HUB, TIMER, Host, Refused, Renewal,
                         command, now, serve, validate_certificate, write_file)


PEBBLE = sys.argv.pop(1) if __name__ == "__main__" else None
HOSTNAME = "fixture-renewal.test"
IP = "127.0.0.1"


def wait_until(condition):
    for _ in range(60):
        try:
            result = condition()
            if result:
                return result
        except OSError:
            pass
        time.sleep(0.12)
    raise AssertionError("Timed out waiting for an isolated TLS service")


class FixtureHost(Host):
    acme_directory = "https://localhost:14000/dir"

    def __init__(self, root):
        self.root = root
        self.config_dir = root / "config"
        self.state_dir = root / "state"
        self.letsencrypt = root / "certbot"
        self.certbot_work = root / "certbot-work"
        self.certbot_logs = root / "certbot-logs"
        self.trust_file = root / "pebble-issuer.pem"
        self.token = root / "api.token"
        self.api = root / "api" / "hub.sock"
        self.apache_site = root / "apache.conf"
        self.certbot = str(root / "certbot-command")
        self.hook = str(root / "hook")
        self.active = False
        self.toggle_count = 0
        self.fail_apache = False
        self.competing = False
        self.services = []

    @property
    def hub_identity(self):
        return 0, 0

    def systemctl(self, action, now_flag, unit, **kwargs):
        assert action in ("enable", "disable") and now_flag == "--now" and unit == TIMER
        self.active = action == "enable"
        self.toggle_count += 1

    def show(self, unit, *_properties):
        if unit == TIMER:
            return {"LoadState": "loaded", "ActiveState": "active" if self.active else "inactive",
                    "UnitFileState": "enabled" if self.active else "disabled",
                    "NextElapseUSecRealtime": "Tue 2026-12-15 03:01:00 UTC"}
        assert unit in ("certbot.timer", "snap.certbot.renew.timer")
        return {"ActiveState": "active" if self.competing else "inactive", "UnitFileState": "disabled"}

    def check_hub(self, config):
        values = serving_tls("GET", self.api, self.token)
        expected = self.served / "current"
        if values != {"schema": 1, "hostname": config.hostname, "certificate_chain": str(expected / "fullchain.pem"),
                      "private_key_path": str(expected / "privkey.pem")}:
            raise Refused("hub_reload_unavailable")

    def check_apache(self, _config):
        self.configtest()

    def configtest(self):
        command(["/usr/sbin/apache2", "-f", str(self.apache_site), "-t"], timeout=5)

    def reload(self, service):
        self.services.append(service)
        if service == HUB:
            assert serving_tls("POST", self.api, self.token) == {"status": "reloaded"}
        else:
            assert service == APACHE
            command(["/usr/sbin/apache2", "-f", str(self.apache_site), "-k", "graceful"], timeout=5)
            if self.fail_apache:
                self.fail_apache = False
                renewal = Renewal(self)
                config = renewal.configuration()
                candidate = renewal.current(config)
                wait_until(lambda: self.application(config, candidate, APACHE) == "applied")
                raise subprocess.CalledProcessError(1, "fixture Apache activation fault")


class AcmeRenewalTests(unittest.TestCase):
    def test_acme_rotation_rollback_and_private_controller(self):
        self.assertEqual(os.geteuid(), 0)
        for namespace in ("net", "mnt"):
            self.assertNotEqual(os.readlink("/proc/self/ns/" + namespace), os.readlink("/proc/1/ns/" + namespace),
                                "Use a private mount and network namespace")
        command(["/usr/sbin/ip", "link", "set", "lo", "up"])
        links = json.loads(command(["/usr/sbin/ip", "-j", "link", "show"], capture=True))
        self.assertEqual([link["ifname"] for link in links], ["lo"])
        scratch = tempfile.TemporaryDirectory(prefix="dc24h-acme-fixture-")
        self.addCleanup(scratch.cleanup)
        self.exercise(Path(scratch.name))

    def exercise(self, root):
        root.chmod(0o755)
        hosts = root / "hosts"
        hosts.write_text(f"127.0.0.1 localhost {HOSTNAME}\n")
        command(["/usr/bin/mount", "--bind", str(hosts), "/etc/hosts"])
        self.addCleanup(command, ["/usr/bin/umount", "/etc/hosts"])
        command(["/usr/bin/mount", "-t", "tmpfs", "-o", "mode=0700", "tmpfs", "/etc/letsencrypt"])
        self.addCleanup(command, ["/usr/bin/umount", "/etc/letsencrypt"])
        transport_ca = authority(root / "transport")
        acme_cert, acme_key = issue(root, transport_ca, "acme-server")
        transport_trust = root / "transport" / "ca.pem"
        pebble_config = root / "pebble.json"
        pebble_config.write_text(json.dumps({"pebble": {"listenAddress": "127.0.0.1:14000",
            "managementListenAddress": "127.0.0.1:15000", "certificate": str(acme_cert),
            "privateKey": str(acme_key), "httpPort": 80, "tlsPort": 5001}}))
        pebble = self.start([PEBBLE, "-config", str(pebble_config)], root / "pebble.log",
                            {"PEBBLE_VA_NOSLEEP": "1", "PEBBLE_AUTHZREUSE": "0", "PEBBLE_WFE_NONCEREJECT": "0"})
        context = loopback_context(transport_trust)

        def get_issuer():
            with urlopen("https://localhost:15000/roots/0", context=context, timeout=2) as res:
                return res.read(8192)

        host = FixtureHost(root)
        host.trust_file.write_bytes(wait_until(get_issuer))
        host.config_dir.mkdir()
        host.state_dir.mkdir(mode=0o700)
        write_file(host.config_dir / "renewal.json", json.dumps(
            {"approval": APPROVAL, "hostname": HOSTNAME, "lineage": HOSTNAME, "ipv4": IP}).encode())
        certbot_paths = ["--config", "/dev/null", "--config-dir", str(host.letsencrypt), "--work-dir",
                         str(host.certbot_work), "--logs-dir", str(host.certbot_logs)]
        command(["/usr/bin/certbot", "certonly", *certbot_paths, "--agree-tos", "--register-unsafely-without-email",
                 "--non-interactive", "--no-directory-hooks", "--standalone", "--preferred-challenges", "http-01",
                 "--http-01-address", IP, "--server", host.acme_directory, "-d", HOSTNAME], timeout=40,
                extra_env={"REQUESTS_CA_BUNDLE": str(transport_trust)})

        force = root / "fixture-due"
        write_file(Path(host.certbot), (
            f"#!/bin/sh\nexport REQUESTS_CA_BUNDLE={shlex.quote(str(transport_trust))}\n"
            f"if test -f {shlex.quote(str(force))}; then rm -f {shlex.quote(str(force))}; "
            "set -- \"$@\" --force-renewal; fi\nexec /usr/bin/certbot \"$@\"\n").encode(), 0o700)
        # Certbot invokes the actual hook entry point against the fixture's private paths.
        environment = {name: str(getattr(host, name)) for name in
                       ("config_dir", "state_dir", "letsencrypt", "trust_file", "certbot_work",
                        "certbot_logs", "acme_directory", "hook")}
        script = Path(__file__).resolve().parents[1] / "scripts"
        hook_code = ("import os,sys\nsys.path.insert(0," + repr(str(script)) + ")\n"
                     "from tls_renewal import Host,Renewal,Path\nhost=Host()\n")
        for name, value in environment.items():
            literal = repr(value) if name in ("acme_directory", "hook") else f"Path({value!r})"
            hook_code += f"host.{name}={literal}\n"
        hook_code += ("Renewal(host).hook(os.environ['DC24H_TLS_HOOK_NONCE'],os.environ['RENEWED_LINEAGE'])\n")
        write_file(Path(host.hook), ("#!/bin/sh\nexec /usr/bin/python3 -I -c " + shlex.quote(hook_code) + "\n").encode(),
                   0o700)

        renewal = Renewal(host)
        renewal.bootstrap()
        config = renewal.configuration()
        first = renewal.current(config)
        self.assertLessEqual(first.not_before, now())
        self.assertGreater(first.not_after, now())
        with self.assertRaisesRegex(Refused, "invalid_certificate"):
            validate_certificate(first.fullchain, acme_key.read_bytes(), HOSTNAME, host.trust_file)
        with self.assertRaisesRegex(Refused, "invalid_certificate"):
            validate_certificate(first.fullchain, first.private_key, "wrong.test", host.trust_file)
        with self.assertRaisesRegex(Refused, "invalid_certificate"):
            validate_certificate(first.fullchain, first.private_key, HOSTNAME, transport_trust)
        with self.assertRaisesRegex(Refused, "invalid_certificate"):
            validate_certificate(b"not PEM", first.private_key, HOSTNAME, host.trust_file)
        pair = host.served / "current" / "fullchain.pem", host.served / "current" / "privkey.pem"
        host.api.parent.mkdir(mode=0o700)
        write_file(host.token, b"d" * 64 + b"\n")
        ini = root / "hub.ini"
        write_config(ini, pair, port=host.adcs_port, idle=110)
        ini.write_text(ini.read_text().replace(
            "[api]\nenabled=off", f"[api]\nenabled=on\nsocket={host.api}\ntoken_file={host.token}"))
        from unittest.mock import patch
        with patch.dict(os.environ, {"DC24H_TLS_RELOAD_HOSTNAME": HOSTNAME, "SSL_CERT_FILE": str(host.trust_file)}):
            hub = Hub(self, "--config", str(ini), ready_pattern=TLS_READY)
        self.assertEqual(tls_endpoint(hub), (IP, host.adcs_port))

        document = root / "html"
        document.mkdir(mode=0o755)
        (document / "index.html").write_text("Isolated WebAdmin TLS\n")
        host.apache_site.write_text(f"""
ServerRoot "{root}"
ServerName {HOSTNAME}
DefaultRuntimeDir "{root}"
PidFile "{root / 'apache.pid'}"
ErrorLog "{root / 'apache-error.log'}"
Listen {IP}:{host.web_port}
LoadModule mpm_event_module /usr/lib/apache2/modules/mod_mpm_event.so
LoadModule authz_core_module /usr/lib/apache2/modules/mod_authz_core.so
LoadModule ssl_module /usr/lib/apache2/modules/mod_ssl.so
User nobody
Group nogroup
<VirtualHost {IP}:{host.web_port}>
    ServerName {HOSTNAME}
    DocumentRoot "{document}"
    <Directory "{document}">
        Require all granted
    </Directory>
    SSLEngine on
    SSLCertificateFile "{pair[0]}"
    SSLCertificateKeyFile "{pair[1]}"
</VirtualHost>
""")
        apache = self.start(["/usr/sbin/apache2", "-f", str(host.apache_site), "-DFOREGROUND"], root / "apache.log")
        wait_until(lambda: host.application(config, first, APACHE) == "applied")

        def controller(data, allow=True):
            client, server = socket.socketpair()
            with client, server:
                client.sendall(data)
                client.shutdown(socket.SHUT_WR)
                serve(server, renewal, lambda _peer: allow)
                server.shutdown(socket.SHUT_WR)
                with client.makefile("rb") as response:
                    payload = response.read()
                return json.loads(payload) if payload else None

        self.assertIsNone(controller(b"enable 42\n", allow=False))
        self.assertIsNone(controller(b"enable 42\nreload apache2\n"))
        self.assertIsNone(controller(b"restart apache2\n"))
        self.assertIsNone(controller(b"enable 9999999999999999999\n"))
        cli = Path("/etc/letsencrypt/cli.ini")
        write_file(cli, b"pre-hook = /usr/bin/true\n")
        self.assertEqual(controller(b"check 42\n")["request_error"], "unsupported_acme")
        cli.unlink()
        host.competing = True
        rejected = controller(b"enable 42\n")
        self.assertEqual((rejected["enabled"], rejected["request_error"]), (False, "competing_scheduler"))
        host.competing = False
        status = controller(b"enable 42\n")
        self.assertEqual((status["ready"], status["enabled"], status["hub"], status["apache"]),
                         (True, True, "applied", "applied"), status)
        self.assertIsInstance(status["next_check"], int)
        self.assertEqual(status["certificate"]["not_after"], first.not_after)
        self.assertEqual(status["days_remaining"], -((now() - first.not_after) // 86400))
        self.assertTrue(controller(b"enable 42\n")["enabled"])
        self.assertEqual(host.toggle_count, 1)
        with renewal.locked():
            self.assertEqual(controller(b"disable 42\n")["request_error"], "busy")
        self.assertTrue(controller(b"status 42\n")["enabled"])

        force.touch()
        host.fail_apache = True
        with self.assertRaisesRegex(Refused, "activation_failed"):
            renewal.run()
        rollback = renewal.status()
        self.assertEqual((rollback["hub"], rollback["apache"], rollback["renewal_error"], rollback["last_renewal"]),
                         ("applied", "applied", "activation_failed", None))
        self.assertEqual(renewal.current(config).fingerprint, first.fingerprint)
        self.assertEqual(host.services, [HUB, APACHE, HUB, APACHE])
        self.assertEqual(host.application(config, first, HUB), "applied")
        self.assertEqual(host.application(config, first, APACHE), "applied")

        # The next scheduled checks reconcile the already-renewed lineage.
        # Only one worker may deploy it even if two checks arrive together.
        with ThreadPoolExecutor(max_workers=2) as pool:
            list(pool.map(lambda _index: renewal.run(), range(2)))
        final = renewal.current(config)
        self.assertNotEqual(final.fingerprint, first.fingerprint)
        self.assertEqual(host.services, [HUB, APACHE, HUB, APACHE, HUB, APACHE])
        self.assertEqual(os.path.realpath(host.served / "current"), str(host.served / final.bundle))
        self.assertTrue((host.served / first.bundle).is_dir())
        self.assertEqual(renewal.status()["last_renewal"] is not None, True)
        self.assertEqual(host.application(config, final, HUB), "applied")
        self.assertEqual(host.application(config, final, APACHE), "applied")
        self.assertIsNone(hub.process.poll())
        self.assertIsNone(apache.poll())
        self.assertIsNone(pebble.poll())

        with closing(HTTPSConnection(HOSTNAME, host.web_port, timeout=2, context=loopback_context(host.trust_file))) as https:
            https.request("GET", "/index.html")
            result = https.getresponse()
            self.assertEqual((result.status, result.read()), (200, b"Isolated WebAdmin TLS\n"))

        self.assertFalse(controller(b"disable 42\n")["enabled"])
        self.assertFalse(controller(b"disable 42\n")["enabled"])
        self.assertEqual(host.toggle_count, 2)
        before = renewal.status()["last_attempt"]
        renewal.run()
        self.assertEqual(renewal.status()["last_attempt"], before)
        self.assertEqual(host.application(config, final, HUB), "applied")
        self.assertEqual(host.application(config, final, APACHE), "applied")
        audit = [json.loads(line) for line in (host.state_dir / "audit.jsonl").read_text().splitlines()]
        self.assertEqual([entry["outcome"] for entry in audit if entry["action"] == "renew"],
                         ["started", "failed", "started", "succeeded", "started", "not_due"])
        self.assertIn(42, {entry["actor_id"] for entry in audit if entry["action"] == "enable"})
        self.assertNotIn("PRIVATE KEY", json.dumps(audit))
        self.assertEqual((host.served / final.bundle / "privkey.pem").stat().st_mode & 0o777, 0o640)

    def start(self, args, log, env=None):
        with log.open("wb") as output:
            process = subprocess.Popen(args, stdout=output, stderr=output, stdin=subprocess.DEVNULL,
                                       env=os.environ | (env or {}), start_new_session=True)
        self.addCleanup(self.stop, process)
        return process

    @staticmethod
    def stop(process):
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=4)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=3)


if __name__ == "__main__":
    unittest.main()
