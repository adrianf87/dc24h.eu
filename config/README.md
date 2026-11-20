# Configuration

Copy `dc24h.ini.example` to a local INI, edit settings, then run:

```sh
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini
```

Nothing is loaded implicitly. CLI overrides file settings; INI paths resolve against the file directory. Keep keys outside Git. See [English (US)](../doc/us/CONFIGURATION.md) | [Polski](../doc/pl/CONFIGURATION.md).

Logging: `level=info|warning|error|off`, `connections=on|off` (default off). See [logging](../doc/us/LOGGING.md).

Security limits are in `[security]`: per-IP active/admission quotas, per-connection frame/invalid budgets and absolute TLS/partial-frame timeouts. All have CLI overrides; see [security](../doc/us/SECURITY.md).

systemd: `dc24h.service.example` and `dc24h.sysusers.example` are hub operator examples. `dc24h-control.service.example` and `dc24h-control.sysusers.example` are separate read-only controller examples. The build does not install units or create accounts. See [System security](../doc/us/SYSTEM_SECURITY.md) | [PL](../doc/pl/SYSTEM_SECURITY.md) and [Service controller](../doc/us/SERVICE_CONTROLLER.md) | [PL](../doc/pl/SERVICE_CONTROLLER.md).

Apache2: `apache2-webadmin.conf.example` is the separate loopback HTTP preview. `apache2-webadmin-https.conf.example` adds HTTP→HTTPS, TLS and certificate paths for static assets and still denies the API. `apache2-webadmin-https-proxy.conf.example` proxies an allowlisted API to the loopback adapter and does not contain the service token. No sample activates a service. Setup: [HTTPS US](../doc/us/APACHE2_HTTPS.md) | [HTTPS PL](../doc/pl/APACHE2_HTTPS.md) | [proxy US](../doc/us/APACHE2_PROXY.md) | [proxy PL](../doc/pl/APACHE2_PROXY.md).
