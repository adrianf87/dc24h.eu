#!/bin/bash
# Disposable MariaDB, C++ API and loopback HTTP. No external SMTP or host ports.
set -euo pipefail
umask 077
test "$(id -u)" = 0
public_port=${1:-}
case "$public_port" in
    7178) other_port=7278 ;;
    7278) other_port=7178 ;;
    *) echo "Usage: stage4_test_database.sh PUBLIC_PORT (7178 or 7278)" >&2; exit 2 ;;
esac
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
fixture_nonce=$(openssl rand -hex 8)
scratch=/run/stage4-fixture-$fixture_nonce
mysql_exec=()
mysql_user_arg=(--user=mysql)
# Ubuntu 26.04 ships MariaDB 11.8 with an enforcing AppArmor profile for
# /var/lib/mariadb. Keep this disposable database *inside* that confined
# directory; never disable or change the host AppArmor policy. Only hosted CI
# may use this fixture exception, never a production or self-hosted runner.
if [ "${GITHUB_ACTIONS:-}" = "true" ] && [ "${RUNNER_ENVIRONMENT:-}" = "github-hosted" ]; then
    . /etc/os-release
    if [ "$ID" = ubuntu ] && [ "$VERSION_ID" = 26.04 ]; then
        [ -d /var/lib/mariadb ] && [ ! -L /var/lib/mariadb ] || {
            echo "Ubuntu 26.04 MariaDB confined directory missing or unsafe" >&2; exit 1;
        }
        scratch=/var/lib/mariadb/stage4-fixture-$fixture_nonce
        # Run the bootstrap and daemon as mysql from the outset. MariaDB
        # 11.8's Ubuntu AppArmor profile does not grant in-daemon setuid,
        # setgid or dac_override needed when a root daemon drops privileges.
        # setpriv changes credentials before exec, outside that profile.
        mysql_exec=(setpriv --reuid=mysql --regid=mysql --init-groups)
        mysql_user_arg=()
    fi
fi
bootstrap=/etc/dc24h/stage4-webadmin-bootstrap-ci.env
state=/etc/dc24h/stage4-test
link=/etc/dc24h/stage4-webadmin-bootstrap-ci-link.env
invalid=/etc/dc24h/stage4-webadmin-bootstrap-ci-invalid.env
[ ! -e "$bootstrap" ] && [ ! -e "$state" ] && [ ! -L "$link" ] && [ ! -e "$link" ] &&
[ ! -L "$invalid" ] && [ ! -e "$invalid" ] || {
    echo "Stage IV disposable configuration already exists" >&2; exit 1;
}
install -d -m 0755 "$scratch"
install -d -m 0700 -o mysql -g mysql "$scratch/db"
install -d -m 0700 /etc/dc24h
install -d -m 0700 "$scratch/api"
native_pid=
db_pid=
cleanup() {
    if [ -n "$native_pid" ]; then kill "$native_pid" 2>/dev/null || :; wait "$native_pid" 2>/dev/null || :; fi
    if [ -n "$db_pid" ]; then kill "$db_pid" 2>/dev/null || :; wait "$db_pid" 2>/dev/null || :; fi
    # Ubuntu 26.04 uses the packaged, AppArmor-approved runtime socket path.
    # Only the random fixture socket is removed; never touch a system socket.
    if [ "${socket:-}" = "/run/mysqld/stage4-fixture-$fixture_nonce.sock" ]; then
        rm -f -- "$socket"
    fi
    rm -rf -- "$scratch" "$state"
    rm -f -- "$bootstrap" "$link" "$invalid"
}
trap cleanup EXIT
socket=$scratch/mysqld.sock
if [ "${mysql_exec[0]:-}" = setpriv ]; then
    # Bootstrap and data remain under the Ubuntu 26.04 MariaDB confinement,
    # while the local socket respects both AppArmor and Stage IV's strict
    # /run/.../*.sock bootstrap schema. Never change the production validator.
    [ ! -L /run/mysqld ] || { echo "Unsafe MariaDB runtime directory" >&2; exit 1; }
    install -d -m 0755 -o mysql -g mysql /run/mysqld
    socket=/run/mysqld/stage4-fixture-$fixture_nonce.sock
