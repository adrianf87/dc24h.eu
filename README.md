# dc24h.eu

Direct Connect Hub server implemented in C++20 for Linux (Debian and Ubuntu).

**Current development version: alpha-0.1.5 (Configuration).**

Completed increments:

- **alpha-0.1.1 Core**: lifecycle, signal handling, graceful shutdown.
- **alpha-0.1.2 Network**: multiple nonblocking IPv4/IPv6 TCP listeners/clients.
- **alpha-0.1.3 ADC**: bounded stream parser, commands, UTF-8/NFC validation.
- **alpha-0.1.4 ADCS**: optional nonblocking TLS 1.2+ listeners using OpenSSL.
- **alpha-0.1.5 Configuration**: bounded UTF-8 INI, CLI overrides, port/limit/path checks and append-file logging.

A full ADC/ADCS hub is **not yet implemented**: no complete ADC handshake, user sessions, authentication, messaging, search, or transfer coordination. MariaDB and React WebAdmin remain future milestones.

## Build

```sh
sudo apt-get install build-essential cmake libutf8proc-dev libssl-dev openssl python3 ncdc
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

Defaults, then explicit INI, then CLI. No file is loaded automatically. INI paths use its directory; CLI paths use the working directory. Use `--log-level info|error|off` and `--log-file PATH` (or `-` for stderr). See [configuration](doc/us/CONFIGURATION.md) for complete precedence, format, limits and logging constraints.
