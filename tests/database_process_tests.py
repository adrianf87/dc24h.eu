"""Disposable MariaDB only: never contacts the host's database or reads my.cnf."""
import hashlib
import os
from pathlib import Path
import pwd
import selectors
import signal
import subprocess
import sys
import tempfile
import time

from api_support import enable_api, fetch_status

MIGRATION_TEST = None
if '--migration-test' in sys.argv:
    index = sys.argv.index('--migration-test')
    MIGRATION_TEST = os.path.abspath(sys.argv[index + 1])
    del sys.argv[index:index + 2]
BINARY, TEST = map(os.path.abspath, sys.argv[1:3])
CORE_TEST = os.path.abspath(sys.argv[3]) if len(sys.argv) >= 4 else None
NCDC = os.path.abspath(sys.argv[4]) if len(sys.argv) >= 6 else None
MSG_CODEC = os.path.abspath(sys.argv[5]) if len(sys.argv) >= 6 else None


def run(args, **kwargs):
    return subprocess.run(args, capture_output=True, text=True, timeout=30, **kwargs)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    with tempfile.TemporaryDirectory(prefix='dc24h-db-') as temp:
        folder = Path(temp)
        data = folder / 'data'
        db_socket = folder / 'db.sock'
        # Bootstrap SQL also creates temporary system tables. Keep those files
        # private when separate CTest instances initialize MariaDB concurrently.
        sql_tmp = folder / 'sql-tmp'
        sql_tmp.mkdir(mode=0o700)
        init = run(['mariadb-install-db', '--no-defaults', '--datadir=' + str(data),
                    '--tmpdir=' + str(sql_tmp),
                    '--auth-root-authentication-method=normal', '--skip-test-db',
                    '--user=' + pwd.getpwuid(os.geteuid()).pw_name])
        require(init.returncode == 0, 'Disposable database initialization failed: ' + init.stderr)
        daemon_log = folder / 'daemon.log'
        base = ['mariadbd', '--no-defaults', '--user=' + pwd.getpwuid(os.geteuid()).pw_name,
                '--datadir=' + str(data), '--socket=' + str(db_socket),
                '--tmpdir=' + str(sql_tmp),
                '--pid-file=' + str(folder / 'db.pid'), '--log-error=' + str(daemon_log),
                '--skip-networking', '--skip-name-resolve',
                '--innodb-buffer-pool-size=32M', '--innodb-log-file-size=8M']
        daemon = None
        hub = None
        def sql(text):
            result = run(['mariadb', '--no-defaults', '--skip-ssl', '--protocol=socket', '--socket=' + str(db_socket),
                          '--user=root', '--batch', '--skip-column-names'], input=text)
            require(result.returncode == 0, 'Test SQL failed: ' + result.stderr)
            return result.stdout.strip()
        def start(extra):
            process = subprocess.Popen(base + extra, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise AssertionError('Disposable MariaDB exited: ' + daemon_log.read_text()[-6000:])
                if db_socket.exists():
                    check = run(['mariadb-admin', '--no-defaults', '--skip-ssl', '--protocol=socket',
                                 '--socket=' + str(db_socket), '--user=root', 'ping'])
                    if check.returncode == 0:
                        return process
                time.sleep(0.05)
            process.kill(); process.wait()
            raise AssertionError('Disposable MariaDB startup timed out: ' + daemon_log.read_text()[-6000:] + '\n' + (check.stderr if 'check' in locals() else 'no socket'))
        def config(extra='', password='password', db_user='dc24h'):
            path = folder / 'hub.ini'
            path.write_text('[network]\nlisten=127.0.0.1:0\n[database]\nenabled=on\nsocket=' +
                            str(db_socket) + '\nuser=' + db_user + '\npassword_file=' + password +
                            '\ntimeout=2\n' + extra, encoding='utf-8')
            return path
        def cli(path, *args):
            return run([BINARY, '--config', str(path), *args])
        def no_secrets(result):
            require(secret not in result.stdout + result.stderr, 'Password leaked')
            require('Access denied' not in result.stderr, 'Connector error leaked')
            require(str(folder) not in result.stderr, 'Private path leaked')
        try:
            daemon = start(['--skip-ssl'])
            # Deliberate quotes/backslash verify password bytes are not treated as SQL.
            secret = "db-test-secret'\\-42"
            password = folder / 'password'
            password.write_text(secret + '\n'); password.chmod(0o600)
            native_hash = '*' + hashlib.sha1(hashlib.sha1(secret.encode()).digest()).hexdigest().upper()
            sql('CREATE DATABASE dc24h CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;'
                "CREATE USER 'dc24h'@'localhost' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                "CREATE USER 'dc24h'@'127.0.0.1' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP ON dc24h.* TO 'dc24h'@'localhost';"
                "GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP ON dc24h.* TO 'dc24h'@'127.0.0.1';")
            path = config()
            for action in ['--check-config', '--check-database', '--database-status']:
                result = cli(path, action)
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            require('version=0 latest=2 pending=2 dirty=no' in cli(path, '--database-status').stdout,
                    'Fresh schema status is incorrect')
            require(sql('SELECT COUNT(*) FROM information_schema.tables WHERE table_schema="dc24h"') == '0',
                    'Check command modified schema')
            result = run([TEST, str(db_socket), str(password)])
            require(result.returncode == 0, result.stderr)
            expected = hashlib.sha256((Path(__file__).resolve().parents[1] /
                                      'database/migrations/0001_schema_migrations.sql').read_bytes()).hexdigest()
            require(sql('SELECT checksum FROM dc24h.dc24h_schema_migrations WHERE version=1') == expected,
                    'Migration checksum differs from SQL source')
            result = cli(path, '--migrate-database')
            require(result.returncode == 0 and 'applied=0' in result.stdout, result.stderr)
            require('version=2 latest=2 pending=0 dirty=no' in cli(path, '--database-status').stdout,
                    'Schema status does not match applied migrations')
            if MIGRATION_TEST:
                result = run([MIGRATION_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            if CORE_TEST:
                sql("DELIMITER //\nCREATE PROCEDURE dc24h.dc24h_core_multiple_results() BEGIN SELECT 1; SELECT 2; END//\nDELIMITER ;\n"
                    "GRANT EXECUTE ON PROCEDURE dc24h.dc24h_core_multiple_results TO 'dc24h'@'localhost';")
                result = run([CORE_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)

            # A read-only service account can connect but cannot run DDL migrations.
            sql("CREATE USER 'runtime'@'localhost' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                "GRANT SELECT ON dc24h.* TO 'runtime'@'localhost';")
            config(db_user='runtime')
            require(cli(path, '--check-database').returncode == 0, 'Read-only connection failed')
            if CORE_TEST:
                result = run([CORE_TEST, str(db_socket), str(password), '--readonly'])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)

            result = cli(path, '--migrate-database')
            require(result.returncode == 0 and 'applied=0' in result.stdout, 'Read-only no-op migration failed')
            require(cli(path, '--database-status').returncode == 0, 'Read-only schema inspection failed')
            # A legacy registry requires ALTER; inspection never upgrades it.
            sql('DELETE FROM dc24h.dc24h_schema_migrations WHERE version=2;'
                'ALTER TABLE dc24h.dc24h_schema_migrations DROP COLUMN state;')
            result = cli(path, '--migrate-database')
            require(result.returncode == 1, 'Upgrade accepted without ALTER privileges'); no_secrets(result)
            require('version=1 latest=2 pending=1 dirty=no' in cli(path, '--database-status').stdout,
                    'Read-only status changed legacy schema')
            config()
            require('applied=1' in cli(path, '--migrate-database').stdout, 'Legacy upgrade failed')
            api_path, api_token = enable_api(path, folder)
            if NCDC:
                for transport in ([], ['--tls']):
                    result = run([sys.executable, str(Path(__file__).with_name('ncdc_login_test.py')),
                                  BINARY, NCDC, MSG_CODEC, '--database-config', str(path), *transport])
                    require(result.returncode == 0, result.stderr)
                    no_secrets(result)
            # Enabled database startup and graceful shutdown.
            hub = subprocess.Popen([BINARY, '--config', str(path)], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, bufsize=0)
            log = bytearray()
            with selectors.DefaultSelector() as selector:
                selector.register(hub.stderr, selectors.EVENT_READ)
                deadline = time.monotonic() + 5
                while b'Network listening on' not in log:
                    remaining = deadline - time.monotonic()
                    require(remaining > 0 and selector.select(remaining), 'Hub startup timed out')
                    chunk = os.read(hub.stderr.fileno(), 4096)
                    require(chunk, 'Hub startup failed: ' + repr(log))
                    log.extend(chunk)
            require(b'Database connected' in log, 'Database not connected before network startup')
            require(fetch_status(api_path, api_token)['database']['enabled'], 'API database status is incorrect')
            hub.send_signal(signal.SIGTERM)
            out, err = hub.communicate(timeout=5)
            require(hub.returncode == 0, 'Enabled database shutdown failed')
            require(secret.encode() not in log + err + out, 'Startup leaked database password')
            hub = None
            require(not api_path.exists(), 'API socket leaked after database shutdown')
            # Database actions do not create the configured log.
            for action in ['--check-database', '--migrate-database', '--database-status']:
                result = run([BINARY, '--config', str(path), '--log-file', str(folder / 'unused.log'), action])
                require(result.returncode == 0 and not (folder / 'unused.log').exists(), result.stderr)
            result = cli(path, '--language', 'pl', '--check-database')
            require(result.returncode == 0 and 'Połączenie z bazą' in result.stdout, result.stderr)
            result = cli(path, '--language', 'pl', '--database-status')
            require(result.returncode == 0 and 'Wersja schematu bazy danych=2' in result.stdout, result.stderr)
            config(db_user='missing_private_user')
            result = cli(path, '--check-database')
            require(result.returncode == 1, 'Missing user accepted'); no_secrets(result)
            config('name=missing_private_database\n')
            result = cli(path, '--check-database')
            require(result.returncode == 1, 'Missing database accepted'); no_secrets(result)
            config(); password.write_text('wrong-secret\n')
            result = cli(path, '--check-database')
            require(result.returncode == 1, 'Wrong password accepted'); no_secrets(result)
            result = cli(path)
            require(result.returncode == 1 and 'Network listening on' not in result.stderr,
                    'Database startup failure opened network listeners'); no_secrets(result)
            password.write_text(secret + '\n'); password.chmod(0o644)
            result = cli(path, '--check-database')
            require(result.returncode == 1, 'Public password file accepted'); no_secrets(result)
            require(cli(path, '--check-config').returncode == 0, 'Dry run read password')
            password.chmod(0o600)
            # Log destination may never alias the password through a hardlink.
            for alias in [password, folder / 'password-alias']:
                if alias != password:
                    os.link(password, alias)
                result = run([BINARY, '--config', str(path), '--log-file', str(alias), '--check-config'])
                require(result.returncode == 2, 'Log/secret alias accepted')
            (folder / 'password-alias').unlink()
            require(password.read_text() == secret + '\n', 'Log changed secret file')
            for action in ['--check-config', '--check-database', '--database-status']:
                result = cli(path, action, '--migrate-database')
                require(result.returncode == 2, 'Conflicting database action accepted')
            for action in ['--check-config', '--check-database', '--migrate-database']:
                require(cli(path, '--database-status', action).returncode == 2, 'Conflicting status action accepted')
            for action in ['--help', '--version']:
                require(cli(path, '--database-status', action).returncode == 2, 'Status accepted with help/version')
            for setting in ['port=0', 'port=65536', 'timeout=0', 'timeout=61', 'password=SECRET',
                            'enabled=on', 'name=bad-name', 'host=localhost', 'tls_ca=server.pem']:
                config(setting + '\n')
                result = cli(path, '--check-config')
                require(result.returncode == 2, 'Invalid setting accepted: ' + setting)
            path.write_text('[database]\nenabled=off\nsocket=missing-db.sock\npassword_file=missing-password\n')
            require(cli(path, '--check-config').returncode == 0, 'Disabled database read credentials')
            for action in ['--check-database', '--migrate-database', '--database-status']:
                require(cli(path, action).returncode == 2, 'Database action accepted in disabled mode')
            config(password='missing-private-password')
            result = cli(path, '--check-database')
            require(result.returncode == 1, 'Missing private password accepted'); no_secrets(result)
            require(cli(path, '--check-config').returncode == 0, 'Dry run opened private password')
            print('Disposable MariaDB: connections, prepared values, migrations, configuration and privacy OK')
        finally:
            if hub is not None:
                hub.kill(); hub.communicate(timeout=5)
            if daemon is not None:
                daemon.terminate()
                try:
                    daemon.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    daemon.kill(); daemon.wait()


if __name__ == '__main__':
    main()
