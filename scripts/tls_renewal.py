#!/usr/bin/env python3
"""Manage the single OWNER-provisioned HTTP-01 lineage and the shared TLS bundle."""

from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime, timezone
import fcntl
import hashlib
import io
import ipaddress
import json
import math
import os
from pathlib import Path
import pwd
import re
import secrets
import shutil
import socket
import ssl
import stat
import struct
import subprocess
import sys
import tempfile
import time

from configobj import ConfigObj, ConfigObjError
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization


APPROVAL = "OWNER_TLS_HTTP01_ACTIVATION_GO"
TIMER = "dc24h-tls-renewal.timer"
HUB = "dc24h-stage3-adcs.service"
APACHE = "apache2.service"
AUTH_SERVICE = "/system.slice/dc24h-stage4-auth.service"
HUB_USER = "dc24h-stage3"
BRIDGE = "/opt/dc24h/scripts/stage4_webadmin_bridge.py"
WEBADMIN_SITE = Path("/etc/apache2/sites-enabled/dc24h-stage4-webadmin.conf")
SYSTEM_ENV = {"PATH": "/usr/sbin:/usr/bin:/sbin:/bin", "LC_ALL": "C", "TZ": "UTC",
              "HOME": "/nonexistent", "XDG_CONFIG_HOME": "/nonexistent"}
LE_DIRECTORY = "https://acme-v02.api.letsencrypt.org/directory"
PEM_LIMIT = 65536
BUNDLE = re.compile(r"cert-[0-9a-f]{64}")
DNS_LABEL = re.compile(r"[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?")
PEM = re.compile(rb"-----BEGIN CERTIFICATE-----\r?\n(?:[A-Za-z0-9+/=]+\r?\n)+-----END CERTIFICATE-----")


class Refused(Exception):
    pass


def now():
    return int(time.time())


def utc_timestamp(value):
    return int(value.replace(tzinfo=timezone.utc).timestamp())


def root_directory(path):
    info = path.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != 0 or info.st_mode & 0o022:
        raise Refused("invalid_config")


def read_file(path, size, private=False, group=None):
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK), "rb") as stream:
        info = os.fstat(stream.fileno())
        if (not stat.S_ISREG(info.st_mode) or info.st_uid != 0 or info.st_nlink != 1 or
                (group is not None and (info.st_gid != group or stat.S_IMODE(info.st_mode) != 0o640)) or
                info.st_mode & (0o077 if private else 0o022) or not 0 < info.st_size <= size):
            raise Refused("invalid_config")
        data = stream.read(size + 1)
        if len(data) > size or len(data) != info.st_size:
            raise Refused("invalid_config")
        return data


def load_json(path, default=None):
    try:
        return json.loads(read_file(path, 8192, private=True))
    except FileNotFoundError:
        return default


