# Database and login configuration — alpha-0.7.15

This stage consolidates the existing MariaDB and ADC account-login policy, adds
one shared timeout resolver, a redacted CLI preview and the read-only WebAdmin
Configuration page. It preserves schema v6, credentials, identity mappings,
GPA/PAS, role policy, existing guest rules and maintenance-only account writes.

## Private INI source

Edit the service's explicit `--config FILE`. Relative paths resolve against its
INI directory. Files use UTF-8, bounded strict keys and literal values: unknown
or duplicate keys, invalid on/off values and out-of-range numbers fail before
startup. There are no database passwords or ADC keys in command-line arguments,
environment expansion, public JSON or generated browser assets.

The existing [default example](../../config/dc24h.ini.example) keeps SQL and
account authentication off. The new [account-enabled ADCS example](../../config/adc-accounts.ini.example)
is a complete loopback ADCS-only profile requiring provisioned files/accounts.
It is not an automatic installer and does not change the existing guest policy.
Keep production copies, parent directories and private credential files protected.

| `[database]` key | Default / accepted values |
| --- | --- |
| enabled | off; strict on/off |
| socket | /run/mysqld/mysqld.sock; nonempty local Unix socket path |
| name | dc24h; 1..64 ASCII letters, digits or underscore |
| user | dc24h; 1..128 bytes without controls |
| password_file | private file required when enabled |
| timeout | 5; integer 1..60 seconds |
| connect_timeout, read_timeout, write_timeout | optional 1..60; each inherits timeout when omitted |

The Connector/C connection and public preview use the same resolver. A C++
override of zero means inheritance; explicit INI zero is invalid. These are
connection/packet-I/O limits, not a total query, transaction or shutdown deadline.
SQL remains local Unix socket only, with no TCP fallback, implicit reconnect or
write replay. See [Database Resilience](DATABASE_RESILIENCE.md).

| `[authentication]` key | Default / accepted values |
| --- | --- |
| enabled | off; strict on/off |
| key_file | separate private 32-byte AES key file required when enabled |
| require_tls | on; account login requires ADCS unless explicitly off |
| max_accounts | 1024; integer 1..4096, also checked in disabled C++ settings |
| attempts_per_ip / attempts_per_account | 20 / 10; each 1..100000 |
| attempts_per_second | 128; global 1..100000 |
| attempt_window_seconds | 60; refill horizon 1..3600 seconds |
| max_tracked_attempt_ips / max_tracked_attempt_accounts | 4096 / 4096; each 1..65536 |

Account authentication requires enabled SQL and its private password file, a
separate ADC key, current schema and valid enrolled accounts. require_tls=on also
requires a configured ADCS listener. TLS certificate and private key must match
at startup. There is no silent downgrade. Guest access remains governed by the
existing identity and feature policies, independently of account authentication.

Login protection also uses `[security]`: login_timeout=30,
tls_handshake_timeout=10, output_timeout=10, close_timeout=2 (all 1..3600 seconds),
max_pending_logins=256, max_pending_per_ip=64 (1..65535), max_login_bytes=65536
(1..16777216), and max_login_frames=64 (1..65535). Authentication token buckets
are shared across ADC/ADCS and reconnects, charge successful attempts too and use
a continuous refill horizon. These settings do not configure persistent bans.

## Validate and inspect

```sh
dc24h --config /etc/dc24h/hub.ini --show-configuration
dc24h --config /etc/dc24h/hub.ini --check-config
dc24h --config /etc/dc24h/hub.ini --check-database
```

`--show-configuration` validates merged syntax, bounds and dependencies, then
prints stable language-independent JSON with effective timeout values and safe
login limits. It reads no password, ADC key or TLS material, never connects to
SQL, binds listeners or opens logs. Thus success is not proof that credentials,
certificates or database availability are correct. `--check-config` additionally
checks enabled ADC/TLS credentials; `--check-database` separately connects/pings.
Migrations and status inspection retain their separate explicit commands.

Preview is mutually exclusive with check-config, database actions, help and
version; invalid combinations exit 2. The normal INI/CLI precedence still applies.
Changing `--language` translates diagnostics/help but never machine JSON keys.
Existing strict file/path protections and no-auto-migration rules remain.

## Public snapshot and panel

Authenticated `GET /api/v1/status` optionally includes `configuration`:
mode (`hub` or `maintenance`), restart_required=true, database enabled/Unix transport/
base and effective timeouts, plus authentication switches/capacities/attempt
limits/login budgets. The C++ data type deliberately has no database name, user,
socket path, password, key or credential-file path. Only these allowlisted fields
are serialized, forwarded and exported. CLI and serving snapshots share the same
projection. The maintenance API reports its actual non-serving mode and disabled
account authentication; it does not load the serving authentication directory.

The Configuration navigation opens a bilingual read-only page. It distinguishes
startup policy from live database health, shows timeout inheritance results,
ADCS requirements and login quotas, and explains the restart procedure. Missing
configuration from an older API displays unavailable data without blocking its
existing monitoring. Malformed new settings reject the whole snapshot and clear
stale values/export availability. Normal polling/pause/manual refresh and safe
snapshot export are shared with the existing panel.

Edit private configuration, validate, then restart the relevant process. No
configuration-writing endpoint, browser credential editor, hot reload, background
SQL probe, account-directory reload or authentication-policy mutation is added.
The separate [account maintenance workflow](WEBADMIN_ACCOUNTS.md) remains required
for account writes. Apache static examples remain unchanged.

## Verification and CI

Native tests cover C++ disabled-mode bounds, timeout inheritance, public-data
redaction and mode dependencies. Process tests prove the preview works with
missing SQL/ADC/TLS credentials without side effects, rejects conflicting actions
and emits identical machine JSON in US/PL. Disposable MariaDB and authenticated
ADC/ADCS tests compare configured policy to running snapshots; real ncdc tests
remain enabled. Node/Chromium tests cover strict types/bounds, credential stripping,
older API fallback, export, read-only controls and Polish mobile layout. The real
C++ → adapter → browser integration verifies effective timeout rendering.

Each public GitHub Ubuntu 24.04 runner receives four logical CPUs, checked with
nproc=4; build/test parallelism derives from nproc without a fixed worker cap.
Remote CI completion is not awaited. See [CI](GITHUB_CI.md).
