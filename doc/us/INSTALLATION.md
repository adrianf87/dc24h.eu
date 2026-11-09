# Debian / Ubuntu installation

Targets: Debian 12/13 and Ubuntu 22.04/24.04/26.04 LTS. Only the locally tested environment is claimed in [TESTING.md](TESTING.md); a target list is not proof of compatibility.

```sh
sudo apt-get update
sudo apt-get install build-essential cmake git python3 libutf8proc-dev libssl-dev openssl ncdc libmariadb-dev
git clone --branch alpha-0.1.10 https://github.com/adrianf87/dc24h.eu.git
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
