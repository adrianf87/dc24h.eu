#!/usr/bin/python3
"""Build and provision a fresh dc24h.eu hub and WebAdmin on Debian or Ubuntu."""

import argparse
import fcntl
import getpass
import ipaddress
import json
import os
import pwd
import re
import secrets
import socket
import stat
import subprocess
import sys
import tempfile
import warnings
from dataclasses import asdict, dataclass
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1]
COMMAND_ENV = {"PATH": "/usr/sbin:/usr/bin:/sbin:/bin", "LANG": "C.UTF-8"}
CONFIG = Path("/etc/dc24h")
INSTALL_RECORD = CONFIG / "installer.json"
HUB_CONFIG = Path("/etc/dc24h-stage3")
HUB_STATE = Path("/var/lib/dc24h-stage3")
HUB_UNIT = Path("/etc/systemd/system/dc24h-stage3-adcs.service")
APACHE_SITE = Path("/etc/apache2/sites-available/dc24h-stage4-webadmin.conf")
APACHE_ENABLED = Path("/etc/apache2/sites-enabled/dc24h-stage4-webadmin.conf")
STAGE4_SERVICES = tuple(f"dc24h-stage4-{name}.service" for name in ("webadmin", "bridge", "auth"))
LEGACY_SERVICES = (
    "dc24h.service", "dc24h-twilight.service", "dc24h-twilight-stage3-adcs.service",
    *(unit.replace("dc24h-", "dc24h-twilight-", 1) for unit in STAGE4_SERVICES),
)
PACKAGE_GROUPS = {
    "Apache2": ("apache2",),
    "MariaDB": ("mariadb-server", "mariadb-client"),
    "Build and runtime dependencies": (
        "build-essential", "cmake", "libssl-dev", "libgcrypt20-dev", "libutf8proc-dev",
        "libmariadb-dev", "msmtp", "ca-certificates", "openssl", "iproute2",
    ),
}
SQL_IDENTIFIER = re.compile(r"[a-z][a-z0-9_]{0,26}")
RESERVED_DATABASES = {"mysql", "sys", "information_schema", "performance_schema", "dc24h_stage4_test"}
RESERVED_USERS = {"root", "mysql", "mariadb", "admin"}
MAILBOX = re.compile(r"[a-z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-z0-9](?:[a-z0-9.-]{0,251}[a-z0-9])?", re.IGNORECASE | re.ASCII)
PLACEHOLDER_PASSWORD = re.compile(
    r"(?:password|administrator|admin|root|changeme|replace|secret|example|placeholder|1234|qwerty|test)[0-9!@#$._-]*",
    re.IGNORECASE,
)


class InstallationError(Exception):
    pass


@dataclass(frozen=True)
class Endpoint:
    domain: str
    public_ipv4: str
    bind_ipv4: str
    port: str
    fullchain: str
    private_key: str


def run(*args, **options):
    return subprocess.run(args, env=COMMAND_ENV, check=True, **options)


def ask(label, valid, default=None):
    suffix = f" [{default}]" if default is not None else ""
    while True:
        value = input(f"{label}{suffix}: ").strip()
        if not value and default is not None:
            value = default
        if valid(value):
            return value
        print("Invalid value. Try again.")


def confirm(label, default=False):
    return ask(label, lambda value: value.lower() in {"y", "n"}, "Y" if default else "N").lower() == "y"


def password(label, minimum=0, repeat=False):
    with warnings.catch_warnings():
        warnings.simplefilter("error", getpass.GetPassWarning)
        while True:
            value = getpass.getpass(label + ": ")
            printable = len(value.encode("utf-8")) <= 1024 and all(ord(ch) >= 32 and ord(ch) != 127 for ch in value)
            if not printable or (minimum and (
                len(value) < minimum or value != value.strip() or len(set(value)) < 8
                or PLACEHOLDER_PASSWORD.fullmatch(value)
            )):
                print(f"Use a single-line password with at least {minimum} characters. "
                      "New passwords need eight distinct characters and no surrounding whitespace.")
                continue
            if not repeat or getpass.getpass("Confirm password: ") == value:
                return value
            print("Passwords differ. Try again.")


