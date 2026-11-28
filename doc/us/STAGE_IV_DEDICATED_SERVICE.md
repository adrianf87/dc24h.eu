# Stage IV WebAdmin operations

For a host running Twilight units or an existing installation under these names, follow the [migration and rollback guide](REBRAND_MIGRATION.md) first.

The approved Ubuntu endpoint is `https://server-747585786.dc24h.eu:7178`.
The historical Debian endpoint is `https://server-747585786.dc24h.eu:7278`.
Use the port of the approved installation throughout this procedure. Apache
terminates TLS and forwards approved API paths to Node at `127.0.0.1:4278`.
The Stage III guest hub remains on ADCS TCP 1512.

Node runs as `dc24h-webadmin` and uses a dedicated local MariaDB account. The
C++ account service uses the existing account and password-verifier tables in
the selected database. It opens a private Unix socket and runs without hub
listeners, so the existing Stage III service can continue serving during
Stage IV deployment. The bridge uses the Stage III UID and verifies the exact
WebAdmin UID with `SO_PEERCRED`. The C++ socket and token stay private to that
UID. The browser panel supports account authentication, verified registration,
session management, password changes and a staff-only account-service status.

## Install or upgrade the reviewed checkout

Use a verified Node 24 LTS distribution at `/opt/node24-release`. For an
upgrade, first take the matched backup described below, record the enabled
sites and units, and stop the Stage IV entry point, bridge, and auth units in
an approved maintenance window. Keep the existing Apache site in place.
Perform the installation from the provider console as root.

```sh
apt-get update
apt-get install cmake g++ libssl-dev libgcrypt20-dev libutf8proc-dev libmariadb-dev \
  mariadb-server mariadb-client msmtp apache2 ca-certificates
cmake -S . -B build-stage4 -DCMAKE_BUILD_TYPE=Release
cmake --build build-stage4 --parallel "$(nproc)" --target dc24h dc24h-owner dc24h-account-admin
public_port=7178 # Use 7278 for the approved historical Debian deployment.
./scripts/stage4_install.sh /opt/node24-release build-stage4 "$public_port"
```

Verify the Node tarball with Node.js's signed `SHASUMS256.txt` using a trusted
release key before extraction. The installer checks the site at
`/etc/apache2/sites-available/dc24h-stage4-webadmin.conf`. A matching site
keeps its contents, ownership, mode, and existing enabled link, including on
reinstall. A mismatch in the listener, vhost, HTTPS, Host, or Origin rules
stops installation. Any change to a reviewed site is an explicit operator
action with a backup and `apache2ctl configtest`. A fresh install publishes
the selected site after the other installation steps succeed. Apache activation,
firewall configuration, and the existing certificate or renewal bundle are
managed separately. The VPS must already have `dc24h-stage3` and the working
ADCS service.

## First-run configuration and provisioning

If the bootstrap file has not been prepared, create it from the example
with owner `root:root` and mode `0600`.

```sh
install -d -o root -g root -m 0700 /etc/dc24h
test -e /etc/dc24h/stage4-webadmin-bootstrap.env ||
  install -o root -g root -m 0600 config/stage4-webadmin-bootstrap.env.example \
    /etc/dc24h/stage4-webadmin-bootstrap.env
editor /etc/dc24h/stage4-webadmin-bootstrap.env
```

