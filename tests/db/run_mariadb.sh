#!/usr/bin/env bash
set -euo pipefail
umask 077

hub=$1
suite=$2
mariadbd=$3
install_db=$4
mariadb=$5
openssl=$6
work=$(mktemp -d /tmp/dc24h-e5.XXXXXXXX)
db_pid=
hub_pid=

cleanup() {
    local status=$?
    if [[ -n $hub_pid ]]; then
        kill "$hub_pid" 2>/dev/null || true
        wait "$hub_pid" || true
    fi
    if [[ -n $db_pid ]]; then
        kill "$db_pid" 2>/dev/null || true
        wait "$db_pid" || true
    fi
    if ((status != 0)); then
        for log in install.log server.log hub.log; do
            if [[ -f "$work/$log" ]]; then
                cat "$work/$log"
            fi
        done
    fi
    rm -rf -- "$work"
}
trap cleanup EXIT

"$install_db" --no-defaults --user="$(id -un)" --datadir="$work/data" \
    --auth-root-authentication-method=normal --skip-test-db >"$work/install.log" 2>&1
"$mariadbd" --no-defaults --user="$(id -un)" --datadir="$work/data" --tmpdir="$work" \
    --skip-networking --socket="$work/mdb.sock" --pid-file="$work/mdb.pid" \
    --log-error="$work/server.log" --innodb-buffer-pool-size=32M --skip-log-bin &
db_pid=$!
sql=("$mariadb" --no-defaults --protocol=socket --user=root --socket="$work/mdb.sock")
for ((attempt = 0; attempt < 100; ++attempt)); do
    if "${sql[@]}" -e 'SELECT 1' >/dev/null 2>&1; then
        break
    fi
    kill -0 "$db_pid"
    sleep 0.1
done

export DC24H_TEST_DB_DIR="$work"
export DC24H_DB_SOCKET="$work/mdb.sock"
export DC24H_DB_NAME=dc24h_e5_test
"${sql[@]}" <<'SQL'
CREATE DATABASE dc24h_e5_test CHARACTER SET utf8mb4;
CREATE DATABASE dc24h_e5_upgrade CHARACTER SET utf8mb4;
SQL
for role in migrator admin reader; do
    read -r password < /proc/sys/kernel/random/uuid
    printf '%s\n' "$password" >"$work/$role.pw"
    "${sql[@]}" <<SQL
CREATE USER 'e5_$role'@'localhost' IDENTIFIED BY '$password';
SQL
done
unset password

"${sql[@]}" <<'SQL'
GRANT CREATE, SELECT, INSERT ON dc24h_e5_test.* TO 'e5_migrator'@'localhost';
GRANT CREATE, SELECT, INSERT ON dc24h_e5_upgrade.* TO 'e5_migrator'@'localhost';
SQL
export DC24H_DB_USER=e5_migrator
export DC24H_DB_PASSWORD_FILE="$work/migrator.pw"
[[ "$("$hub" --db migrate)" == "MariaDB schema 3 ready" ]]
[[ "$("$hub" --db migrate)" == "MariaDB schema 3 ready" ]]
[[ "$("$hub" --db status)" == "MariaDB schema 3 / 3" ]]

"${sql[@]}" <<'SQL'
REVOKE INSERT ON dc24h_e5_test.* FROM 'e5_migrator'@'localhost';
GRANT INSERT ON dc24h_e5_test.schema_migrations TO 'e5_migrator'@'localhost';
GRANT SELECT ON dc24h_e5_test.schema_migrations TO 'e5_reader'@'localhost', 'e5_admin'@'localhost';
GRANT SELECT ON dc24h_e5_test.hub_settings TO 'e5_reader'@'localhost';
GRANT SELECT ON dc24h_e5_test.bans TO 'e5_reader'@'localhost';
GRANT SELECT, INSERT, UPDATE ON dc24h_e5_test.hub_settings TO 'e5_admin'@'localhost';
GRANT SELECT, INSERT, DELETE ON dc24h_e5_test.bans TO 'e5_admin'@'localhost';
GRANT SELECT, INSERT ON dc24h_e5_test.audit_events TO 'e5_admin'@'localhost';
SQL
export DC24H_DB_USER=e5_admin
export DC24H_DB_PASSWORD_FILE="$work/admin.pw"
"$hub" --db settings set hub-name 'Stored hub' --actor maintainer
"$hub" --db settings set hub-description 'Stored description'
"$hub" --db settings list | grep -Fx $'hub-name\tStored hub'
"$hub" --db bans add ip 192.0.2.25 permanent 'Fixture ban'
"$hub" --db bans list | grep -F $'ip\t192.0.2.25\tFixture ban'
"$hub" --db bans remove 1
"$hub" --db audit list | grep -F $'ban.removed\t'

"$openssl" req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes \
    -subj /CN=localhost -days 1 -keyout "$work/key.pem" -out "$work/cert.pem" \
    >"$work/openssl.log" 2>&1
export DC24H_DB_USER=e5_reader
export DC24H_DB_PASSWORD_FILE="$work/reader.pw"

expect_hub_name() {
    local expected=$1
    local description=$2
    shift 2
    "$hub" --serve --mariadb --enable-adc --adcs-port 0 --adc-port 0 --threads 1 \
        --tls-cert "$work/cert.pem" --tls-key "$work/key.pem" "$@" \
        >"$work/listen.log" 2>"$work/hub.log" &
    hub_pid=$!
    local port info line attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        port=$(awk '/Listening for ADC on/ {print $NF}' "$work/listen.log")
        if [[ -n $port ]]; then
            break
        fi
        kill -0 "$hub_pid"
        sleep 0.05
    done
    (
        exec 3<>"/dev/tcp/127.0.0.1/$port"
        printf 'HSUP ADBASE ADTIGR\n' >&3
        IFS= read -r -t 3 line <&3
        IFS= read -r -t 3 line <&3
        IFS= read -r -t 3 info <&3
        [[ $info == "IINF CT32 NI$expected VE"*"$description" ]]
        if [[ -z $description ]]; then
            [[ $info != *' DE'* ]]
        fi
    )
    kill -TERM "$hub_pid"
    wait "$hub_pid"
    hub_pid=
}

expect_hub_name 'Stored\shub' ' DEStored\sdescription'
expect_hub_name 'Override\shub' ' DEStored\sdescription' --hub-name 'Override hub'
DC24H_DB_SOCKET="$work/missing.sock" expect_hub_name 'dc24h.eu' ''
chmod 0644 "$work/reader.pw"
if "$hub" --db status >"$work/refused.log" 2>&1; then
    echo "Accepted a readable password file" >&2
    exit 1
fi
grep -F 'must be a private regular file' "$work/refused.log"
chmod 0600 "$work/reader.pw"

"$suite"
