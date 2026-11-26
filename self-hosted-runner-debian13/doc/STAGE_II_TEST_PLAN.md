# Supplemental Debian 13 runner test plan — Stage II (P0)

**Tracking:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10)
and [Draft PR #13](https://github.com/adrianf87/dc24h.eu-twilight/pull/13).
All acceptance items remain open until independently verified.

| Test area | Supported by this initial runner integration | Remaining acceptance |
| --- | --- | --- |
| Debian 13 inventory | Bounded, unprivileged, read-only host/runner preflight | OWNER-reviewed actual VPS inventory, ownership and NAT evidence |
| C++ and WebAdmin | Source identity and metadata smoke tests only | Reproducible isolated builds and staged installation from exact SHA |
| systemd | Read-only old/new unit-state checks | Disposable VM tests of marker, `Conflicts`, and no two writers |
| Backup and restore | **Not executed on this runner** | Synthetic backup plus verified restore and cold-start/checkpoint |
| Upgrade and rollback | **Not executed on this runner** | Isolated upgrade, injected failure, rollback, ownership/ACL/xattrs |
| Monitoring / RPO/RTO | CPU/tool visibility only | Measured recovery outcomes, limits, and stop criteria |
| OWNER acceptance | Not implied by any runner result | Separate `OWNER STAGE II ACCEPTANCE` and `OWNER MERGE GO` |

## Activation sequence

1. OWNER verifies the runner is non-production and labels only runner #21 with
   `twilight-debian13-stage2`. The presence of passwordless sudo must be
   explicitly risk-reviewed.
2. Review the workflow and tests in Draft PR #13; finish GitHub-hosted Ubuntu
   CI. **Do not merge this draft solely to enable a runner job.**
3. After an independently authorized merge makes the workflow available on
   protected `main`, OWNER may manually select **Debian 13 supplemental smoke**
   in GitHub Actions and enter `RUN_SAFE_TESTS_NO_SUDO`.
4. Confirm the run used the expected exact SHA, runner name, and Debian 13;
   review the sanitized result artifact. Failure or unknown state means NO-GO.
5. Schedule separately approved tests on a **disposable VM/clone** for clean
   C++/WebAdmin install, systemd conflict, backup/restore and actual rollback.

Never point those destructive rehearsals at the live VPS or private state.
No tag, production services, or public listener is authorized by this plan.

## Supplemental Stage II sudo qualification (separate manual workflow)

The OWNER has explicitly authorized using the `github-runner` account and
passwordless `sudo` for Stage II testing. The proposed
`debian13-stage2-sudo.yml` workflow **must first be reviewed, pass hosted CI,
and be merged to protected `main` under a separate OWNER decision**. It has no
PR/push trigger and runs only for the exact reviewed `main` SHA on runner #21.

The workflow explicitly invokes only `sudo -n id -u` and the existing
**read-only** root preflight. This does not stop other subprocesses from
invoking passwordless sudo; only an isolated/disposable host or separately
approved sudoers restriction enforces that boundary. Raw root inventory remains
in runner temporary storage; only a
bounded summary may become a GitHub artifact. Builds and the synthetic recovery
test run as the normal, unprivileged `github-runner` account.

This workflow tests:

1. Root-visible host inventory (without service or path mutation).
2. Exact-SHA C++/WebAdmin builds, a curated **unprivileged** CTest subset,
   `--check-config`, and a CMake install
   into a fresh runner-temporary prefix, never `/usr` or `/usr/local`.
3. **Static** systemd unit contract (Conflicts, OWNER marker and hardening).
4. Synthetic-only backup, restore, injected failure and rollback with matching
   hashes and file modes.
5. A bounded process exit/abnormal-exit monitoring simulation.

This does **not** prove a real systemd service cutover, production checkpoint,
restored MariaDB/private state, power loss, ACL/xattr restoration, or the hub's
RPO/RTO. Full CTest, including tests that use sudo to create real systemd
units or exercise dpkg process integration, is intentionally reserved for a
disposable VM/clone. Those remain open for separate acceptance.