Set `WEBADMIN_PUBLIC_PORT` to the same explicit `7178` or `7278` selected
above. Keep `WEBADMIN_HOST=server-747585786.dc24h.eu` and
`WEBADMIN_HTTPS=true`. Set `STAGE4_ENVIRONMENT=production` and both `MARIADB_DATABASE` and
`MARIADB_APPROVED_DATABASE` to `dc24h_stage4_production`. Use a dedicated
application user ending in `_production`, such as `dc24h_stage4_production`.
Keep MariaDB on the local Unix socket. An empty `MARIADB_ROOT_PASSWORD` uses
the usual Debian MariaDB Unix-socket root authentication. Fill
`ADMIN_INITIAL_PASSWORD` before the first run, with at least 15 characters.
Set `ADMIN_EMAIL` to the mailbox for enrollment and login codes. Use the account's
existing mailbox when re-running initialization. Choose distinct strong MariaDB
application and SMTP passwords. Set the SMTP hostname, username and provider
port. Use STARTTLS on 587 or set
`SMTP_STARTTLS=false` for implicit TLS on the provider's port. TLS and
certificate verification are mandatory in both modes. The sender is
`admin@dc24h.eu`. Values are literal text after the first equals sign.
Store production input in the protected file and type passwords only on the
console or HTTPS form as appropriate.

```sh
/usr/local/sbin/dc24h-stage4-init
```

The initializer checks the Apache site against the selected Origin before
touching runtime state or MariaDB. It verifies the approved schema name, applies the native C++
migrations followed by versioned Stage IV migrations, checks schema and grants,
and provisions `Administrator` using the C++ password implementation.
An existing unbound Administrator or Owner requires operator review and an
audited migration. Re-running the initializer on the same database preserves
the established Administrator, mailbox and verifier. After creation, the initial
password can be blank when re-provisioning the same database. The running
units receive the generated service credentials and dedicated `msmtp` config.
Production initialization writes the matched `DC24H_STAGE4_ORIGIN` in
`/etc/dc24h/stage4-activation.env`. Keep any older service override consistent
with that value throughout upgrade and rollback.
Erase the `ADMIN_INITIAL_PASSWORD` and `MARIADB_ROOT_PASSWORD` values from the
bootstrap file once initialization succeeds. If removing the entire bootstrap
file, keep any recurring configuration needed for later provisioning in the
owner's approved password store. Editing or unlinking the file does not erase
old blocks, snapshots, editor history or backups.

For a disposable installation use the example's `test` environment and
explicit `dc24h_stage4_test` schema. Its generated material is under
`/etc/dc24h/stage4-test`. The production systemd unit rejects that database.

## Start, SMTP qualification and HTTPS

```sh
origin="https://server-747585786.dc24h.eu:$public_port"
grep -Fx "DC24H_STAGE4_ORIGIN=$origin" /etc/dc24h/stage4-activation.env
systemctl start dc24h-stage4-webadmin.service
systemctl status dc24h-stage4-{auth,bridge,webadmin}.service --no-pager
journalctl -u dc24h-stage4-webadmin.service --no-pager -n 30
ss -ltn "( sport = :4278 or sport = :$public_port or sport = :1512 or sport = :1511 or sport = :3306 )"
```

Before opening ingress, test mail from the dedicated service account. Exit
status zero confirms relay acceptance. Confirm the probe message in the
configured Administrator mailbox.

```sh
admin_email='admin@dc24h.eu' # Use the configured Administrator mailbox.
printf 'From: admin@dc24h.eu\nTo: %s\nSubject: Stage IV SMTP probe\n\nDelivery check.\n' "$admin_email" |
  runuser -u dc24h-webadmin -- /usr/bin/msmtp --file=/var/lib/dc24h-stage4/.msmtprc -t
```

Check that the hostname resolves to the VPS and inspect the existing HTTPS
sites for access logging of cookie headers and upstream cookie forwarding.
Cookies are shared across HTTPS ports on this hostname. Verify the existing
certificate and key paths in the reviewed vhost. Keep the shared bundle paths
if the approved TLS renewal procedure has already configured them. An older
site needs a reviewed Apache allowlist update before exposing the TLS renewal
routes in WebAdmin. [TLS renewal setup](TLS_AUTO_RENEWAL.md)

```sh
a2enmod ssl headers rewrite proxy proxy_http
a2ensite dc24h-stage4-webadmin
apache2ctl configtest && systemctl reload apache2
systemctl enable dc24h-stage4-webadmin.service
```

