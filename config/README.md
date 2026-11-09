# Configuration

Copy `dc24h.ini.example` to a local INI, edit settings, then run:

```sh
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini
```

Nothing is loaded implicitly. CLI overrides file settings; INI paths resolve against the file directory. Keep keys outside Git. See [English (US)](../doc/us/CONFIGURATION.md) | [Polski](../doc/pl/CONFIGURATION.md).

Logging: `level=info|warning|error|off`, `connections=on|off` (default off). See [logging](../doc/us/LOGGING.md).

Security limits are in `[security]`: per-IP active/admission quotas, per-connection frame/invalid budgets and absolute TLS/partial-frame timeouts. All have CLI overrides; see [security](../doc/us/SECURITY.md).

Apache2: `apache2-webadmin.conf.example` is the separate loopback HTTP preview. `apache2-webadmin-https.conf.example` adds HTTP→HTTPS, TLS and certificate paths for static assets. Neither sample activates a service or proxies the private API. Setup: [US](../doc/us/APACHE2_HTTPS.md) | [PL](../doc/pl/APACHE2_HTTPS.md).
