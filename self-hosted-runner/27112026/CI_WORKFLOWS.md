# CI workflow audit — 27 November 2026

**Inspected branch:** `feature/stage4-webadmin-dedicated-service` in `adrianf87/dc24h.eu-twilight`. Inspected the eight `.github/workflows/` files read-only after creating this runner documentation.

| Workflow file | Runner declaration(s) seen | Debian 13 migration |
| --- | --- | --- |
| `beta07-codeql.yml` | `${{ matrix.os }}` (Ubuntu matrix) | Pending |
| `beta08-hardening.yml` | `ubuntu-24.04` | Pending |
| `beta09-stage4-storage.yml` | `${{ matrix.os }}` (Ubuntu matrix) | Pending |
| `build.yml` | `${{ matrix.os }}` and `ubuntu-24.04` | Pending |
| `debian13-stage3-full-qualification.yml` | `ubuntu-24.04` | Pending |
| `public-stage3-qualification.yml` | `${{ matrix.os }}` and `ubuntu-24.04` | Pending |
| `public-stage4-design.yml` | `${{ matrix.os }}` and `ubuntu-24.04` | Pending |
| `stage4-ubuntu-vm-validation.yml` | `${{ matrix.os }}` | Pending |

**OWNER's desired CI policy:** future tests on **Debian 13 only**, not Ubuntu 22.04/24.04/26.04 or Debian 12. The current file state has **not yet been migrated**. This documentation change does not modify workflow triggers, required branch checks or job scheduling.

## Safe next steps

1. Confirm the actual registered label set and online status of runner [#22](https://github.com/adrianf87/dc24h.eu-twilight/settings/actions/runners/22) without disclosing credentials.
2. Classify jobs by trust boundary: pull-request code, trusted builds, privileged systemd/host tests, public integration tests.
3. Prevent automatic PR code and package install scripts from gaining host-root via the `github-runner` account's passwordless `sudo` on this public service machine.
4. Prepare a separate reviewed workflow change; route approved manual privileged tests to the exact Debian 13 runner and commit.
5. Verify Node.js compatibility for WebAdmin (runner had Node.js **20.19.2**, npm **9.2.0** at snapshot).
6. Coordinate required check names and test status reporting. Skipped jobs are not qualification PASS.

**Result:** documentation/audit only. No CI workflow dispatch or live host change was performed by this directory addition.
