# Runner profile — `server-747585786`

| Property | Current information | Verification status |
| --- | --- | --- |
| GitHub repository | `adrianf87/dc24h.eu-twilight` | OWNER-provided URL |
| Runner name | `server-747585786` | OWNER-reported; check GitHub runner details |
| GitHub runner ID | `21` | From OWNER-provided runner settings URL |
| Operating system | Debian 13 (Trixie) VPS | OWNER-reported; host preflight pending |
| Logical CPU threads | 16 | OWNER-reported; host preflight pending |
| Service account | `github-runner` | OWNER-reported; host preflight pending |
| Privileges | Passwordless `sudo` | OWNER-reported; **high-risk**, never used by this workflow |
| Memory, disk, isolation | Not yet independently established | Read-only host preflight pending |
| Public IPv4 / NAT | Unknown for this runner | Never infer from Issue #10's public IP |
| Dedicated runner label | `twilight-debian13-stage2` | **Must be added/confirmed by OWNER** |

This profile contains **no registration token, runner credentials, SSH keys,
server secrets, private addresses, service logs, or private state**.

## Read-only verification

After confirming that the runner is a disposable test host without active hub
services, OWNER can manually launch the supplemental workflow. The job records
only a bounded, redacted JSON summary: Debian release, architecture, logical
CPUs, approved runner identity, basic tool availability, and fixed hub unit
states. No raw IP addresses, environment variables, process command lines,
certificate data, or journal contents are collected.

The workflow's `runs-on` label is an **exact-label requirement**, not proof of
identity. The first job step also verifies `RUNNER_NAME` and the Unix account.
`systemd` and process checks must be clear before even a source-only smoke test.

## Privilege concern

The `github-runner` account has OWNER-reported passwordless sudo. A job that
executes malicious source or dependencies could therefore gain root privileges
even if its workflow does not explicitly use `sudo`. For this reason:

1. Keep the job **manual-only**, OWNER-triggered, and restricted to the reviewed
   default-branch source. Never run untrusted PR/fork payloads on this VPS.
2. Prefer a separate disposable VM with no production secrets or mounted state.
3. Strongly consider removing passwordless sudo or replacing it with narrowly
   scoped, reviewed privileges before broader self-hosted CI use.
4. Do not claim physical isolation or production safety until the host has been
   independently inspected and accepted.

Runner management settings: [GitHub Actions runner #21](https://github.com/adrianf87/dc24h.eu-twilight/settings/actions/runners/21).
