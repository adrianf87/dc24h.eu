# Stage III public ADCS qualification — GitHub Ubuntu only

**Repository:** `adrianf87/dc24h.eu-twilight` · **Issue:** [#10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10)

**Public guest hub:** `adcs://server-747585786.dc24h.eu:1512`, IPv4
`62.171.156.32`. Plaintext ADC TCP 1511 is forbidden, as are public WebAdmin,
database and maintenance ports.

## CI execution model

Only GitHub-hosted Ubuntu 22.04, 24.04 and 26.04 runners are supported.
`server-747585786` is the **public ADCS service host**, not an Actions worker.
The old Debian 13 runner inventory, stage-II sudo scripts, Stage III host
activation/restart workflows and related runner tests were deleted in PR #23.

The surviving workflow
`.github/workflows/debian13-stage3-full-qualification.yml` performs:

| Check | GitHub Ubuntu | Actual public Debian VPS |
| --- | --- | --- |
| Real `ncdc`, two synthetic guests and ADC login | Yes, external test | Receives bounded synthetic guest traffic |
| Inbound/outbound chat, search, CTM/RCM and reconnect | Yes, external test | Must respond as expected |
| CA/SAN/TLS identity, DNS and forbidden public TCP ports | Yes, bounded external probe | No host-local listener inventory |
| `sudo` and systemd tooling sanity | Yes, on ephemeral Ubuntu VM | No sudo or systemd access |
| Three real Stage III service restarts | **Not available through CI** | Requires manual OWNER procedure |
| Same-pair certificate copy/restart, host backup/restore/rollback | **Not available through CI** | Requires manual OWNER procedure |

The workflow runs regression checks on the PR branch and supports an OWNER
`workflow_dispatch` from the exact SHA of the branch or protected `main`, with
explicit confirmation `RUN_STAGE3_UBUNTU_EXTERNAL_ONLY`. That dispatch never
claims host restart or TLS rotation evidence. See the
[pre-merge procedure](PUBLIC_STAGE_III_PREMERGE.md).

## Scope and diagnostics

- The Ubuntu 24.04 external job installs `ncdc` with `sudo apt-get` **inside
  the temporary GitHub VM**. It never executes downloaded PR code as root.
- A separate safe `sudo` contract runs on Ubuntu 22.04, 24.04 and 26.04,
  proving only that the disposable runner offers expected OS tools.
- Tests use fixed synthetic guest markers and bounded timeouts. Published
  diagnostic reports contain stable status flags and frame counts, not raw
  private terminal contents, SIDs, keys or passwords.
- The independent public TCP check probes only the bounded named port set;
  passing it does not certify an exhaustive firewall configuration.

## Acceptance boundary

GitHub CI can qualify the **public external protocol behavior**. It cannot
certify on-host systemd continuity, live private-key operations, actual Debian
backup/restore or rollback once the VPS runner has been removed. Record those
as **NOT TESTED**, not PASS. Keep the PR unmerged until the OWNER reviews all
required evidence and explicitly approves the remaining operational risk.

Do not reintroduce a VPS Actions runner, SSH-based deployment or remote root
credentials in an attempt to bypass the removal decision.
