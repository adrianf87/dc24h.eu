# Roadmap / Plan rozwoju

## Implemented

- alpha-0.1.1 — Core lifecycle, signals, logging.
- alpha-0.1.2 — nonblocking TCP listeners, concurrent peers, limits.
- alpha-0.1.3 — ADC syntax parser and bounded stream decoder.
- alpha-0.1.4 — ADCS TLS inbound transport, credentials, tests.

## Not implemented yet

1. ADC serializer, SID allocation, full client handshake and session state; complete ncdc login.
2. Response queues with bounded output and backpressure; graceful TLS close_notify.
3. File configuration, TLS certificate hot reload and operational telemetry.
4. User authentication, permissions, bans and MariaDB migrations.
5. Chat, search and client transfer coordination.
6. HTTP API and React/TypeScript WebAdmin (served through Apache2).

English TLS details: [us/ADCS_TLS.md](us/ADCS_TLS.md). Polish: [pl/ADCS_TLS.md](pl/ADCS_TLS.md).
