# Monitoring — alpha-0.9.11

CPU, memory, sessions and latency. The branch starts from main `402fa10`. The branch name is `alpha-0.9.11`.

The hub publishes one sample on the existing bearer-only `GET /api/v1/status`. Health is unchanged. There is no new route, INI key, schema migration, Apache setting or WebAdmin page. The existing dashboard shows the sample when the object is present.

## Process

The sample reads only `/proc/self/stat` on the API thread. It does not read the command line, the environment, another process or a cgroup.

`cpu_user_ms` and `cpu_system_ms` are cumulative user and system time in milliseconds. Child processes are not included. `rss_bytes` is the resident page count times the page size. It is not a limit and it is not a leak detector.

`cpu_window_ms` is the wall time since the previous successful sample. `cpu_delta_ms` is the user plus system time consumed in that window. The first sample, a failed read and two reads in the same millisecond report a zero window. Several threads can make the delta larger than the window. This is not host CPU.

A failed read sets `available` to false and the process numbers to zero. The hub does not log the record.

## Sessions

The object copies the current phase gauges `sessions_accepted`, `sessions_negotiating`, `sessions_identified`, `sessions_ready`, `sessions_closing` and `sessions_closed`, plus `sessions_created` and `sessions_completed`. `sessions_ready` is the same kind of count as `users.online`, sampled on its own, so the two numbers can differ. No nickname, SID, CID, address or account is included.

## Latency

`login_last_ms` and `login_max_ms` measure steady-clock time from session accept to the ready transition. `login_samples` counts those transitions. A failed or repeated ready call is not a sample. This is not a client round trip.

`reactor_last_ms` and `reactor_max_ms` measure one pass of reactor work between waits, in whole milliseconds. `reactor_samples` counts those passes. The wait itself is not included. A slow disk or a large accept batch can raise the maximum. This is not a per-user delay and it is not stored.

## Limits

Samples are independent. Nothing is written to MariaDB, to a file or to a remote collector. There is no Prometheus endpoint, no history and no per-session latency list. Schema v9 is unchanged. `Require local` stays. Status and health stay bearer-only. This does not close residual threat T13.

`monitoring.unit` checks the stat record, tick and page conversion, overflow and a live process sample. `users.statistics` checks accept-to-ready samples. `api.process` checks the status object.
