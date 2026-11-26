# Runner profile — `server-747585786`

| Property | Current information | Verification status |
| --- | --- | --- |
| GitHub repository | `adrianf87/dc24h.eu-twilight` | OWNER-provided URL |
| Runner name | `server-747585786` | Verified in GitHub settings and read-only run #45757153126 |
| GitHub runner ID | `21` | From OWNER-provided runner settings URL |
| Operating system | Debian 13 (Trixie) VPS | Verified by read-only runner preflight |
| Logical CPU threads | 16 | Verified by read-only runner preflight |
| Service account | `github-runner` | Verified by read-only runner preflight |
| Privileges | Passwordless `sudo` | OWNER-confirmed test VPS; root inventory passed in workflow run #45763395514 |
| Memory, disk, isolation | Not yet independently established | Read-only host preflight pending |
| Public IPv4 / NAT | Unknown for this runner | Never infer from Issue #10's public IP |
| Dedicated runner label | `twilight-debian13-stage2` | Verified in GitHub runner settings |

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
3. The OWNER explicitly authorized a broader Stage II test-only host rehearsal
   while preserving the runner account's existing unrestricted sudo. The
   reviewed workflow calls `sudo -n` for identity, root inventory and an
   exact-SHA, disposable `/opt` + runtime systemd unit proof. It still avoids
   installing packages or touching production service/data paths.
4. Do not claim physical isolation or production safety until the host has been
   independently inspected and accepted.

Runner management settings: [GitHub Actions runner #21](https://github.com/adrianf87/dc24h.eu-twilight/settings/actions/runners/21).