def missing_packages():
    approved = []
    declined = []
    for group, packages in PACKAGE_GROUPS.items():
        missing = []
        for package in packages:
            status = subprocess.run(
                ["dpkg-query", "-W", "-f=${db:Status-Status}", package],
                env=COMMAND_ENV, capture_output=True, text=True,
            )
            if status.returncode or status.stdout != "installed":
                missing.append(package)
        if not missing:
            print(f"Required packages for {group} are installed.")
            continue
        print(f"Missing packages for {group} ({', '.join(missing)})")
        (approved if confirm("Install these packages") else declined).extend(missing)
    if declined:
        raise InstallationError("Required packages were declined: " + ", ".join(declined))
    return approved


def dns_name(value):
    labels = value.lower().split(".")
    return (len(value) <= 253 and len(labels) >= 2 and re.match(r"[a-z]", labels[-1])
            and all(re.fullmatch(r"[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?", label) for label in labels))


def ipv4(value):
    try:
        address = ipaddress.IPv4Address(value)
        return not (address.is_unspecified or address.is_multicast or address.is_reserved)
    except ipaddress.AddressValueError:
        return False


def port(value):
    return bool(re.fullmatch(r"[1-9][0-9]{0,4}", value)) and int(value) <= 65535


def tls_path(value):
    return bool(re.fullmatch(r"/[a-zA-Z0-9._/-]+", value)) and Path(value).is_file()


def endpoint(label, default_port, shared=None):
    print(f"Enter the {label} endpoint. For NAT, enter the IPv4 assigned to this server as the local bind address.")
    if shared:
        domain, public_ipv4 = shared.domain, shared.public_ipv4
    else:
        domain = ask(f"{label} DNS domain", dns_name).lower()
        public_ipv4 = ask(f"{label} public IPv4", ipv4)
    bind_ipv4 = ask(f"{label} local bind IPv4", ipv4, shared.bind_ipv4 if shared else public_ipv4)
    tcp_port = ask(f"{label} TCP port (1-65535, except 3306 and 4278)",
                   lambda value: port(value) and value not in {"3306", "4278"}, default_port)
    if shared:
        fullchain, private_key = shared.fullchain, shared.private_key
    else:
        certificate_dir = "/etc/letsencrypt/live/" + domain
        fullchain = ask(f"{label} TLS full chain (absolute path)", tls_path, certificate_dir + "/fullchain.pem")
        private_key = ask(f"{label} TLS private key (absolute path)", tls_path, certificate_dir + "/privkey.pem")
    return Endpoint(domain, public_ipv4, bind_ipv4, tcp_port, fullchain, private_key)


def verify_endpoints(web, hub):
    if web.port == hub.port and (web.port == "443" or
                                {web.public_ipv4, web.bind_ipv4} & {hub.public_ipv4, hub.bind_ipv4}):
        raise InstallationError("Select distinct HTTPS and ADCS listener addresses or ports.")
    interfaces = json.loads(run("ip", "-j", "-4", "address", "show", capture_output=True).stdout)
    assigned = {item["local"] for interface in interfaces for item in interface.get("addr_info", [])}
    for name, selected in (("WebAdmin", web), ("Hub", hub)):
        if selected.bind_ipv4 not in assigned:
            raise InstallationError(f"{name} bind address {selected.bind_ipv4} is absent from local interfaces.")
        if name == "Hub" or selected.port != "443":
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
                try:
                    probe.bind((selected.bind_ipv4, int(selected.port)))
                except OSError:
                    raise InstallationError(f"{name} listener {selected.bind_ipv4}:{selected.port} is unavailable.") from None
        try:
            options = {"stdout": subprocess.PIPE, "stderr": subprocess.DEVNULL}
            san = run("openssl", "x509", "-in", selected.fullchain, "-noout", "-ext", "subjectAltName", **options)
            run("openssl", "verify", "-purpose", "sslserver", "-verify_hostname", selected.domain,
                "-untrusted", selected.fullchain, selected.fullchain, **options)
            certificate = run("openssl", "x509", "-in", selected.fullchain, "-noout", "-pubkey", **options)
            key = run("openssl", "pkey", "-in", selected.private_key, "-passin", "pass:", "-pubout", **options)
        except subprocess.CalledProcessError:
            raise InstallationError(f"{name} requires a trusted certificate for {selected.domain} and a readable key.") from None
        if b"DNS:" not in san.stdout or certificate.stdout != key.stdout:
            raise InstallationError(f"{name} TLS files must contain a DNS SAN and a matching private key.")


