#!/bin/bash
# Disposable MariaDB, C++ API and loopback HTTP. No external SMTP or host ports.
set -euo pipefail
umask 077
test "$(id -u)" = 0
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
scratch=/run/stage4-fixture-$(openssl rand -hex 8)
bootstrap=/etc/dc24h/stage4-webadmin-bootstrap-ci.env
state=/etc/dc24h/stage4-test
link=/etc/dc24h/stage4-webadmin-bootstrap-ci-link.env
[ ! -e "$bootstrap" ] && [ ! -e "$state" ] && [ ! -L "$link" ] && [ ! -e "$link" ] || {
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
    rm -rf -- "$scratch" "$state"
    rm -f -- "$bootstrap" "$link"
}
trap cleanup EXIT
socket=$scratch/mysqld.sock
# The socket is reachable only over the local file system, on an isolated datadir.
chmod 0775 "$scratch"
chgrp mysql "$scratch"
mariadb-install-db --no-defaults --user=mysql --datadir="$scratch/db" --skip-test-db >/dev/null 2>&1
start_database() {
    mariadbd --no-defaults --user=mysql --datadir="$scratch/db" --skip-networking \
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
sed -e "s|^MARIADB_SOCKET=.*|MARIADB_SOCKET=$socket|" \
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
dc24h-stage4-init --config "$bootstrap"
dc24h-stage4-init --config "$bootstrap"
test "$(sql -N dc24h_stage4_test -e "SELECT COUNT(*) FROM dc24h_accounts WHERE role='owner'")" = 1
test "$(sql -N dc24h_stage4_test -e "SELECT COUNT(*) FROM dc24h_account_credentials WHERE verifier LIKE '%.invalid' OR OCTET_LENGTH(verifier)<100")" = 0
sed "s|/run/dc24h-stage4-auth/api.sock|$scratch/api/auth.sock|" "$state/auth.ini" > "$scratch/api/auth.ini"
/opt/dc24h/bin/dc24h-twilight-account-admin --config "$scratch/api/auth.ini" --stage4-serving \
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
    STAGE4_FIXTURE="$scratch" node --import tsx --test tests/stage4-mariadb.integration.ts
)
mariadb-dump --no-defaults -S "$socket" -u root --single-transaction --routines --databases dc24h_stage4_test \
    > "$scratch/backup.sql"
kill "$db_pid"
wait "$db_pid"
db_pid=
start_database
test "$(sql -N dc24h_stage4_test -e 'SELECT COUNT(*) FROM stage4_emails WHERE verified_at IS NOT NULL')" = 2
# The entire server and schema are disposable. Installers never execute this DROP.
sql -e 'DROP DATABASE dc24h_stage4_test'
sql < "$scratch/backup.sql"
test "$(sql -N dc24h_stage4_test -e 'SELECT COUNT(*) FROM stage4_emails WHERE verified_at IS NOT NULL')" = 2
test "$(sql -N dc24h_stage4_test -e 'SELECT COUNT(*) FROM stage4_schema_migrations')" = 2
sql dc24h_stage4_test -e 'DELETE FROM stage4_sessions; DELETE FROM stage4_challenges'
echo "PASS MariaDB restart, logical backup, restore and session invalidation (disposable)"
