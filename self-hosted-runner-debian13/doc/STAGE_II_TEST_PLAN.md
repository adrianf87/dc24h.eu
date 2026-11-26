# Supplemental Debian 13 runner test plan — Stage II (P0)

**Tracking:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10)
and [Stage II workflow update PR #15](https://github.com/adrianf87/dc24h.eu-twilight/pull/15).
The new host qualification is proposed, not yet executed or accepted.

| Test area | Proposed manual Debian 13 qualification | Remaining acceptance |
| --- | --- | --- |
| Debian 13 inventory | Bounded, unprivileged, read-only host/runner preflight | OWNER-reviewed actual VPS inventory, ownership and NAT evidence |
| C++ and WebAdmin | Exact-SHA builds, temp staging and real root-owned install to unique `/opt` prefix | Production installer/package layout and actual deployment acceptance |
| systemd | Real test-owned service, crash/restart and test-unit `Conflicts`; shipped template static contract | Production unit marker and old/new cutover are deliberately untested |
| Backup and restore | Test-only fixture SHA-256 + modes verified across backup, corruption and restore | Private MariaDB/checkpoint, ACL/xattr and real state recovery not tested |
| Upgrade and rollback | Corrupt installed test binary, restore byte-exact known-good executable | Historical-version package rollback and DB schema compatibility not tested |
| Monitoring / RPO/RTO | Test unit journal, observed crash restart and cleanup | Real hub RPO/RTO and power-loss behavior untested |
| OWNER acceptance | Not implied by any runner result | Separate `OWNER STAGE II ACCEPTANCE` and `OWNER MERGE GO` |

## Activation sequence

1. OWNER verifies the runner is non-production and labels only runner #21 with
   `twilight-debian13-stage2`. The presence of passwordless sudo must be
   explicitly risk-reviewed.
2. Review the new privileged test scope in PR #15 and the GitHub-hosted Ubuntu
   CI. Keep protected `main` unchanged until a new OWNER merge decision.
3. After the independently approved merge, OWNER manually selects **Debian 13
   Stage II full sudo host qualification** on `main` and enters the new
   confirmation `RUN_STAGE2_SUDO_HOST_TESTS`.
4. Confirm the run used the expected exact SHA, runner name, and Debian 13;
   review the sanitized result artifact. Failure or unknown state means NO-GO.
5. Inspect `/opt`/runtime-unit cleanup evidence. Any failure or leftover
   test-only service requires manual OWNER review before another run.

The OWNER confirmed that this Debian 13 VPS is disposable, test-only and has
no production secrets/private state. No real hub unit, existing data, tag,
production deployment or public listener is authorized by this plan.

## Supplemental Stage II sudo qualification (separate manual workflow)

The OWNER explicitly retained passwordless sudo for `github-runner` on the
approved test VPS. The updated `debian13-stage2-sudo.yml` workflow must be
reviewed, pass hosted CI and be merged to protected `main` under a new OWNER
decision. It has no PR/push trigger and runs only against the reviewed `main`
SHA on runner #21.

The workflow explicitly calls `sudo -n` to verify identity, run **read-only**
inventory, and run `stage2_host_proof.py` as root. It does not limit the OS
sudoers configuration. Raw root inventory remains private; only bounded
redacted summaries become artifacts. C++/WebAdmin builds and synthetic checks
still run as the normal unprivileged `github-runner` account.

This workflow tests:

1. Root-visible host inventory (without service or path mutation).
2. Exact-SHA C++/WebAdmin builds, curated unprivileged CTest, `--check-config`,
   temporary staging and an additional **root-owned /opt** install; never
   `/usr` or `/usr/local`.
3. **Real** test-only systemd unit start/restart, SIGKILL recovery, test-unit
   `Conflicts`, journal observation and shipped-unit static contract.
4. Test-owned fixture backup, restore, injected corruption and byte-exact
   installed-binary rollback; synthetic recovery tests still run separately.
5. Actual CTest `systemd.sandbox` with sudo, then checked cleanup of test units
   and the unique `/opt` prefix.

This does **not** prove production service cutover, a previous-version package
rollback, MariaDB/private-state restoration, power-loss safety, ACL/xattr
restoration or real hub RPO/RTO. Broad CTest and dpkg/package-manager changes
remain excluded. Those gaps require separate OWNER acceptance.