def read_owned(path, mode=0o600, uid=0, gid=0):
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    except FileNotFoundError:
        return None
    with os.fdopen(fd, "rb") as stream:
        info = os.fstat(stream.fileno())
        if (not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_uid != uid
                or info.st_gid != gid or stat.S_IMODE(info.st_mode) != mode or info.st_size > 65536):
            raise InstallationError(f"Unexpected ownership or permissions on {path}")
        return stream.read(65536)


def publish(path, data, mode=0o600, uid=0, gid=0):
    existing = read_owned(path, mode, uid, gid)
    if existing is not None:
        if existing != data:
            raise InstallationError(f"Existing file differs from the installation plan: {path}")
        return
    fd, temporary = tempfile.mkstemp(dir=path.parent, prefix=".dc24h-install-")
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fchown(stream.fileno(), uid, gid)
            os.fchmod(stream.fileno(), mode)
            os.fsync(stream.fileno())
        os.link(temporary, path, follow_symlinks=False)
    finally:
        os.unlink(temporary)


def directory(path, mode, uid=0, gid=0):
    try:
        path.mkdir(mode=mode)
        os.chown(path, uid, gid)
        path.chmod(mode)
    except FileExistsError:
        pass
    info = path.lstat()
    if (not stat.S_ISDIR(info.st_mode) or info.st_uid != uid or info.st_gid != gid
            or stat.S_IMODE(info.st_mode) != mode):
        raise InstallationError(f"Unexpected ownership or permissions on {path}")


def installation_record(plan):
    directory(CONFIG, 0o700)
    saved = read_owned(INSTALL_RECORD)
    if saved is not None:
        record = json.loads(saved)
        if record["plan"] != plan:
            raise InstallationError(f"Use the original settings to resume the installation recorded in {INSTALL_RECORD}.")
    else:
        if any(CONFIG.iterdir()) or any(os.path.lexists(path) for path in (
            APACHE_SITE, Path("/opt/dc24h"), Path("/opt/dc24h-stage3"), HUB_CONFIG, HUB_STATE,
            Path("/var/lib/dc24h-stage4"), Path("/var/lib/dc24h-stage4-auth"),
            Path("/usr/local/share/dc24h/stage4"), Path("/usr/local/sbin/dc24h-stage4-init"),
        )):
            raise InstallationError("An existing hub or WebAdmin installation needs the documented upgrade procedure.")
        record = {"plan": plan, "id": secrets.token_hex(32),
                  "web_password": secrets.token_hex(32), "hub_token": secrets.token_hex(32)}
    if os.path.lexists(APACHE_ENABLED):
        raise InstallationError("The Stage IV Apache site is enabled. Review it before attempting an installation.")
    for unit in (HUB_UNIT.name, *STAGE4_SERVICES, *LEGACY_SERVICES):
        properties = run("systemctl", "show", "--property=LoadState,ActiveState,DropInPaths", unit,
                         capture_output=True, text=True).stdout.splitlines()
        if "ActiveState=inactive" not in properties or "DropInPaths=" not in properties:
            raise InstallationError(f"Review the state and drop-ins of {unit} before installation.")
        if (saved is None or unit in LEGACY_SERVICES) and "LoadState=not-found" not in properties:
            raise InstallationError(f"An existing unit needs the documented upgrade procedure: {unit}")
    publish(INSTALL_RECORD, (json.dumps(record, indent=2) + "\n").encode())
    return record


def bootstrap(record, web, database, hub_user, root_user, db_socket, admin_email, credentials, smtp):
    template = (SOURCE / "config/stage4-webadmin-bootstrap.env.example").read_text(encoding="utf-8")
    fields = dict(line.split("=", 1) for line in template.splitlines() if line and not line.startswith("#"))
    fields.update(
        STAGE4_ENVIRONMENT="production", MARIADB_DATABASE=database, MARIADB_APPROVED_DATABASE=database,
        MARIADB_ROOT_USERNAME=root_user, MARIADB_SOCKET=db_socket, MARIADB_ROOT_PASSWORD=credentials["root"],
        MARIADB_APP_USERNAME=f"dc24h_{record['id'][:8]}_production", MARIADB_APP_PASSWORD=record["web_password"],
        MARIADB_HUB_USERNAME=hub_user, MARIADB_HUB_PASSWORD=credentials["hub"],
        ADMIN_EMAIL=admin_email, ADMIN_INITIAL_PASSWORD=credentials["admin"], SMTP_PASSWORD=credentials["smtp"],
        WEBADMIN_HOST=web.domain, WEBADMIN_PUBLIC_PORT=web.port, WEBADMIN_BIND_IPV4=web.bind_ipv4,
        INSTALLATION_ID=record["id"], **smtp,
    )
    run("/usr/local/sbin/dc24h-stage4-init", "--stdin",
        input="".join(f"{field}={value}\n" for field, value in fields.items()).encode("utf-8"))