fi
# The socket is reachable only over the local file system, on an isolated datadir.
chmod 0775 "$scratch"
chgrp mysql "$scratch"
if ! "${mysql_exec[@]}" mariadb-install-db --no-defaults "${mysql_user_arg[@]}" --datadir="$scratch/db" \
        --skip-test-db >"$scratch/db-bootstrap.log" 2>&1; then
    echo "Disposable MariaDB initialization failed on the selected Ubuntu image" >&2
    # No credentials exist yet. Report a bounded excerpt for CI diagnosis.
    sed -n '1,20p' "$scratch/db-bootstrap.log" >&2
    tail -n 25 "$scratch/db-bootstrap.log" >&2
    exit 1
fi
start_database() {
    "${mysql_exec[@]}" mariadbd --no-defaults "${mysql_user_arg[@]}" --datadir="$scratch/db" --skip-networking \
        --socket="$socket" --pid-file="$scratch/db/mariadb.pid" --log-error="$scratch/db/mariadb.log" &
    db_pid=$!
    for attempt in {1..50}; do
        if mariadb-admin --no-defaults -S "$socket" -u root ping >/dev/null 2>&1; then return; fi
        sleep .2
    done
    echo "Disposable MariaDB startup failed" >&2
    return 1
}
sql() { mariadb --no-defaults -S "$socket" -u root "$@"; }
start_database
admin_password=$(openssl rand -hex 24)
admin_email=owner-fixture@example.test
sed -e "s|^MARIADB_SOCKET=.*|MARIADB_SOCKET=$socket|" \
    -e "s|^WEBADMIN_PUBLIC_PORT=.*|WEBADMIN_PUBLIC_PORT=$public_port|" \
    -e "s|^ADMIN_EMAIL=.*|ADMIN_EMAIL=${admin_email^^}|" \
    -e "s|^ADMIN_INITIAL_PASSWORD=$|ADMIN_INITIAL_PASSWORD=$admin_password|" \
    -e "s|^MARIADB_APP_PASSWORD=$|MARIADB_APP_PASSWORD=$(openssl rand -hex 24)|" \
    -e 's|^SMTP_HOST=$|SMTP_HOST=localhost|' \
    -e 's|^SMTP_USERNAME=$|SMTP_USERNAME=fixture@dc24h.eu|' \
    -e "s|^SMTP_PASSWORD=$|SMTP_PASSWORD=$(openssl rand -hex 24)|" \
    "$repo/config/stage4-webadmin-bootstrap.env.example" > "$bootstrap"
ln -s "$bootstrap" /etc/dc24h/stage4-webadmin-bootstrap-ci-link.env
if dc24h-stage4-init --config /etc/dc24h/stage4-webadmin-bootstrap-ci-link.env >/dev/null 2>&1; then
    unlink /etc/dc24h/stage4-webadmin-bootstrap-ci-link.env
    echo "Bootstrap accepted a symlink" >&2; exit 1
fi
unlink /etc/dc24h/stage4-webadmin-bootstrap-ci-link.env
chmod 0644 "$bootstrap"
if dc24h-stage4-init --config "$bootstrap" >/dev/null 2>&1; then
    echo "Bootstrap accepted unsafe permissions" >&2; exit 1
fi
chmod 0600 "$bootstrap"
reject_bootstrap() {
    sed "$1" "$bootstrap" > "$invalid"
    if dc24h-stage4-init --config "$invalid" >"$scratch/bootstrap-refusal.log" 2>&1; then
        echo "Bootstrap accepted invalid configuration" >&2
        exit 1
    fi
    grep -Fq "$2" "$scratch/bootstrap-refusal.log"
    test ! -e "$state"
}
reject_bootstrap "s/^WEBADMIN_PUBLIC_PORT=.*/WEBADMIN_PUBLIC_PORT=$other_port/" \
    'Stage IV Apache site does not match the selected HTTPS Origin'
reject_bootstrap 's/^WEBADMIN_PUBLIC_PORT=.*/WEBADMIN_PUBLIC_PORT=443/' 'Invalid bootstrap field WEBADMIN_PUBLIC_PORT'
reject_bootstrap 's/^WEBADMIN_HOST=.*/WEBADMIN_HOST=foreign.invalid/' 'Invalid bootstrap field WEBADMIN_HOST'
reject_bootstrap 's/^WEBADMIN_HTTPS=.*/WEBADMIN_HTTPS=false/' 'Invalid bootstrap field WEBADMIN_HTTPS'
reject_bootstrap 's/^ADMIN_EMAIL=.*/ADMIN_EMAIL=invalid address/' 'Invalid bootstrap field ADMIN_EMAIL'
echo 'PASS invalid Apache endpoint and Administrator mailbox refused before provisioning'
dc24h-stage4-init --config "$bootstrap"
dc24h-stage4-init --config "$bootstrap"
sed 's/^ADMIN_EMAIL=.*/ADMIN_EMAIL=other-fixture@example.test/' "$bootstrap" > "$invalid"
if dc24h-stage4-init --config "$invalid" >"$scratch/bootstrap-refusal.log" 2>&1; then
    echo "Bootstrap accepted a change to an existing Administrator mailbox" >&2; exit 1
