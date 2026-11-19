"""Disposable MariaDB only: never contacts the host's database or reads my.cnf."""
import hashlib
import json
import os
from pathlib import Path
import pwd
import re
import selectors
import signal
import socket
import subprocess
import sys
import tempfile
import time

from api_support import enable_api, fetch_status

BANS_TEST = None
if '--bans-test' in sys.argv:
    index = sys.argv.index('--bans-test');BANS_TEST = os.path.abspath(sys.argv[index+1]);del sys.argv[index:index+2]
ADMIN_CORE_TEST = None
if '--admin-core-test' in sys.argv:
    index = sys.argv.index('--admin-core-test')
    ADMIN_CORE_TEST = os.path.abspath(sys.argv[index+1])
    del sys.argv[index:index+2]
OWNER_CLI_TEST = None
if '--owner-cli-test' in sys.argv:
    index = sys.argv.index('--owner-cli-test')
    OWNER_CLI_TEST = os.path.abspath(sys.argv[index+1])
    del sys.argv[index:index+2]
WEBADMIN_TEST = None
if '--account-webadmin-test' in sys.argv:
    index = sys.argv.index('--account-webadmin-test')
    WEBADMIN_TEST = (os.path.abspath(sys.argv[index+1]), os.path.abspath(sys.argv[index+2]))
    del sys.argv[index:index+3]
RESILIENCE_TEST = None
if '--resilience-test' in sys.argv:
    index = sys.argv.index('--resilience-test')
    RESILIENCE_TEST = os.path.abspath(sys.argv[index + 1])
    del sys.argv[index:index + 2]
MANAGEMENT_TEST = None
if '--management-test' in sys.argv:
    index = sys.argv.index('--management-test')
    MANAGEMENT_TEST = os.path.abspath(sys.argv[index + 1])
    del sys.argv[index:index + 2]
ADC_AUTH_TEST = None
if '--adc-authentication-test' in sys.argv:
    index = sys.argv.index('--adc-authentication-test')
    ADC_AUTH_TEST = os.path.abspath(sys.argv[index + 1])
    del sys.argv[index:index + 2]
PASSWORD_TEST = None
if '--password-test' in sys.argv:
    index = sys.argv.index('--password-test')
    PASSWORD_TEST = os.path.abspath(sys.argv[index + 1])
    del sys.argv[index:index + 2]
IDENTITY_TEST = None
if '--identity-test' in sys.argv:
    index = sys.argv.index('--identity-test')
    IDENTITY_TEST = os.path.abspath(sys.argv[index + 1])
    del sys.argv[index:index + 2]
REGISTRATION_TEST = None
if '--registration-test' in sys.argv:
    index = sys.argv.index('--registration-test')
    REGISTRATION_TEST = os.path.abspath(sys.argv[index + 1])
    del sys.argv[index:index + 2]