def install_hub(hub, database, user, db_socket, hub_password, hub_token):
    identity = pwd.getpwnam("dc24h-stage3")
    uid, gid = identity.pw_uid, identity.pw_gid
    if not uid or not gid:
        raise InstallationError("A dedicated non-root hub identity is required.")
    directory(HUB_CONFIG, 0o750, gid=gid)
    directory(HUB_STATE, 0o700, uid, gid)
    publish(HUB_STATE / "database.password", hub_password.encode("utf-8"), uid=uid, gid=gid)
    publish(HUB_STATE / "api.token", hub_token.encode("ascii"), uid=uid, gid=gid)
    config = f"""[network]
listen = none
adcs_listen = {hub.bind_ipv4}:{hub.port}

[tls]
certificate = /run/credentials/{HUB_UNIT.name}/adcs-chain.pem
private_key = /run/credentials/{HUB_UNIT.name}/adcs-key.pem

[database]
enabled = on
socket = {db_socket}
name = {database}
user = {user}
password_file = {HUB_STATE}/database.password

[api]
enabled = on
socket = /run/dc24h-stage3-api/hub.sock
token_file = {HUB_STATE}/api.token
"""
    publish(HUB_CONFIG / "dc24h.ini", config.encode(), 0o640, gid=gid)
    unit = (SOURCE / "config/dc24h-stage3-adcs.service.example").read_text(encoding="utf-8")
    replacements = {
        "@HUB_HOSTNAME@": hub.domain, "@HUB_FULLCHAIN@": hub.fullchain, "@HUB_KEY@": hub.private_key,
        "@HUB_CAPABILITY@": "CAP_NET_BIND_SERVICE" if int(hub.port) < 1024 else "",
    }
    for field, value in replacements.items():
        unit = unit.replace(field, value)
    publish(HUB_UNIT, unit.encode(), 0o644)
    help_text = run("systemd-analyze", "--help", capture_output=True, text=True).stdout
    verify_args = ["--recursive-errors=no"] if "--recursive-errors=" in help_text else []
    run("systemd-analyze", *verify_args, "verify", str(HUB_UNIT))
    run("runuser", "-u", "dc24h-stage3", "--", "/opt/dc24h/bin/dc24h",
        "--config", str(HUB_CONFIG / "dc24h.ini"), "--database-status")
    run("systemctl", "daemon-reload")


