# Roadmap

| Stage | Implemented scope |
| --- | --- |
| alpha-0.1.1 Core | Lifecycle, start/stop, synchronous system signals, logging |
| alpha-0.1.2 Network | Nonblocking TCP, IPv4/IPv6, multiple listeners and clients, resource limits |
| alpha-0.1.3 ADC | Bounded stream syntax parser, commands and data validation |
| alpha-0.1.4 ADCS | TLS credentials, nonblocking encrypted inbound transport, tests, US/PL documentation and operator messages |
| alpha-0.1.5 Configuration | INI, CLI overrides, path/limit validation and append-file logging |
| alpha-0.1.6 Logging | Startup summaries, WARN, typed connection events, bounded UTF-8 and rate budgets |
| alpha-0.1.7 Security | Bounded IP admission, frame quotas and absolute deadlines |
| alpha-0.1.8 Database | Optional local MariaDB connection, prepared queries and explicit migration metadata |
| alpha-0.1.9 API | Local authenticated HTTP/JSON status and health; read-only |
| alpha-0.1.10 WebAdmin | React/TypeScript/Vite read-only layout, EN/PL, local development adapter, static Apache example |

Next increments must define their version and acceptance tests before implementation:

1. ADC serializer, bounded output/backpressure, SID allocation and session state machine; complete ncdc login.
2. Graceful TLS close_notify, certificate reload, telemetry and asynchronous/rotating logs.
3. Users, Argon2id authentication, roles/bans, application SQL migrations and verified remote database TLS.
4. Public/private chat, search and client transfer coordination.
5. Administrative API mutations/roles/audit, authenticated Apache2 gateway and production web sessions/roles.

Versions beyond alpha-0.1.10 remain planned. Other branches do not establish release readiness.

## alpha-0.1.5

Implemented: explicit INI configuration, CLI overrides, listener/limit/path validation, check-only mode, append-file logging and severity selection. Later remote branch versions are outside this task.
