# Stage IV WebAdmin operations

The public endpoint is `https://server-747585786.dc24h.eu:7278`. Apache terminates TLS
and forwards the approved API paths to Node at `127.0.0.1:4278`. The Stage III
guest hub remains on ADCS TCP 1512. ADC TCP 1511 stays disabled.

Node runs as `dc24h-webadmin` and uses a dedicated local MariaDB account. The
C++ account service uses the existing account and password-verifier tables in
the selected database. It opens a private Unix socket and runs without hub
listeners, so the existing Stage III service can continue serving during
Stage IV deployment. The bridge uses the Stage III UID and verifies the exact
WebAdmin UID with `SO_PEERCRED`. The C++ socket and token stay private to that
UID. The browser panel supports account authentication, verified registration,
session management, password changes and a staff-only account-service status.

## Install the reviewed checkout on Debian 13

From the provider console as root, with the Stage IV feature branch checked
out and a verified Node 24 LTS distribution extracted at `/opt/node24-release`:

```sh
apt-get update
apt-get install cmake g++ libssl-dev libgcrypt20-dev libutf8proc-dev libmariadb-dev \
  mariadb-server mariadb-client msmtp apache2 ca-certificates
cmake -S . -B build-stage4 -DCMAKE_BUILD_TYPE=Release
cmake --build build-stage4 --parallel "$(nproc)" --target dc24h dc24h-owner dc24h-account-admin
./scripts/stage4_install.sh /opt/node24-release build-stage4
```

Verify the Node tarball with Node.js's signed `SHASUMS256.txt` using a trusted
release key before extraction. The installer installs the three units, the
Apache site definition and the compiled application. Unit and site activation
are separate commands below. The VPS must already have `dc24h-stage3` and
the working ADCS service.

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

Set `STAGE4_ENVIRONMENT=production` and both `MARIADB_DATABASE` and
`MARIADB_APPROVED_DATABASE` to `dc24h_stage4_production`. Use a dedicated
application user ending in `_production`, such as `dc24h_stage4_production`.
Keep MariaDB on the local Unix socket. An empty `MARIADB_ROOT_PASSWORD` uses
the usual Debian MariaDB Unix-socket root authentication. Fill
`ADMIN_INITIAL_PASSWORD` before the first run, with at least 15 characters,
and choose distinct strong MariaDB application and SMTP passwords. Set the SMTP
hostname, username and provider port. Use STARTTLS on 587 or set
`SMTP_STARTTLS=false` for implicit TLS on the provider's port. TLS and
certificate verification are mandatory in both modes. The sender is
`admin@dc24h.eu`. Values are literal text after the first equals sign.
Store production input in the protected file and type passwords only on the
console or HTTPS form as appropriate.

```sh
/usr/local/sbin/dc24h-stage4-init
```

The initializer verifies the approved schema name, applies the native C++
migrations followed by versioned Stage IV migrations, checks schema and grants,
and provisions `Administrator` using the C++ password implementation.
An existing unbound Administrator or Owner requires operator review and an
audited migration. Re-running the initializer on the same database preserves
the established Administrator and verifier. After creation, the initial
password can be blank when re-provisioning the same database. The running
units receive the generated service credentials and dedicated `msmtp` config.
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
systemctl start dc24h-twilight-stage4-webadmin.service
systemctl status dc24h-twilight-stage4-{auth,bridge,webadmin}.service --no-pager
journalctl -u dc24h-twilight-stage4-webadmin.service --no-pager -n 30
ss -ltn '( sport = :4278 or sport = :7278 or sport = :1512 or sport = :1511 or sport = :3306 )'
```

Before opening ingress, test mail from the dedicated service account. Exit
status zero confirms relay acceptance. Confirm the message separately in
`admin@dc24h.eu`. This probe contains no login code.

```sh
printf 'From: admin@dc24h.eu\nTo: admin@dc24h.eu\nSubject: Stage IV SMTP probe\n\nDelivery check.\n' |
  runuser -u dc24h-webadmin -- /usr/bin/msmtp --file=/var/lib/dc24h-stage4/.msmtprc -t
```

Check that the hostname resolves to the VPS and inspect the existing HTTPS
sites for access logging of cookie headers and upstream cookie forwarding.
Cookies are shared across HTTPS ports on this hostname. The certificate
paths live under `/etc/letsencrypt/live/server-747585786.dc24h.eu/`.

```sh
a2enmod ssl headers rewrite proxy proxy_http
a2ensite dc24h-stage4-webadmin
apache2ctl configtest && systemctl reload apache2
systemctl enable dc24h-twilight-stage4-webadmin.service
```

Allow TCP 7278 in the provider firewall when it filters ingress. Keep TCP
4278 and 3306 local and keep TCP 1511 closed. From an outside network run
`curl --fail --silent --show-error https://server-747585786.dc24h.eu:7278/`
with certificate verification enabled, and verify the ADCS hub on 1512
with a client that validates its TLS certificate.

Sign in with username `Administrator` and the initial password. The first
successful password proof triggers an eight-digit enrollment code to
`admin@dc24h.eu`. Enter that code on the same page to activate the account.
Each later login sends a fresh code to the stored address. New users use
**Załóż konto**, enter a username, password and email, then verify a code
before the application creates their `registered` account. They sign in
separately with password and a fresh code. Codes expire after five minutes
or five incorrect guesses. A failed or delayed registration can be started
again after the pending reservation expires, subject to attempt limits.
Password changes invalidate existing sessions.

```sh
systemctl restart dc24h-twilight-stage4-webadmin.service
systemctl status dc24h-twilight-stage4-{auth,bridge,webadmin}.service --no-pager
```

## Backup, restore and rollback

Store backups encrypted and limit access to the recovery operator. Include
MariaDB data and procedures, the private Stage IV runtime files, the exact
deployed binaries and the Apache/systemd configurations. Keep the database
backup and runtime credential archive together as a matched pair.

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
systemctl disable --now dc24h-twilight-stage4-webadmin.service
systemctl stop dc24h-twilight-stage4-bridge.service dc24h-twilight-stage4-auth.service
systemctl is-active dc24h-stage3-adcs.service
```

Restore only the reviewed Stage IV artifacts and matching database if needed.
Avoid restoring an old database over new registrations without an approved
recovery decision. Leave the live certificate and Stage III service in place.

## Acceptance

Run `scripts/stage4_test_database.sh` against its disposable fixture after
installation. The test exercises native SQL migrations, owner initialization,
the C++ proof path, session and challenge transactions, signup, SMTP rejection
with a mock sender, backup/restore and restart of the disposable MariaDB.
GitHub qualification runs on hosted Ubuntu 22.04 only. Record
production systemd startup/restart, actual mailbox receipt, successful Owner
enrollment and external HTTPS separately with result and deployed commit SHA.
