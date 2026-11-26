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