def sync_dir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def write_file(path, data, mode=0o600, gid=0):
    fd, temporary = tempfile.mkstemp(prefix=".dc24h-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            os.fchown(stream.fileno(), 0, gid)
            os.fchmod(stream.fileno(), mode)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        sync_dir(path.parent)
    finally:
        Path(temporary).unlink(missing_ok=True)


def save_json(path, value):
    write_file(path, (json.dumps(value, ensure_ascii=True, separators=(",", ":")) + "\n").encode())


@dataclass(frozen=True)
class Configuration:
    hostname: str
    lineage: str
    ipv4: str


@dataclass(frozen=True)
class Certificate:
    fullchain: bytes
    private_key: bytes
    fingerprint: str
    hostname: str
    issuer: str
    not_before: int
    not_after: int

    @property
    def bundle(self):
        return "cert-" + self.fingerprint

    def summary(self):
        return {key: getattr(self, key) for key in ("hostname", "issuer", "not_before", "not_after")}


def validate_certificate(chain, key, hostname, trust_file):
    try:
        blocks = PEM.findall(chain)
        if not blocks or len(blocks) > 8 or PEM.sub(b"", chain).strip():
            raise ValueError()
        cert = x509.load_pem_x509_certificate(blocks[0])
        san = cert.extensions.get_extension_for_class(x509.SubjectAlternativeName).value
        if len(san) != 1 or san.get_values_for_type(x509.DNSName) != [hostname]:
            raise ValueError()
        try:
            if cert.extensions.get_extension_for_class(x509.BasicConstraints).value.ca:
                raise ValueError()
        except x509.ExtensionNotFound:
            pass
        private = serialization.load_pem_private_key(key, password=None)
        encoding = serialization.Encoding.DER
        public_format = serialization.PublicFormat.SubjectPublicKeyInfo
        if private.public_key().public_bytes(encoding, public_format) != cert.public_key().public_bytes(encoding, public_format):
            raise ValueError()
        before_utc = getattr(cert, "not_valid_before_utc", None)
        after_utc = getattr(cert, "not_valid_after_utc", None)
        before = utc_timestamp(before_utc if before_utc is not None else cert.not_valid_before)
        after = utc_timestamp(after_utc if after_utc is not None else cert.not_valid_after)
        if not before <= now() < after:
            raise ValueError()
        with tempfile.TemporaryDirectory(prefix="dc24h-tls-verify-") as scratch:
            leaf = Path(scratch) / "leaf.pem"
            leaf.write_bytes(blocks[0] + b"\n")
            verify = ["/usr/bin/openssl", "verify", "-auth_level", "2", "-purpose", "sslserver",
                      "-verify_hostname", hostname, "-CAfile", str(trust_file), "-no-CApath", "-no-CAstore"]
            if len(blocks) > 1:
                intermediates = Path(scratch) / "chain.pem"
                intermediates.write_bytes(b"\n".join(blocks[1:]) + b"\n")
                verify.extend(["-untrusted", str(intermediates)])
            command([*verify, str(leaf)], timeout=2)
        issuer = "".join(character for character in cert.issuer.rfc4514_string()
                         if not 0 <= ord(character) < 32 and not 127 <= ord(character) <= 159)[:120]
        return Certificate(chain, key, cert.fingerprint(hashes.SHA256()).hex(),
                           hostname, issuer or "Unknown issuer", before, after)
    except (OSError, ValueError, TypeError, x509.ExtensionNotFound, subprocess.SubprocessError):
        raise Refused("invalid_certificate") from None


def command(args, *, timeout=3, capture=False, extra_env=None):
    result = subprocess.run(args, check=True, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE if capture else subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL, timeout=timeout,
                            env=SYSTEM_ENV | (extra_env or {}), text=capture)
    return result.stdout


class Host:
    config_dir = Path("/etc/dc24h-tls")
    state_dir = Path("/var/lib/dc24h-tls")
    letsencrypt = Path("/etc/letsencrypt")
    certbot_work = Path("/var/lib/letsencrypt")
    certbot_logs = Path("/var/log/letsencrypt")
    trust_file = Path("/etc/ssl/certs/ca-certificates.crt")
    acme_directory = LE_DIRECTORY
    http_port = 80
    adcs_port = 1512
    web_port = 7178
    apache_site = WEBADMIN_SITE
    certbot = "/usr/bin/certbot"
    hook = "/usr/local/libexec/dc24h-tls/certbot-deploy-hook"

    @property
    def served(self):
        return self.config_dir / "served"

    @property
    def hub_identity(self):
        entry = pwd.getpwnam(HUB_USER)
        if not entry.pw_uid or not entry.pw_gid:
            raise Refused("invalid_config")
        return entry.pw_uid, entry.pw_gid

    def systemctl(self, *args, capture=False, timeout=3):
        return command(["/usr/bin/systemctl", "--system", "--no-ask-password", *args],
                       capture=capture, timeout=timeout)

    def show(self, unit, *properties):
        args = [f"--property={name}" for name in properties]
        output = self.systemctl("show", *args, unit, capture=True)
        return dict(line.split("=", 1) for line in output.splitlines() if "=" in line)

    def check_network(self, config):
        try:
            output = command(["/usr/bin/getent", "ahosts", config.hostname], timeout=2, capture=True)
            addresses = {line.split()[0] for line in output.splitlines() if line}
            first = command(["/usr/bin/getent", "hosts", config.hostname], timeout=2, capture=True)
            if addresses != {config.ipv4} or {line.split()[0] for line in first.splitlines() if line} != addresses:
                raise Refused("dns_mismatch")
        except (OSError, subprocess.SubprocessError):
            raise Refused("dns_mismatch") from None
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
                # Match Certbot's standalone listener when closed connections are in TIME_WAIT.
                listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                listener.bind((config.ipv4, self.http_port))
        except OSError:
            raise Refused("http01_unavailable") from None

    def check_hub(self, config):
        unit = self.show(HUB, "ActiveState", "CanReload", "ExecReload")
        expected = f"argv[]=/usr/bin/python3 -I {BRIDGE} tls-reload ;"
        script = unit.get("ExecReload", "")
        if (unit.get("ActiveState") != "active" or unit.get("CanReload") != "yes" or
                script.count("argv[]=") != 1 or expected not in script or "ignore_errors=yes" in script):
            raise Refused("hub_reload_unavailable")
        try:
            command(["/usr/bin/setpriv", "--no-new-privs", "--reuid", HUB_USER, "--regid", HUB_USER,
                     "--clear-groups", "/usr/bin/python3", "-I", BRIDGE, "tls-probe"],
                    extra_env={"DC24H_TLS_RELOAD_HOSTNAME": config.hostname}, timeout=3)
        except (OSError, subprocess.SubprocessError):
            raise Refused("hub_reload_unavailable") from None

    def check_apache(self, config):
        unit = self.show(APACHE, "ActiveState", "CanReload", "ExecReload")
        script = unit.get("ExecReload", "")
        if (unit.get("ActiveState") != "active" or unit.get("CanReload") != "yes" or
                script.count("argv[]=") != 1 or "ignore_errors=yes" in script or
                not re.search(r"argv\[\]=/usr/sbin/apache(?:2)?ctl graceful ;", script)):
            raise Refused("apache_reload_unavailable")
        try:
            target = self.apache_site.resolve(strict=True)
            root_directory(target.parent)
            if target.parent != Path("/etc/apache2/sites-available"):
                raise Refused("apache_reload_unavailable")
            text = read_file(target, 32768).decode()
            for directive, leaf in (("SSLCertificateFile", "fullchain.pem"), ("SSLCertificateKeyFile", "privkey.pem")):
                paths = re.findall(rf"(?im)^\s*{directive}\s+(\S+)\s*$", text)
                if paths != [f'"{self.served / "current" / leaf}"']:
                    raise Refused("apache_reload_unavailable")
            output = command(["/usr/sbin/apache2ctl", "-t", "-D", "DUMP_VHOSTS"], capture=True)
            expected = rf":{self.web_port}\s+{re.escape(config.hostname)}\s+\({re.escape(str(self.apache_site))}:"
            if not re.search(expected, output):
                raise Refused("apache_reload_unavailable")
        except (OSError, ValueError, subprocess.SubprocessError):
            raise Refused("apache_reload_unavailable") from None

    def configtest(self):
        command(["/usr/sbin/apache2ctl", "configtest"], timeout=5)

    def reload(self, service):
        self.systemctl("reload", service, timeout=12)

    def application(self, config, certificate, service):
        address = (config.ipv4, self.adcs_port) if service == HUB else ("127.0.0.1", self.web_port)
        try:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            context.hostname_checks_common_name = False
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.load_verify_locations(cafile=str(self.trust_file))
            with socket.create_connection(address, timeout=1.5) as raw:
                with context.wrap_socket(raw, server_hostname=config.hostname) as peer:
                    actual = hashlib.sha256(peer.getpeercert(binary_form=True)).hexdigest()
            return "applied" if actual == certificate.fingerprint else "mismatch"
        except OSError:
            return "unavailable"

    def renew(self, config, nonce):
        command([self.certbot, "renew", "--cert-name", config.lineage, "--config", "/dev/null",
                 "--config-dir", str(self.letsencrypt), "--work-dir", str(self.certbot_work),
                 "--logs-dir", str(self.certbot_logs), "--no-random-sleep-on-renew", "--server", self.acme_directory,
                 "--standalone", "--preferred-challenges", "http-01", "--http-01-address", config.ipv4,
                 "--http-01-port", str(self.http_port),
                 "--non-interactive", "--no-directory-hooks", "--deploy-hook", self.hook],
                timeout=480, extra_env={"DC24H_TLS_HOOK_NONCE": nonce})


class Renewal:
    def __init__(self, host):
        self.host = host
        root_directory(host.state_dir)
        self.state_path = host.state_dir / "state.json"

    def configuration(self):
        try:
            root_directory(self.host.config_dir)
            fields = load_json(self.host.config_dir / "renewal.json")
            if fields is None:
                raise Refused("not_configured")
            if (not isinstance(fields, dict) or set(fields) != {"approval", "hostname", "lineage", "ipv4"} or
                    not all(isinstance(value, str) for value in fields.values())):
                raise Refused("invalid_config")
            if fields["approval"] != APPROVAL:
                raise Refused("not_configured")
            hostname = fields["hostname"]
            lineage = fields["lineage"]
            if (len(hostname) > 253 or "." not in hostname or
                    not all(DNS_LABEL.fullmatch(label) for label in hostname.split(".")) or
                    not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_.-]{0,253}", lineage)):
                raise Refused("invalid_config")
            address = ipaddress.IPv4Address(fields["ipv4"])
            if address.is_unspecified or address.is_multicast:
                raise Refused("invalid_config")
            return Configuration(hostname, lineage, str(address))
        except FileNotFoundError:
            raise Refused("not_configured") from None
        except (OSError, ValueError):
            raise Refused("invalid_config") from None

    def state(self):
        result = load_json(self.state_path, {})
        if not isinstance(result, dict):
            raise Refused("state_unavailable")
        return result

    @contextmanager
    def locked(self, blocking=False):
        fd = os.open(self.host.state_dir / "operation.lock",
                     os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
        try:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | (0 if blocking else fcntl.LOCK_NB))
            except BlockingIOError:
                raise Refused("busy") from None
            yield
        finally:
            os.close(fd)

    def audit(self, action, outcome, actor=None, error=None):
        record = {"at": now(), "action": action, "outcome": outcome, "actor_id": actor, "error": error}
        line = json.dumps(record, separators=(",", ":")).encode() + b"\n"
        print(line.decode().rstrip("\n"), file=sys.stderr, flush=True)
        try:
            fd = os.open(self.host.state_dir / "audit.jsonl",
                         os.O_WRONLY | os.O_APPEND | os.O_CREAT | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
            with os.fdopen(fd, "ab") as stream:
                stream.write(line)
                stream.flush()
                os.fsync(stream.fileno())
            sync_dir(self.host.state_dir)
        except OSError:
            raise Refused("audit_unavailable") from None

    def lineage_paths(self, config):
        root = self.host.letsencrypt
        try:
            for directory in (root, root / "renewal", root / "live", root / "archive"):
                root_directory(directory)
            live = root / "live" / config.lineage
            archive = root / "archive" / config.lineage
            root_directory(live)
            root_directory(archive)
            content = read_file(root / "renewal" / (config.lineage + ".conf"), 16384).decode()
            entries = ConfigObj(io.StringIO(content), interpolation=False, raise_errors=True)
            params = entries.get("renewalparams")
            allowed = {"account", "authenticator", "server", "key_type", "elliptic_curve", "rsa_key_size",
                       "pref_challs", "http01_port", "http01_address", "installer", "reuse_key",
                       "preferred_chain", "preferred_profile", "autorenew", "renew_hook", "deploy_hook",
                       "config_dir", "work_dir", "logs_dir"}
            if (not isinstance(params, dict) or set(params) - allowed or
                    entries.get("archive_dir") != str(archive) or
                    params.get("server") != self.host.acme_directory or params.get("authenticator") != "standalone" or
                    params.get("installer") not in (None, "None") or
                    params.get("autorenew", "True").lower() != "true" or
                    params.get("reuse_key", "False").lower() != "false" or
                    params.get("renew_hook", self.host.hook) != self.host.hook or
                    params.get("deploy_hook", self.host.hook) != self.host.hook or
                    params.get("config_dir", str(root)) != str(root) or
                    params.get("work_dir", str(self.host.certbot_work)) != str(self.host.certbot_work) or
                    params.get("logs_dir", str(self.host.certbot_logs)) != str(self.host.certbot_logs) or
                    params.get("pref_challs") not in ("http-01", ["http-01"]) or
                    params.get("http01_address") != config.ipv4 or
                    params.get("http01_port", str(self.host.http_port)) != str(self.host.http_port)):
                raise Refused("unsupported_acme")
            targets = {}
            versions = set()
            for key in ("cert", "chain", "fullchain", "privkey"):
                link = live / (key + ".pem")
                if entries.get(key) != str(link) or not link.is_symlink():
                    raise Refused("unsupported_acme")
                target = link.resolve(strict=True)
                version = re.fullmatch(rf"{key}([1-9][0-9]*)\.pem", target.name)
                if target.parent != archive or not version:
                    raise Refused("unsupported_acme")
                targets[key] = target
                versions.add(version.group(1))
            if len(versions) != 1:
                raise Refused("unsupported_acme")
            return targets
        except (OSError, AttributeError, ConfigObjError, TypeError, ValueError):
            raise Refused("unsupported_acme") from None

    def lineage(self, config):
        paths = self.lineage_paths(config)
        certificate = self.certificate(config, paths["fullchain"], paths["privkey"], private=True)
        try:
            # Certbot derives the renewal SANs from cert.pem. It must be the same leaf we validated.
            leaf = x509.load_pem_x509_certificate(read_file(paths["cert"], PEM_LIMIT))
            if leaf.fingerprint(hashes.SHA256()).hex() != certificate.fingerprint:
                raise Refused("invalid_certificate")
        except (OSError, ValueError):
            raise Refused("invalid_certificate") from None
        return certificate

    def certificate(self, config, chain, key, *, private=False, group=None):
        try:
            return validate_certificate(read_file(chain, PEM_LIMIT, group=group),
                                        read_file(key, PEM_LIMIT, private=private, group=group),
                                        config.hostname, self.host.trust_file)
        except OSError:
            raise Refused("invalid_certificate") from None

    def bundle(self, name, config):
        if not isinstance(name, str) or not BUNDLE.fullmatch(name):
            raise Refused("missing_baseline")
        directory = self.host.served / name
        root_directory(directory)
        gid = self.host.hub_identity[1]
        info = directory.lstat()
        if stat.S_IMODE(info.st_mode) != 0o750 or info.st_gid != gid:
            raise Refused("invalid_certificate")
        cert = self.certificate(config, directory / "fullchain.pem", directory / "privkey.pem", group=gid)
        if cert.bundle != name:
            raise Refused("invalid_certificate")
        return cert

    def current(self, config):
        try:
            root_directory(self.host.served)
            return self.bundle(os.readlink(self.host.served / "current"), config)
        except FileNotFoundError:
            raise Refused("missing_baseline") from None

    def stage(self, certificate):
        directory = self.host.served / certificate.bundle
        try:
            directory.mkdir(mode=0o700)
        except FileExistsError:
            root_directory(directory)
        gid = self.host.hub_identity[1]
        os.chown(directory, 0, gid)
        write_file(directory / "fullchain.pem", certificate.fullchain, 0o640, gid)
        write_file(directory / "privkey.pem", certificate.private_key, 0o640, gid)
        directory.chmod(0o750)
        sync_dir(self.host.served)

    def select(self, name):
        if not BUNDLE.fullmatch(name):
            raise Refused("invalid_certificate")
        temporary = self.host.served / (".current-" + secrets.token_hex(12))
        try:
            temporary.symlink_to(name)
            os.replace(temporary, self.host.served / "current")
            sync_dir(self.host.served)
        finally:
            temporary.unlink(missing_ok=True)

    def bootstrap(self):
        with self.locked():
            config = self.configuration()
            self.host.served.mkdir(mode=0o750, exist_ok=True)
            root_directory(self.host.served)
            os.chown(self.host.served, 0, self.host.hub_identity[1])
            self.host.served.chmod(0o750)
            try:
                missing = not os.path.lexists(self.host.served / "current")
                certificate = self.lineage(config) if missing else self.current(config)
                if missing:
                    self.stage(certificate)
                    self.select(certificate.bundle)
                state = self.state()
                state.update(certificate=certificate.summary(), certificate_invalid=False)
                save_json(self.state_path, state)
                self.audit("bootstrap", "succeeded")
            except OSError:
                raise Refused("state_unavailable") from None

    def schedule(self):
        try:
            fields = self.host.show(TIMER, "LoadState", "ActiveState", "UnitFileState", "NextElapseUSecRealtime")
            if fields.get("LoadState") != "loaded":
                return "unknown", None
            active = fields.get("ActiveState") == "active"
            enabled = fields.get("UnitFileState") == "enabled"
            wakeup = fields.get("NextElapseUSecRealtime", "")
            if active and enabled:
                try:
                    due = utc_timestamp(datetime.strptime(wakeup, "%a %Y-%m-%d %H:%M:%S UTC"))
                except ValueError:
                    due = None
                return "active", due
            if not active and fields.get("UnitFileState") == "disabled":
                return "disabled", None
            return "inconsistent", None
        except (OSError, subprocess.SubprocessError):
            return "unknown", None

    def status(self, request_error=None):
        try:
            self.configuration()
            configured = True
            configuration_error = None
        except Refused as error:
            configured = False
            configuration_error = str(error)
        state = self.state()
        schedule, due = self.schedule()
        cert = state.get("certificate")
        days = math.ceil((cert["not_after"] - now()) / 86400) if cert else None
        certificate_status = "unavailable"
        if cert:
            certificate_status = "expired" if now() >= cert["not_after"] else "expiring" if days <= 30 else "valid"
        if state.get("certificate_invalid") and certificate_status != "expired":
            certificate_status = "invalid"
        readiness_error = configuration_error or state.get("readiness_error")
        if state.get("pending"):
            readiness_error = "rollback_pending"
        if schedule == "unknown" and not readiness_error:
            readiness_error = "schedule_unavailable"
        return {"schema": 1, "configured": configured, "enabled": schedule == "active",
                "ready": bool(state.get("checked_at") and not readiness_error and certificate_status in ("valid", "expiring")),
                "schedule": schedule, "certificate": cert, "certificate_status": certificate_status,
                "days_remaining": days, "next_check": due, "checked_at": state.get("checked_at"),
                "last_attempt": state.get("last_attempt"), "last_renewal": state.get("last_renewal"),
                "apache": state.get("apache", "unavailable"), "hub": state.get("hub", "unavailable"),
                "readiness_error": readiness_error, "renewal_error": state.get("renewal_error"),
                "request_error": request_error}

    def verify_application(self, state, config, certificate):
        state["hub"] = self.host.application(config, certificate, HUB)
        state["apache"] = self.host.application(config, certificate, APACHE)
        return state["hub"] == state["apache"] == "applied"

    def check(self, state):
        state.update(checked_at=now(), hub="unavailable", apache="unavailable")
        try:
            config = self.configuration()
            if state.get("pending"):
                raise Refused("rollback_pending")
            try:
                certificate = self.current(config)
            except Refused as error:
                state["certificate_invalid"] = str(error) == "invalid_certificate"
                raise
            state.update(certificate=certificate.summary(), certificate_invalid=False)
            served = self.verify_application(state, config, certificate)
            if not os.access(self.host.certbot, os.X_OK) or not os.access(self.host.hook, os.X_OK):
                raise Refused("certbot_unavailable")
            # Certbot merges its default cli.ini even with --config /dev/null.
            try:
                cli = read_file(Path("/etc/letsencrypt/cli.ini"), 16384).decode()
            except FileNotFoundError:
                cli = ""
            for line in (item.strip() for item in cli.splitlines()):
                if line and not line.startswith(("#", ";")) and not re.fullmatch(
                        r"(?:max-log-backups\s*=\s*[0-9]{1,3}|preconfigured-renewal\s*=\s*(?:True|False))", line):
                    raise Refused("unsupported_acme")
            self.lineage(config)
            for unit in ("certbot.timer", "snap.certbot.renew.timer"):
                status = self.host.show(unit, "ActiveState", "UnitFileState")
                if status.get("ActiveState") not in (None, "inactive", "failed") or status.get(
                        "UnitFileState", "").startswith("enabled"):
                    raise Refused("competing_scheduler")
            self.host.check_network(config)
            self.host.check_hub(config)
            self.host.check_apache(config)
            if not served:
                raise Refused("services_not_verified")
            if self.schedule()[0] == "unknown":
                raise Refused("schedule_unavailable")
            state["readiness_error"] = None
            return config, certificate
        except Refused as error:
            state["readiness_error"] = str(error)
            raise
        except (OSError, subprocess.SubprocessError):
            state["readiness_error"] = "state_unavailable"
            raise Refused("state_unavailable") from None
        finally:
            save_json(self.state_path, state)

    def control(self, operation, actor):
        if operation == "status":
            return self.status()
        error = None
        try:
            with self.locked():
                self.audit(operation, "started", actor)
                try:
                    if operation != "disable":
                        self.check(self.state())
                    if operation in ("enable", "disable"):
                        self.set_schedule(operation)
                except Refused as refusal:
                    error = str(refusal)
                self.audit(operation, "failed" if error else "succeeded", actor, error)
        except Refused as refusal:
            error = str(refusal)
        return self.status(error)

    def set_schedule(self, operation):
        expected = "active" if operation == "enable" else "disabled"
        if self.schedule()[0] == expected:
            return
        try:
            self.host.systemctl(operation, "--now", TIMER, timeout=4)
            if self.schedule()[0] == expected:
                return
        except (OSError, subprocess.SubprocessError):
            pass
        if operation == "enable":
            try:
                self.host.systemctl("disable", "--now", TIMER, timeout=3)
            except (OSError, subprocess.SubprocessError):
                pass
        raise Refused("timer_change_failed")

    def hook(self, nonce, lineage):
        if not re.fullmatch(r"[0-9a-f]{32}", nonce):
            raise Refused("hook_missing")
        config = self.configuration()
        if lineage != str(self.host.letsencrypt / "live" / config.lineage):
            raise Refused("hook_missing")
        certificate = self.lineage(config)
        save_json(self.host.state_dir / "receipt.json", {"nonce": nonce, "fingerprint": certificate.fingerprint})

    def observe(self, config, certificate, service, attempts=8):
        result = "unavailable"
        for attempt in range(attempts):
            result = self.host.application(config, certificate, service)
            if result == "applied":
                break
            if attempt + 1 < attempts:
                time.sleep(0.25)
        return result

    def restore(self, state, config):
        try:
            old = self.bundle(state["pending"], config)
            self.select(old.bundle)
            for service, key in ((HUB, "hub"), (APACHE, "apache")):
                result = self.host.application(config, old, service)
                if result != "applied":
                    try:
                        self.host.reload(service)
                    except (OSError, subprocess.SubprocessError):
                        pass
                    result = self.observe(config, old, service)
                state[key] = result
            if state["hub"] != "applied" or state["apache"] != "applied":
                raise Refused("rollback_failed")
            state["certificate"] = old.summary()
            state.pop("pending")
            save_json(self.state_path, state)
        except (OSError, Refused):
            state["renewal_error"] = "rollback_failed"
            save_json(self.state_path, state)
            raise Refused("rollback_failed") from None

    def apply(self, state, config, previous, candidate):
        if candidate.not_before < previous.not_before or candidate.not_after - now() < 86400:
            raise Refused("invalid_certificate")
        self.stage(candidate)
        state["pending"] = previous.bundle
        save_json(self.state_path, state)
        try:
            self.select(candidate.bundle)
            self.host.configtest()
            for service, key in ((HUB, "hub"), (APACHE, "apache")):
                self.host.reload(service)
                state[key] = self.observe(config, candidate, service)
                if state[key] != "applied":
                    raise Refused("activation_failed")
        except (OSError, Refused, subprocess.SubprocessError):
            self.restore(state, config)
            raise Refused("activation_failed") from None
        state.pop("pending")
        state.update(certificate=candidate.summary(), last_renewal=now(), checked_at=now(), renewal_error=None)
        save_json(self.state_path, state)
        for entry in self.host.served.iterdir():
            if BUNDLE.fullmatch(entry.name) and entry.name not in (previous.bundle, candidate.bundle):
                try:
                    shutil.rmtree(entry)
                except OSError:
                    self.audit("prune", "failed", error="state_unavailable")

    def recover(self):
        with self.locked(blocking=True):
            state = self.state()
            if state.get("pending"):
                self.restore(state, self.configuration())
                self.audit("rollback", "succeeded")

    def run(self):
        with self.locked(blocking=True):
            if self.schedule()[0] != "active":
                return
            state = self.state()
            self.audit("renew", "started")
            state["last_attempt"] = now()
            save_json(self.state_path, state)
            try:
                if state.get("pending"):
                    self.restore(state, self.configuration())
                    self.audit("rollback", "succeeded")
                config, previous = self.check(state)
                before = self.lineage(config)
                nonce = secrets.token_hex(16)
                try:
                    self.host.renew(config, nonce)
                except (OSError, subprocess.SubprocessError):
                    raise Refused("certbot_failed") from None
                candidate = self.lineage(config)
                if candidate.fingerprint != before.fingerprint:
                    receipt = load_json(self.host.state_dir / "receipt.json")
                    if receipt != {"nonce": nonce, "fingerprint": candidate.fingerprint}:
                        raise Refused("hook_missing")
                rotated = candidate.fingerprint != previous.fingerprint
                if rotated:
                    self.apply(state, config, previous, candidate)
                state["renewal_error"] = None
                save_json(self.state_path, state)
            except (Refused, OSError, ValueError) as error:
                failure = str(error) if isinstance(error, Refused) else "state_unavailable"
                # Reload the last atomic state so a failed write cannot discard a pending rollback.
                latest = self.state()
                latest["renewal_error"] = failure
                save_json(self.state_path, latest)
                self.audit("renew", "failed", error=failure)
                raise Refused(failure) from None
            self.audit("renew", "succeeded" if rotated else "not_due")


def allowed_peer(peer, uid):
    pid, user, _group = struct.unpack("3i", peer.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
    if uid == 0 or user != uid or pid < 2:
        return False
    try:
        # Match the service cgroup as well as the UID shared by hub processes.
        # Ubuntu 22.04 and later default to the unified cgroup hierarchy.
        return f"0::{AUTH_SERVICE}" in Path(f"/proc/{pid}/cgroup").read_text().splitlines()
    except OSError:
        return False


def serve(peer, renewal, authorized):
    peer.settimeout(2)
    if not authorized(peer):
        return
    with peer.makefile("rb") as reader:
        request = reader.read(64)
    match = re.fullmatch(rb"(status|check|enable|disable) ([1-9][0-9]{0,18})\n", request)
    if not match or int(match[2]) > 9223372036854775807:
        return
    result = renewal.control(match[1].decode(), int(match[2]))
    peer.sendall((json.dumps(result, ensure_ascii=True, separators=(",", ":")) + "\n").encode())


def main():
    if os.geteuid() != 0 or len(sys.argv) != 2:
        return 1
    operation = sys.argv[1]
    try:
        host = Host()
        renewal = Renewal(host)
        if operation == "control":
            with socket.socket(fileno=os.dup(0)) as peer:
                serve(peer, renewal, lambda client: allowed_peer(client, host.hub_identity[0]))
        elif operation == "bootstrap":
            renewal.bootstrap()
        elif operation == "run":
            renewal.run()
        elif operation == "recover":
            renewal.recover()
        elif operation == "deploy-hook":
            renewal.hook(os.environ.get("DC24H_TLS_HOOK_NONCE", ""), os.environ.get("RENEWED_LINEAGE", ""))
        else:
            return 1
    except (OSError, KeyError, Refused, ValueError):
        print("dc24h TLS operation failed; see the root-only TLS audit and Certbot logs", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