fi
grep -Fq 'Existing Administrator requires an audited operator migration' "$scratch/bootstrap-refusal.log"
test "$(sql -N dc24h_stage4_test -e 'SELECT email FROM stage4_emails WHERE initial_owner=1')" = "$admin_email"
test "$(sql -N dc24h_stage4_test -e "SELECT COUNT(*) FROM dc24h_accounts WHERE role='owner'")" = 1
test "$(sql -N dc24h_stage4_test -e "SELECT COUNT(*) FROM dc24h_account_credentials WHERE verifier LIKE '%.invalid' OR OCTET_LENGTH(verifier)<100")" = 0
sed "s|/run/dc24h-stage4-auth/api.sock|$scratch/api/auth.sock|" "$state/auth.ini" > "$scratch/api/auth.ini"
/opt/dc24h/bin/dc24h-account-admin --config "$scratch/api/auth.ini" --stage4-serving \
    >"$scratch/api/native.log" 2>&1 &
native_pid=$!
for attempt in {1..50}; do
    test ! -S "$scratch/api/auth.sock" || break
    if ! kill -0 "$native_pid" 2>/dev/null; then cat "$scratch/api/native.log" >&2; exit 1; fi
    sleep .2
done
test -S "$scratch/api/auth.sock"
printf '%s' "$admin_password" > "$scratch/api/fixture.password"
(
    cd "$repo/webadmin"
    STAGE4_FIXTURE="$scratch" STAGE4_FIXTURE_PORT="$public_port" STAGE4_FIXTURE_ADMIN_EMAIL="$admin_email" \
        node --import tsx --test tests/stage4-mariadb.integration.ts
)
mariadb-dump --no-defaults -S "$socket" -u root --single-transaction --routines --databases dc24h_stage4_test \
    > "$scratch/backup.sql"
cp -a -- "$state" "$scratch/state-backup"
kill "$db_pid"
wait "$db_pid"
db_pid=
start_database
test "$(sql -N dc24h_stage4_test -e 'SELECT COUNT(*) FROM stage4_emails WHERE verified_at IS NOT NULL')" = 2
# The entire server and schema are disposable. Installers never execute this DROP.
sql dc24h_stage4_test -e "UPDATE stage4_schema_migrations SET checksum=REPEAT('0',64) WHERE version=1"
if dc24h-stage4-init --config "$bootstrap" >"$scratch/migration-check.log" 2>&1; then
    echo "Bootstrap accepted a changed migration checksum" >&2; exit 1
fi
grep -Fq 'Stage IV schema failed' "$scratch/migration-check.log"
test "$(sql -N dc24h_stage4_test -e 'SELECT COUNT(*) FROM stage4_emails WHERE verified_at IS NOT NULL')" = 2
echo 'PASS migration checksum mismatch refused without removing account data (disposable)'
sql -e 'DROP DATABASE dc24h_stage4_test'
sql < "$scratch/backup.sql"
mv -- "$state" "$scratch/state-replaced"
cp -a -- "$scratch/state-backup" "$state"
diff -qr -- "$scratch/state-backup" "$state"
test "$(sql -N dc24h_stage4_test -e 'SELECT COUNT(*) FROM stage4_emails WHERE verified_at IS NOT NULL')" = 2
test "$(sql -N dc24h_stage4_test -e 'SELECT COUNT(*) FROM stage4_schema_migrations')" = 3
dc24h-stage4-init --config "$bootstrap"
sql dc24h_stage4_test -e 'DELETE FROM stage4_sessions; DELETE FROM stage4_challenges'
test "$(sql -N dc24h_stage4_test -e 'SELECT (SELECT COUNT(*) FROM stage4_sessions) + (SELECT COUNT(*) FROM stage4_challenges)')" = 0
echo "PASS MariaDB restart, matched SQL/configuration restore and session invalidation (disposable)"
