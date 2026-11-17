# Debian / Ubuntu installation

**alpha-0.7.13 Database Resilience:** [explicit recovery and independent timeouts](DATABASE_RESILIENCE.md) add one owner-thread reconnect attempt, fresh password-file validation and protection against SQL replay or live transaction replacement. Schema v6 and startup authentication snapshots remain. CI uses GitHub-hosted Ubuntu 24.04 runners; remote completion is not awaited. This entry supersedes earlier descriptions of reconnect as unavailable.

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

Targets: Debian 12/13 and Ubuntu 22.04/24.04/26.04 LTS. Only the locally tested environment is claimed in [TESTING.md](TESTING.md); a target list is not proof of compatibility.

```sh
sudo apt-get update
sudo apt-get install build-essential cmake git python3 libutf8proc-dev libgcrypt20-dev libssl-dev openssl ncdc libmariadb-dev
git clone --branch alpha-0.1.11 https://github.com/adrianf87/dc24h.eu.git
cd dc24h.eu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/dc24h --help
./build/dc24h --listen 127.0.0.1:1511
```

Run as an unprivileged account. The process stays in the foreground and stops with Ctrl+C or SIGTERM. It does not daemonize, create a PID file, edit firewall rules or install/start a system service automatically. A future deployment can use systemd Type=simple, an absolute ExecStart path, a dedicated user and journal capture of stderr.

For ADCS configure readable certificate/key paths with the correct service-user permissions; see [ADCS_TLS.md](ADCS_TLS.md). The executable does not obtain or renew certificates. Apache2 is optional for static WebAdmin assets; the authenticated production gateway remains planned; MariaDB connections are optional and disabled by default. See [DATABASE.md](DATABASE.md). See [CONFIGURATION.md](CONFIGURATION.md) for port and language options.

For explicit file settings, copy the [example](../../config/dc24h.ini.example), adjust paths and run `./build/dc24h --config config/dc24h.ini --check-config` before normal startup with the same `--config`. Local INI files are ignored by Git.

For the installed static HTTPS template, certificate paths, local access policy and explicit Apache activation, see [APACHE2_HTTPS.md](APACHE2_HTTPS.md). The example is always installed under `etc/dc24h`; no certificate/site/service is activated by CMake.
