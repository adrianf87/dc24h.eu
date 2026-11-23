# Beta 0.4.14 — controlled performance evidence (Stage IV)

This is an **operator-run measurement protocol**, not a published capacity
claim. As of this change, no connection to the requested Debian 13 target
(Intel Xeon D-1521, eight hardware threads, 31,997 MiB RAM) has been made.
All target measurements and acceptance limits remain **NOT MEASURED**.

## Preflight and isolation

1. Schedule a maintenance-only, **disposable** Debian 13 pilot with a known
   configuration, dedicated test accounts/data, TCP/ADCS listeners bound to
   loopback and a separate self-signed or operator-approved localhost CA.
   Never run this against public hubs or real accounts. Do not enable public
   WebAdmin; the private HTTPS gateway remains loopback-only.
2. Record the target OS/kernel, CPU frequency/governor, available RAM,
   MariaDB/Apache2 versions, C++ and Node builds, commit SHA, worker limits,
   listener/certificate settings, and the exact test profile and timing.
3. Test idle baseline, then one bounded workload per profile. Collect
   `systemd-cgtop`, `pidstat`, MariaDB status and network counters separately;
   the probe only snapshots `/proc/PID/status` `VmRSS` before/after (not peak).
4. Keep the session cookie in a transient owner-only `0600` file containing
   exactly `dc24h_remote=<64 lowercase hex chars>` and remove it securely
   after the run. It is never included in the report. Do not commit it.

## Repeatable commands on the **target**

From the repository root, using actual isolated pilot ports and an approved
certificate trust anchor (never `--insecure` or TLS verification disabled):

```sh
python3 scripts/stage4_perf_probe.py --profile adc --port 411 --requests 100 --rate 5 --concurrency 2 --ack-isolated
python3 scripts/stage4_perf_probe.py --profile adcs --port 1511 --ca-file /private/localhost-ca.pem --requests 100 --rate 5 --concurrency 2 --ack-isolated
python3 scripts/stage4_perf_probe.py --profile webadmin --port 8443 --ca-file /private/localhost-ca.pem --requests 100 --rate 5 --concurrency 2 --ack-isolated
python3 scripts/stage4_perf_probe.py --profile api --port 8443 --ca-file /private/localhost-ca.pem --cookie-file /private/session-cookie --requests 50 --rate 2 --concurrency 2 --ack-isolated
```

The probe connects only to localhost/127.0.0.1, never to an arbitrary host.
It refuses to start without the explicit isolation acknowledgment, enforces
at most 1,000 requests, 20 starts/s, eight concurrent workers and five-second
per-operation timeouts, and never sends administrative writes. ADC/ADCS send
only the bounded initial `HSUP ADBASE ADTIGR` and require a complete `ISUP`
advertisement with `ADBASE`; an ADC error/status line does not count as success.
WebAdmin fetches only `/remote.html` with an HTML media type; the API profile
fetches only the authorized read-only `/remote/v1/summary` and requires the
versioned read-only JSON response. A missing, expired or rejected session counts
as an **error**, not a successful request or capacity datapoint.

Each invocation emits JSON with successful throughput, failure rate,
latency percentiles (p50/p95/p99), elapsed time and optional RSS snapshots.
The rate is an *attempted start rate*, not measured sustained throughput.
Archive the output with the release candidate and redacted host observations;
do not include a token, cookie, real actor, IP or private API response.

| Required isolated measurement | Status | Evidence |
|---|---|---|
| ADC protocol startup/response | **NOT MEASURED** | Target operator to attach report |
| ADCS authenticated TLS startup/response | **NOT MEASURED** | Target operator to attach report |
| HTTPS WebAdmin static page | **NOT MEASURED** | Target operator to attach report |
| Authenticated private read-only API | **NOT MEASURED** | Target operator to attach report |
| CPU, RSS, MariaDB load, error rate under workload | **NOT MEASURED** | Target operator to attach observations |

No latency, throughput or resource-use SLO is asserted until the target owner
has measured, reviewed and approved a profile. A microbenchmark or hosted CI
run does **not** substitute for these actual operational measurements.