def install():
    if os.geteuid() != 0 or not sys.stdin.isatty():
        raise InstallationError("Run the installer as root from an interactive terminal.")
    if platform_id() not in {"debian", "ubuntu"}:
        raise InstallationError("The installer requires Debian or Ubuntu with systemd.")
    os.umask(0o077)

    packages = missing_packages()
    node_dir = ask("Verified Node.js 24 LTS installation directory", lambda value:
                   value.startswith("/") and (Path(value) / "bin/node").is_file()
                   and (Path(value) / "bin/npm").is_file(), "/opt/node24-release")
    node_dir = str(Path(node_dir).resolve())
    version = run(node_dir + "/bin/node", "--version", capture_output=True, text=True).stdout
    if not version.startswith("v24."):
        raise InstallationError("Node.js 24 LTS with npm is required.")
    detected = run("nproc", capture_output=True, text=True).stdout.strip()
    jobs = ask("Parallel CMake build jobs (positive integer, detected CPUs shown)", lambda value:
               bool(re.fullmatch(r"[1-9][0-9]{0,8}", value)), detected)

    web = endpoint("WebAdmin", "7178")
    reuse = confirm("Use the WebAdmin domain and public IPv4 for the hub", default=True)
    hub = endpoint("ADCS hub", "1512", web if reuse else None)
    database = ask("Hub MariaDB database name (lowercase letters, digits and underscores)", lambda value:
                   SQL_IDENTIFIER.fullmatch(value) and value not in RESERVED_DATABASES, "dc24h")
    hub_user = ask("Hub MariaDB login", lambda value:
                   SQL_IDENTIFIER.fullmatch(value) and value not in RESERVED_USERS, "dc24h_hub")
    root_user = ask("MariaDB administrator login", lambda value:
                    SQL_IDENTIFIER.fullmatch(value) and value != hub_user, "root")
    db_socket = ask("MariaDB Unix socket", lambda value:
                    re.fullmatch(r"/run/(?:[a-z0-9_-]+/)+[a-z0-9_.-]+\.sock", value), "/run/mysqld/mysqld.sock")
    admin_email = ask("Administrator email for WebAdmin login codes", lambda value:
                      len(value) <= 254 and ".." not in value and MAILBOX.fullmatch(value), "admin@dc24h.eu").lower()
    smtp = {
        "SMTP_HOST": ask("SMTP server for Administrator email codes", lambda value:
                         re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9.-]{0,252}", value) is not None),
        "SMTP_PORT": ask("SMTP port", port, "587"),
        "SMTP_USERNAME": ask("SMTP login", lambda value:
                             re.fullmatch(r"[a-zA-Z0-9@._+%-]{1,254}", value) is not None, "admin@dc24h.eu"),
        "SMTP_STARTTLS": "true" if confirm("Use SMTP STARTTLS", default=True) else "false",
    }
    credentials = {
        "admin": password("Initial Administrator password", 15, repeat=True),
        "hub": password("Hub MariaDB password", 20, repeat=True),
        "root": password("MariaDB administrator password (Enter for local socket authentication)"),
        "smtp": password("SMTP password", 12),
    }
    if len({credentials["admin"], credentials["hub"], credentials["smtp"]}) != 3:
        raise InstallationError("Use distinct Administrator, hub database and SMTP passwords.")
    origin = "https://" + web.domain + (":" + web.port if web.port != "443" else "")
    print(f"WebAdmin {origin} ({web.public_ipv4}, bind {web.bind_ipv4})")
    print(f"ADCS hub adcs://{hub.domain}:{hub.port} ({hub.public_ipv4}, bind {hub.bind_ipv4})")
    print(f"Hub database {database}, login {hub_user}, build jobs {jobs}.")
    print("The installer builds the hub and npm packages, starts local MariaDB if needed, and prepares both services.")
    if not confirm("Proceed with installation"):
        print("Installation cancelled.")
        return

    plan = {"webadmin": asdict(web), "hub": asdict(hub),
            "database": database, "hub_user": hub_user, "socket": db_socket}
    with open("/run/dc24h-install.lock", "a", opener=lambda path, flags:
              os.open(path, flags | os.O_NOFOLLOW, 0o600)) as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if packages:
            run("apt-get", "update", umask=0o022)
            run("apt-get", "install", "-y", "--no-install-recommends", *packages, umask=0o022)
        verify_endpoints(web, hub)
        record = installation_record(plan)
        run("systemctl", "start", "mariadb.service")
        run("systemd-sysusers", "--inline", 'u dc24h-stage3 - "dc24h ADCS hub" /var/lib/dc24h-stage3 /usr/sbin/nologin')
        with tempfile.TemporaryDirectory(prefix="dc24h-build-", dir="/var/tmp") as build:
            run("cmake", "-S", str(SOURCE), "-B", build, "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=OFF")
            run("cmake", "--build", build, "--parallel", jobs, "--target", "dc24h", "dc24h-owner", "dc24h-account-admin")
            run(str(SOURCE / "scripts/stage4_install.sh"), node_dir, build, web.port, web.domain, web.bind_ipv4,
                web.fullchain, web.private_key, umask=0o022)
        bootstrap(record, web, database, hub_user, root_user, db_socket, admin_email, credentials, smtp)
        install_hub(hub, database, hub_user, db_socket, credentials["hub"], record["hub_token"])
    print("Installation prepared. Review DNS, routing, SMTP and the service activation steps in doc/us/INSTALLATION.md.")


def platform_id():
    return next((line.removeprefix("ID=").strip('"') for line in Path("/etc/os-release").read_text().splitlines()
                 if line.startswith("ID=")), "")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    try:
        install()
    except (EOFError, KeyboardInterrupt, getpass.GetPassWarning):
        print("\nInstallation interrupted.", file=sys.stderr)
        return 1
    except BlockingIOError:
        print("Another dc24h installer is running.", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as error:
        print(f"Installation failed while running {Path(error.cmd[0]).name} (exit {error.returncode}).", file=sys.stderr)
        return 1
    except (InstallationError, OSError, ValueError, KeyError) as error:
        print(f"Installation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
