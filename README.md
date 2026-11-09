# dc24h.eu

Direct Connect Hub server implemented in C++20 for Linux (Debian and Ubuntu).

**Current development version: alpha-0.2.7 (CID/PID).**

Completed increments:

- **alpha-0.2.7 CID/PID**: dedicated Tiger identity verifier, typed public CID/verified result, direct cleansed buffers, constant-time digest comparison and deterministic provider-failure tests. INF uses the shared verifier; Network attachment remains planned.

- **alpha-0.2.6 INF**: bounded client metadata, Tiger PID/CID verification, peer-address and field validation, transactional updates and public snapshots without PD. Composed ClientSession is tested with real ncdc; live transport attachment remains planned.

- **alpha-0.2.5 SID**: typed 20-bit base32 SID codec, bounded shared hub pool, move-only RAII reservations and SidSession composition with SUP/state. Real ncdc fixture uses allocated SIDs; live Network attachment remains planned.

- **alpha-0.2.4 SUP**: bounded feature AD/RM sets, BASE checks, TIGR session-hash selection, deterministic ISUP and composition with ADC State. Tested with real ncdc in an isolated fixture; Network attachment remains planned.

- **alpha-0.2.3 ADC State**: independent hub connection state machine, explicit validation gates, command/direction checks and source-SID binding. Transport wiring and full login remain planned.

- **alpha-0.2.2 ADC Parser**: BASE argument layouts, named-field syntax, bounded decimal integers, feature budgets and TCP/TLS recovery tests.

- **alpha-0.2.1 Project**: [alpha-0.1 audit](doc/us/AUDIT_ALPHA_0.1.md), prioritized alpha-0.2 acceptance plan, version/documentation checks and required CI integrations.

- **alpha-0.1.1 Core**: lifecycle, signal handling, graceful shutdown.
- **alpha-0.1.2 Network**: multiple nonblocking IPv4/IPv6 TCP listeners/clients.
- **alpha-0.1.3 ADC**: bounded stream parser, commands, UTF-8/NFC validation.
- **alpha-0.1.4 ADCS**: optional nonblocking TLS 1.2+ listeners using OpenSSL.
- **alpha-0.1.5 Configuration**: bounded UTF-8 INI, CLI overrides, port/limit/path checks and append-file logging.
- **alpha-0.1.6 Logging**: startup summaries, WARN, opt-in connection events, bounded single-line UTF-8 records and rate budgets.
- **alpha-0.1.7 Security**: bounded per-IP admissions, frame quotas and absolute TLS/unfinished-frame deadlines.
- **alpha-0.1.8 Database**: optional MariaDB connection, private password files, local Unix sockets, prepared queries and explicit migration metadata baseline.

- **alpha-0.1.9 API**: optional local HTTP/JSON status and health routes, private Unix socket, same-UID access and Bearer authentication.

- **alpha-0.1.10 WebAdmin**: responsive React/TypeScript/Vite read-only panel, EN/PL, safe local development adapter, tests and an Apache2 static-preview example.

- **alpha-0.1.11 Apache2**: customizable static WebAdmin HTTPS example, HTTP→HTTPS redirect, TLS 1.2/1.3, certificate paths and isolated HTTPS tests.

A full ADC/ADCS hub is **not yet implemented**: no complete ADC handshake or live user sessions, hub-user authentication, messaging, search, or transfer coordination. Account persistence and a production authenticated web gateway remain future milestones.

## Build

```sh
sudo apt-get install build-essential cmake libutf8proc-dev libgcrypt20-dev libssl-dev openssl python3 ncdc libmariadb-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/dc24h --version
```

## Run

```sh
# Plain ADC parser on loopback:
./build/dc24h --listen 127.0.0.1:1511

# TLS only (replace credentials with your own):
./build/dc24h --adcs-listen 127.0.0.1:1512 \
  --tls-cert /etc/dc24h/tls/fullchain.pem \
  --tls-key /etc/dc24h/tls/privkey.pem
```

Use `--language pl` for Polish help and operator messages; `us` is the default.
Add explicit `--listen` options to enable mixed plaintext/TLS service.

Stop with Ctrl+C or SIGTERM. The key must not be committed to Git.
CMake 3.22+, C++20, OpenSSL and utf8proc development packages are required.

Documentation: [English (US)](doc/us/README.md) | [Polski](doc/pl/README.md) | [Module documentation index](doc/README.md) | [CHANGELOG.md](CHANGELOG.md).

License: [GPL-3.0](LICENSE).

## Configuration

```sh
cp config/dc24h.ini.example config/dc24h.ini
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini --max-connections 512
```

Defaults, then explicit INI, then CLI. No file is loaded automatically. INI paths use its directory; CLI paths use the working directory. Use `--log-level info|warning|error|off` and `--log-file PATH` (or `-` for stderr). See [configuration](doc/us/CONFIGURATION.md) for complete precedence, format, limits and logging constraints.

Connection records are off by default; enable with `--log-connections on` or `[logging] connections=on` at INFO. Records use internal IDs and fixed events without client addresses or ADC payloads. See [logging](doc/us/LOGGING.md) for limits and suppression behavior.

Security defaults: 128 active peers/IP, 256 admissions/IP/minute, 1024 decoder results/s/connection, 512 invalid frames/connection, and 10-second TLS/partial-frame deadlines. Frame bodies remain capped at 16 KiB with 128 parameters. Configure `[security]` or CLI; see [security](doc/us/SECURITY.md) for exact policies and limitations.

MariaDB is disabled by default. Configure `[database]` to enable connection checks before listener startup. Use `--check-database` to connect/ping or `--migrate-database` to explicitly create/verify migration metadata; startup never migrates. Install `mariadb-server` to run disposable integration tests. See [database documentation](doc/us/DATABASE.md) for setup, privileges and limits.

The administrative API is disabled by default. Enable `[api]` with a private Unix socket directory and a private 64-hex-character token file. Authenticated `GET /api/v1/health` and `GET /api/v1/status` are read-only. The read-only WebAdmin uses a local development adapter; production web authentication, a remote gateway and administrative mutations remain planned. See [API setup and contract](doc/us/API.md).

## WebAdmin

```sh
cd webadmin
npm ci
npm run build
npm run dev
```

Requires Node.js `^20.19.0 || >=22.12.0`. Open the local URL printed by Vite. Without API configuration the panel shows unavailable values. For the optional real C++ connection, use the same service UID and server-only socket/token-file path variables; never paste a service token into the browser. See [WebAdmin US](doc/us/WEBADMIN.md) / [WebAdmin PL](doc/pl/WEBADMIN.md) for setup, Apache2 and optional CMake asset installation. Production browser authentication, roles, mutations and an authenticated Apache gateway remain planned.

An Apache2 HTTPS template is included at [config/apache2-webadmin-https.conf.example](config/apache2-webadmin-https.conf.example) and installed with the configuration examples. Customize domain/certificate/asset paths before enabling it; default access is local and API routes remain denied. Setup: [HTTPS US](doc/us/APACHE2_HTTPS.md) / [HTTPS PL](doc/pl/APACHE2_HTTPS.md). No host site or certificate is activated automatically.