ACCOUNT_TEST = None
if '--account-test' in sys.argv:
    index = sys.argv.index('--account-test')
    ACCOUNT_TEST = os.path.abspath(sys.argv[index + 1])
    del sys.argv[index:index + 2]
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
        recovery = None
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
            if RESILIENCE_TEST:
                sql("GRANT CREATE TEMPORARY TABLES ON dc24h.* TO 'dc24h'@'localhost';")
                # An accepting Unix socket that never sends a server handshake.
                blackhole = folder / 'blackhole.sock'
                with socket.socket(socket.AF_UNIX) as listener:
                    listener.bind(str(blackhole)); listener.listen(1); listener.settimeout(5)
                    stalled = subprocess.Popen([RESILIENCE_TEST, str(blackhole), str(password), 'handshake'],
                                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                    try:
                        with listener.accept()[0]:
                            out, err = stalled.communicate(timeout=12)
                        require(stalled.returncode == 0, err)
                        no_secrets(subprocess.CompletedProcess([], stalled.returncode, out, err))
                    finally:
                        if stalled.poll() is None:
                            stalled.kill(); stalled.communicate(timeout=5)
                for key in ('connect_timeout', 'read_timeout', 'write_timeout'):
                    for value in (1, 60):
                        require(cli(config(f'{key}={value}\n'), '--check-config').returncode == 0,
                                'Valid independent database timeout rejected')
                    for value in (0, 61, -1, 'x', ''):
                        require(cli(config(f'{key}={value}\n'), '--check-config').returncode == 2,
                                'Invalid independent database timeout accepted')
                    require(cli(config(f'{key}=1\n{key}=2\n'), '--check-config').returncode == 2,
                            'Duplicate independent database timeout accepted')
                config()
            for action in ['--check-config', '--check-database', '--database-status']:
                result = cli(path, action)
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            require('version=0 latest=9 pending=9 dirty=no' in cli(path, '--database-status').stdout,
                    'Fresh schema status is incorrect')
            early = folder / 'early.bk'
            refused = cli(path, '--backup-database', str(early))
            require(refused.returncode == 1, refused.stderr)
            require(not early.exists(), 'Unfinished schema left a backup file')
            no_secrets(refused)
            require(str(folder) not in refused.stderr, 'Backup path leaked')
            require(sql('SELECT COUNT(*) FROM information_schema.tables WHERE table_schema="dc24h"') == '0',
                    'Check command modified schema')
            if RESILIENCE_TEST:
                result = run([RESILIENCE_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
                from database_fault_proxy import fault_case
                for mode in ('write', 'commit_ack'):
                    no_secrets(fault_case(RESILIENCE_TEST, folder, db_socket, password, mode))
                require(sql('SELECT value FROM dc24h.dc24h_ack_fixture WHERE id=1') == '1',
                        'Acknowledged COMMIT was lost or replayed')
                sql('DROP TABLE dc24h.dc24h_ack_fixture;')
                sql('DROP TABLE dc24h.dc24h_resilience_fixture;')
            result = run([TEST, str(db_socket), str(password)])
            require(result.returncode == 0, result.stderr)
            expected = hashlib.sha256((Path(__file__).resolve().parents[1] /
                                      'database/migrations/0001_schema_migrations.sql').read_bytes()).hexdigest()
            require(sql('SELECT checksum FROM dc24h.dc24h_schema_migrations WHERE version=1') == expected,
                    'Migration checksum differs from SQL source')
            result = cli(path, '--migrate-database')
            require(result.returncode == 0 and 'applied=0' in result.stdout, result.stderr)
            require('version=9 latest=9 pending=0 dirty=no' in cli(path, '--database-status').stdout,
                    'Schema status does not match applied migrations')
            sql("INSERT INTO dc24h.dc24h_accounts (id,name,role,status,created_at,updated_at,revision) "
                "VALUES (7,'backup-user','registered','active',1,1,1)")
            sql("INSERT INTO dc24h.dc24h_account_credentials (account_id,verifier,created_at) "
                "VALUES (7,UNHEX('00FF61'),1)")
            snapshot = folder / 'cli-backup.bk'
            saved = cli(path, '--backup-database', str(snapshot))
            require(saved.returncode == 0 and saved.stdout.startswith('Database backup written bytes='), saved.stderr)
            no_secrets(saved)
            require(str(folder) not in saved.stdout + saved.stderr, 'Backup path leaked')
            require((snapshot.stat().st_mode & 0o777) == 0o600, 'Backup file is not owner-only')
            payload = snapshot.read_bytes()
            require(secret.encode() not in payload, 'Database password stored in the backup')
            require(b'backup-user' in payload and b'\x00\xffa' in payload, 'Backup omitted stored rows')
            again = cli(path, '--backup-database', str(snapshot))
            require(again.returncode == 1 and snapshot.read_bytes() == payload, again.stderr)
            no_secrets(again)
            link = folder / 'cli-backup.link'
            link.symlink_to(snapshot)
            linked = cli(path, '--backup-database', str(link))
            require(linked.returncode == 1 and snapshot.read_bytes() == payload, linked.stderr)
            sql('DELETE FROM dc24h.dc24h_account_credentials')
            sql('DELETE FROM dc24h.dc24h_accounts')
            snapshot.chmod(0o640)
            exposed = cli(path, '--restore-database', str(snapshot))
            require(exposed.returncode == 1 and sql('SELECT COUNT(*) FROM dc24h.dc24h_accounts') == '0', exposed.stderr)
            no_secrets(exposed)
            snapshot.chmod(0o600)
            loaded = cli(path, '--language', 'pl', '--restore-database', str(snapshot))
            require(loaded.returncode == 0 and 'wiersze=' in loaded.stdout, loaded.stderr)
            require(str(folder) not in loaded.stdout + loaded.stderr, 'Restore path leaked')
            require(sql("SELECT HEX(verifier) FROM dc24h.dc24h_account_credentials WHERE account_id=7") == '00FF61',
                    'Restored credential bytes differ')
            repeat = cli(path, '--restore-database', str(snapshot))
            require(repeat.returncode == 1 and sql('SELECT COUNT(*) FROM dc24h.dc24h_accounts') == '1', repeat.stderr)
            sql('DELETE FROM dc24h.dc24h_account_credentials')
            sql('DELETE FROM dc24h.dc24h_accounts')
            require(sql('SELECT COUNT(*) FROM dc24h.dc24h_accounts') == '0', 'Backup test left an account')
            sql('ALTER TABLE dc24h.dc24h_accounts AUTO_INCREMENT=1')
            snapshot.unlink()
            link.unlink()
            if OWNER_CLI_TEST:
                from owner_cli_process_tests import exercise
                exercise(OWNER_CLI_TEST, path, folder, sql)
            if MIGRATION_TEST:
                result = run([MIGRATION_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            if REGISTRATION_TEST:
                sql("CREATE USER 'registration_limited'@'localhost' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                    "GRANT SELECT ON dc24h.* TO 'registration_limited'@'localhost';"
                    "GRANT INSERT ON dc24h.dc24h_accounts TO 'registration_limited'@'localhost';")
                result = run([REGISTRATION_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
                require('Registration test password 42!' not in result.stdout + result.stderr and
                        '$dc24h-scrypt$' not in result.stdout + result.stderr, 'Registration credential leaked')
                stored = sql("SELECT HEX(c.verifier) FROM dc24h.dc24h_account_credentials c "
                             "JOIN dc24h.dc24h_accounts a ON a.id=c.account_id "
                             "WHERE a.name IN ('RegisteredOne','RegisteredTwo') ORDER BY a.name").splitlines()
                require(len(stored) == 2 and stored[0] != stored[1], 'Independent salts missing')
                for text in stored:
                    encoded = bytes.fromhex(text).decode('ascii')
                    fields = encoded.split('$')
                    require(len(fields) == 5 and fields[:3] == ['', 'dc24h-scrypt', 'v=1,ln=17,r=8,p=1'],
                            'Unexpected credential format')
                    salt, key = map(bytes.fromhex, fields[3:])
                    require(len(salt) == 32 and len(key) == 32, 'Unexpected credential size')
                    expected_key = hashlib.scrypt(b'Registration test password 42!', salt=salt, n=131072, r=8, p=1,
                                                  maxmem=256 * 1024 * 1024, dklen=32)
                    require(key == expected_key, 'Independent password derivation differs')
            if BANS_TEST:
                result = run([BANS_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr);no_secrets(result)
                from database_fault_proxy import fault_case
                actor_id = sql("SELECT id FROM dc24h.dc24h_accounts WHERE name='BanOwner'")
                no_secrets(fault_case(BANS_TEST, folder, db_socket, password, 'commit_ack', (actor_id,)))
                require(sql("SELECT COUNT(*) FROM dc24h.dc24h_bans WHERE subject='192.0.2.99' AND revision=1") == '1', 'Ban COMMIT was replayed or not committed')
                from bans_process_tests import exercise
                exercise(BINARY,path,folder,sql)
            if ADMIN_CORE_TEST:
                sql("CREATE USER 'management_limited'@'localhost' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                    "GRANT SELECT ON dc24h.* TO 'management_limited'@'localhost';"
                    "GRANT UPDATE ON dc24h.dc24h_accounts TO 'management_limited'@'localhost';"
                    "GRANT INSERT,UPDATE ON dc24h.dc24h_account_credentials TO 'management_limited'@'localhost';")
                result = run([ADMIN_CORE_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
                from database_fault_proxy import fault_case
                actor_id = sql("SELECT id FROM dc24h.dc24h_accounts WHERE name='CoreSuccessor'")
                no_secrets(fault_case(ADMIN_CORE_TEST, folder, db_socket, password, 'commit_ack', (actor_id,)))
                require(sql("SELECT COUNT(*) FROM dc24h.dc24h_accounts WHERE name='CoreUnknownWrite' AND revision=1") == '1',
                        'Admin Core lost COMMIT was replayed or not committed')
                require(sql("SELECT COUNT(*) FROM dc24h.dc24h_account_credentials c JOIN dc24h.dc24h_accounts a ON a.id=c.account_id WHERE a.name='CoreUnknownWrite'") == '1',
                        'Admin Core lost COMMIT was not atomic')
            if MANAGEMENT_TEST:
                keyfile = folder / 'management-adc.key'
                keyfile.write_text('02' * 32 + '\n'); keyfile.chmod(0o600)
                sql("CREATE USER 'management_limited'@'localhost' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                    "GRANT SELECT ON dc24h.* TO 'management_limited'@'localhost';"
                    "GRANT UPDATE ON dc24h.dc24h_accounts TO 'management_limited'@'localhost';"
                    "GRANT INSERT,UPDATE ON dc24h.dc24h_account_credentials TO 'management_limited'@'localhost';")
                result = subprocess.run([MANAGEMENT_TEST, str(db_socket), str(password), str(keyfile)],
                                        capture_output=True, text=True, timeout=150)
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
                for value in ('Management original password', 'Management replacement password', 'Management reset password', '$dc24h-scrypt$'):
                    require(value not in result.stdout + result.stderr, 'Account management secret leaked')
                import shutil
                ncdc = shutil.which('ncdc')
                require(ncdc is not None, 'ncdc is required for managed account integration')
                result = subprocess.run([sys.executable, str(Path(__file__).with_name('adc_authentication_process_tests.py')),
                    BINARY, str(path), str(keyfile), ncdc,
                    'AuthenticationTests.test_real_ncdc_password_rejection_then_normal_adc_and_adcs'],
                    capture_output=True, text=True, timeout=45)
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            if ADC_AUTH_TEST:
                keyfile = folder / 'adc.key'
                keyfile.write_text('01' * 32 + '\n'); keyfile.chmod(0o600)
                sql("CREATE USER 'adc_runtime'@'localhost' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                    "GRANT SELECT ON dc24h.dc24h_bans TO 'adc_runtime'@'localhost';"
                    "GRANT SELECT ON dc24h.dc24h_schema_migrations TO 'adc_runtime'@'localhost';"
                    "GRANT SELECT ON dc24h.dc24h_accounts TO 'adc_runtime'@'localhost';"
                    "GRANT SELECT ON dc24h.dc24h_account_identities TO 'adc_runtime'@'localhost';"
                    "GRANT SELECT ON dc24h.dc24h_account_adc_credentials TO 'adc_runtime'@'localhost';")
                result = run([ADC_AUTH_TEST, str(db_socket), str(password), str(keyfile)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
                import shutil
                ncdc = shutil.which('ncdc')
                require(ncdc is not None, 'ncdc is required for ADC authentication integration')
                result = subprocess.run([sys.executable, str(Path(__file__).with_name('adc_authentication_process_tests.py')),
                                         BINARY, str(path), str(keyfile), ncdc], capture_output=True, text=True, timeout=90)
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            if PASSWORD_TEST:
                sql("CREATE USER 'password_limited'@'localhost' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                    "CREATE USER 'password_no_credentials'@'localhost' IDENTIFIED BY PASSWORD '" + native_hash + "';"
                    "GRANT SELECT ON dc24h.dc24h_schema_migrations TO 'password_limited'@'localhost';"
                    "GRANT SELECT ON dc24h.dc24h_accounts TO 'password_limited'@'localhost';"
                    "GRANT SELECT ON dc24h.dc24h_account_credentials TO 'password_limited'@'localhost';"
                    "GRANT SELECT ON dc24h.dc24h_schema_migrations TO 'password_no_credentials'@'localhost';"
                    "GRANT SELECT ON dc24h.dc24h_accounts TO 'password_no_credentials'@'localhost';")
                result = run([PASSWORD_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
                require('Registration test password 42!' not in result.stdout + result.stderr and
                        '$dc24h-scrypt$' not in result.stdout + result.stderr, 'Credential check leaked material')
            if IDENTITY_TEST:
                result = run([IDENTITY_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            if ACCOUNT_TEST:
                result = run([ACCOUNT_TEST, str(db_socket), str(password)])
                require(result.returncode == 0, 'Account test exit=' + str(result.returncode) + ': ' + result.stderr)
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

            if ACCOUNT_TEST:
                result = run([ACCOUNT_TEST, str(db_socket), str(password), '--readonly'])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            result = cli(path, '--migrate-database')
            require(result.returncode == 0 and 'applied=0' in result.stdout, 'Read-only no-op migration failed')
            require(cli(path, '--database-status').returncode == 0, 'Read-only schema inspection failed')
            if REGISTRATION_TEST:
                result = run([REGISTRATION_TEST, str(db_socket), str(password), '--readonly'])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            if IDENTITY_TEST:
                result = run([IDENTITY_TEST, str(db_socket), str(password), '--readonly'])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            if PASSWORD_TEST:
                result = run([PASSWORD_TEST, str(db_socket), str(password), '--readonly'])
                require(result.returncode == 0, result.stderr)
                no_secrets(result)
            # A legacy registry requires ALTER; inspection never upgrades it.
            sql('DROP TABLE dc24h.dc24h_audit_events;'
                'DROP TABLE dc24h.dc24h_bans;'
                'DROP TABLE dc24h.dc24h_account_adc_credentials;'
                'DROP TABLE dc24h.dc24h_account_identities;'
                'DROP TABLE dc24h.dc24h_account_credentials;'
                'DROP TABLE dc24h.dc24h_accounts;'
                'DELETE FROM dc24h.dc24h_schema_migrations WHERE version>=2;'
                'ALTER TABLE dc24h.dc24h_schema_migrations DROP COLUMN state;')
            result = cli(path, '--migrate-database')
            require(result.returncode == 1, 'Upgrade accepted without ALTER privileges'); no_secrets(result)
            require('version=1 latest=9 pending=8 dirty=no' in cli(path, '--database-status').stdout,
                    'Read-only status changed legacy schema')
            config()
            require('applied=8' in cli(path, '--migrate-database').stdout, 'Legacy upgrade failed')
            api_path, api_token = enable_api(path, folder)
            if NCDC:
                for transport in ([], ['--tls']):
                    result = run([sys.executable, str(Path(__file__).with_name('ncdc_login_test.py')),
                                  BINARY, NCDC, MSG_CODEC, '--database-config', str(path), *transport])
                    require(result.returncode == 0, result.stderr)
                    no_secrets(result)
            if WEBADMIN_TEST:
                from account_webadmin_process_tests import exercise
                exercise(*WEBADMIN_TEST, path, db_socket, password, api_token, api_path)
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
            published_policy = fetch_status(api_path, api_token)['configuration']
            expected_policy = json.loads(cli(path, '--show-configuration').stdout)
            require(published_policy == expected_policy, 'Serving and CLI effective policy differ')
            if RESILIENCE_TEST:
                recovery = subprocess.Popen([RESILIENCE_TEST, str(db_socket), str(password), 'restart'],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
                def recovery_line(expected):
                    with selectors.DefaultSelector() as ready:
                        ready.register(recovery.stdout, selectors.EVENT_READ)
                        require(ready.select(15), 'Recovery test response timed out')
                    require(recovery.stdout.readline().strip() == expected, 'Unexpected recovery test response')
                recovery_line('ready')
                daemon.terminate(); daemon.wait(timeout=15); daemon = None
                recovery.stdin.write('down\n'); recovery.stdin.flush(); recovery_line('down')
                # Startup configuration is not a liveness probe. Existing reactor
                # sessions and new guest handshakes work without synchronous SQL.
                require(fetch_status(api_path, api_token)['server']['core_running'], 'Database outage stopped hub API')
                endpoint = re.search(rb'Network listening on 127\.0\.0\.1:([0-9]+)', log)
                require(endpoint is not None, 'Missing loopback hub endpoint')
                with socket.create_connection(('127.0.0.1', int(endpoint[1])), timeout=3) as peer:
                    peer.sendall(b'HSUP ADBASE ADTIGR\n')
                    with peer.makefile('rb') as frames:
                        require(frames.readline() == b'ISUP ADBASE ADTIGR\n', 'Database outage stopped ADC')
                        require(frames.readline().startswith(b'ISID '), 'Database outage stopped SID allocation')
                unavailable = cli(path, '--check-database')
                require(unavailable.returncode == 1, 'Database outage accepted by CLI'); no_secrets(unavailable)
                daemon = start(['--skip-ssl'])
                recovery.stdin.write('up\n'); recovery.stdin.flush(); recovery_line('recovered')
                out, err = recovery.communicate(timeout=5)
                require(recovery.returncode == 0, err)
                no_secrets(subprocess.CompletedProcess([], recovery.returncode, out, err)); recovery = None
                require(cli(path, '--check-database').returncode == 0, 'Database did not recover')
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
            require(result.returncode == 0 and 'Wersja schematu bazy danych=9' in result.stdout, result.stderr)
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
            require(cli(path, '--backup-database', str(folder / 'conflict.bk'), '--migrate-database').returncode == 2,
                    'Backup accepted with migration')
            require(cli(path, '--restore-database', str(folder / 'conflict.bk'), '--database-status').returncode == 2,
                    'Restore accepted with status')
            require(not (folder / 'conflict.bk').exists(), 'Conflicting backup created a file')
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
            require(cli(path, '--backup-database', str(folder / 'disabled.bk')).returncode == 2,
                    'Backup accepted while the database is disabled')
            require(cli(path, '--restore-database', str(folder / 'disabled.bk')).returncode == 2,
                    'Restore accepted while the database is disabled')
            require(not (folder / 'disabled.bk').exists(), 'Disabled backup created a file')
            config(password='missing-private-password')
            result = cli(path, '--check-database')
            require(result.returncode == 1, 'Missing private password accepted'); no_secrets(result)
            require(cli(path, '--check-config').returncode == 0, 'Dry run opened private password')
            print('Disposable MariaDB: connections, prepared values, migrations, configuration and privacy OK')
        finally:
            if recovery is not None:
                recovery.kill(); recovery.communicate(timeout=5)
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