Use only the approved public port in the provider firewall. Keep TCP 4278
and 3306 local and keep TCP 1511 closed. From an outside network run
`curl --fail --silent --show-error "$origin/"`
with certificate verification enabled, and verify the ADCS hub on 1512
with a client that validates its TLS certificate.

Sign in with username `Administrator` and the initial password. The first
successful password proof sends an eight-digit enrollment code to the
configured Administrator mailbox. Enter the code on the same page to activate
the account.
Each later login sends a fresh code to the stored address. New users use
**Załóż konto**, enter a username, password and email, then verify a code
before the application creates their `registered` account. They sign in
separately with password and a fresh code. Codes expire after five minutes
or five incorrect guesses. A failed or delayed registration can be started
again after the pending reservation expires, subject to attempt limits.
Password changes invalidate existing sessions.

```sh
systemctl restart dc24h-stage4-webadmin.service
systemctl status dc24h-stage4-{auth,bridge,webadmin}.service --no-pager
```

## Backup, restore and rollback

Store backups encrypted and limit access to the recovery operator. Include
MariaDB data and procedures, the private Stage IV runtime files, the exact
deployed binaries, the reviewed Apache site, its enabled state, and the
systemd units and drop-ins. Record the public port and the matched activation
file. Keep the database backup and runtime credential archive together as a
matched pair.

```sh
umask 077
backup="/root/stage4-production-$(date -u +%Y%m%dT%H%M%SZ).sql"
mariadb-dump --protocol=socket --single-transaction --routines \
  --databases dc24h_stage4_production >"$backup"
```

Rehearse restoration on a separate disposable MariaDB server before
production recovery. A recovery operator must identify the incident and
account out of band and record the decision in the protected incident log.
Do not mark an address verified by editing the database. If mailbox access is
lost, keep the affected account suspended until mailbox control is recovered
or an audited ownership transfer is completed through the existing C++
maintenance tooling. Use the operator's backup process or recover the mailbox
first. The public panel offers no email-only recovery path. After restoring a
matched backup and checking migrations, invalidate restored capabilities by
clearing `stage4_sessions` and `stage4_challenges` before enabling ingress.
The latter also clears pending registration reservations.

To withdraw the Stage IV release, disable only its vhost and units:

```sh
a2dissite dc24h-stage4-webadmin
apache2ctl configtest && systemctl reload apache2
systemctl disable --now dc24h-stage4-webadmin.service
systemctl stop dc24h-stage4-bridge.service dc24h-stage4-auth.service
systemctl is-active dc24h-stage3-adcs.service
```

For rollback, restore the previous matched Stage IV binaries, Apache site,
activation file, and systemd definitions from the backup. Verify the
`Listen` and `VirtualHost` port, Host and Origin rules, and the matched Node
Origin. Verify `apache2ctl configtest` before restoring the recorded enabled
state. Recheck the HTTPS page, password and OTP login, roles, logout, and
ADCS afterward. Restore the database only when needed under a separate recovery
decision that accounts for new registrations. Keep the live certificate and
Stage III service in place.

## Acceptance

The hosted test workflow runs `scripts/stage4_test_install.sh` on a disposable
runner for vhost preservation, reinstall, failure, and rollback. It runs
`scripts/stage4_test_database.sh 7178` with a separate local database. The test exercises native SQL migrations, owner initialization,
the C++ proof path, session and challenge transactions, signup, SMTP rejection
with a mock sender, backup/restore and restart of the disposable MariaDB.
GitHub qualification covers hosted Ubuntu 22.04, 24.04 and 26.04 on the
exact implementation SHA tested. Production activation requires its own
Owner decision. The OWNER reported a successful live
Administrator email-OTP login and working password change on November 27,
2026. Record the corresponding deployed SHA, actual mailbox receipt and
service state separately from independent external TLS, denied-route,
role/session, restart and rollback acceptance. New documentation commits
require their own exact-head CI verification; green CI or operator evidence
alone is not `OWNER STAGE IV ACCEPTANCE` or `OWNER MERGE GO`.
