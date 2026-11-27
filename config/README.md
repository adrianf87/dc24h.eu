# Configuration

Stage IV public WebAdmin uses a **separate `dc24h-webadmin` account** and
restricted Unix auth bridge, never the serving hub UID for Node. Review
`dc24h-stage4-webadmin.sysusers.example`,
`dc24h-twilight-stage4-bridge.service.example` and
`dc24h-twilight-stage4-webadmin.service.example` together. Nothing is
installed/enabled by the repository. Deployment and rollback checklist:
[US](../doc/us/STAGE_IV_DEDICATED_SERVICE.md) |
[PL](../doc/pl/STAGE_IV_DEDICATED_SERVICE.md).

Copy `dc24h.ini.example` to a local INI, edit settings, then run:

```sh
./build/dc24h-twilight --config config/dc24h.ini --check-config
./build/dc24h-twilight --config config/dc24h.ini
```

Nothing is loaded implicitly. CLI overrides file settings; INI paths resolve against the file directory. Keep keys outside Git. See [English (US)](../doc/us/CONFIGURATION.md) | [Polski](../doc/pl/CONFIGURATION.md).

Logging: `level=info|warning|error|off`, `connections=on|off` (default off). See [logging](../doc/us/LOGGING.md).

Security limits are in `[security]`: per-IP active/admission quotas, per-connection frame/invalid budgets and absolute TLS/partial-frame timeouts. All have CLI overrides; see [security](../doc/us/SECURITY.md).

systemd: `dc24h-twilight.service.example` and `dc24h.sysusers.example` are hub operator examples. `dc24h-twilight-control.service.example` and `dc24h-control.sysusers.example` describe the separate controller and dedicated maintenance peer accounts; `dc24h-twilight-remote-gateway.service.example` is local-only. The old user/group names and data paths intentionally remain stable. `49-dc24h-control.rules.example` is a review-only Polkit rule restricting controller actions to `dc24h-twilight.service`. The new hub unit declares `Conflicts=dc24h.service` and requires a separate `/etc/dc24h/twilight-unit-migration-approved` operator marker; this does not constitute permission to migrate live state. The build does not enable units, create that marker, apply the rule or create accounts. See [Stage IV mapping](../doc/us/TWILIGHT_STAGE_IV.md) | [PL](../doc/pl/TWILIGHT_STAGE_IV.md) and [System security](../doc/us/SYSTEM_SECURITY.md) | [PL](../doc/pl/SYSTEM_SECURITY.md).

Apache2: `apache2-webadmin.conf.example` is the separate loopback HTTP preview. `apache2-webadmin-https.conf.example` adds HTTP→HTTPS, TLS and certificate paths for static assets and still denies the API. `apache2-webadmin-https-proxy.conf.example` proxies an allowlisted API to the loopback adapter and does not contain the service token. No sample activates a service. Setup: [HTTPS US](../doc/us/APACHE2_HTTPS.md) | [HTTPS PL](../doc/pl/APACHE2_HTTPS.md) | [proxy US](../doc/us/APACHE2_PROXY.md) | [proxy PL](../doc/pl/APACHE2_PROXY.md).
