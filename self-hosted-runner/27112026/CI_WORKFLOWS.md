# CI workflow migration — 27 November 2026

**Branch:** `feature/stage4-webadmin-dedicated-service` in `adrianf87/dc24h.eu-twilight`.

The OWNER confirmed that `server-747585786` is a **test VPS** and approved executing the hub, WebAdmin, build scripts, and sudo-enabled CI tests using the `github-runner` account. No additional OWNER confirmation gate was introduced for automatic pull-request CI. The public ADCS and WebAdmin services share this Debian 13 machine, so CI build and package steps execute on that same host.

## Migrated workflow inventory

| Workflow file | Runner / OS after migration | Notes |
| --- | --- | --- |
| `beta07-codeql.yml` | `[self-hosted, Linux, X64]`, Debian 13 | Exact-head CodeQL and package prerequisites |
| `beta08-hardening.yml` | `[self-hosted, Linux, X64]`, Debian 13 | TSAN and parser fuzz |
| `beta09-stage4-storage.yml` | `[self-hosted, Linux, X64]`, Debian 13 | Stage IV offline audit and fault tests |
| `build.yml` | `[self-hosted, Linux, X64]`, Debian 13 | C++, WebAdmin and regression checks |
| `debian13-stage3-full-qualification.yml` | `[self-hosted, Linux, X64]`, Debian 13 | Manual Stage III extended/public tests; existing workflow approval remains |
| `public-stage3-qualification.yml` | `[self-hosted, Linux, X64]`, Debian 13 | Public ADCS qualification, some jobs require `main` |
| `public-stage4-design.yml` | `[self-hosted, Linux, X64]`, Debian 13 | Stage IV service design, database/SMTP fixtures, browser tests |
| `stage4-ubuntu-vm-validation.yml` | `[self-hosted, Linux, X64]`, Debian 13 | Retained historical workflow filename; renamed display to **Stage IV Debian 13 self-hosted qualification** |

All eight workflow YAML files use the self-hosted labels; none route a job to Ubuntu 22.04/24.04/26.04 or Debian 12. Debian 13-specific installation uses `self-hosted-runner/scripts/install-debian13-ci-deps.sh`. Identity checks reject execution on the wrong host, OS, or account.

The Stage IV VM workflow has been replaced by a Debian 13 host job. It builds the C++ targets, runs CTest, Node 24 WebAdmin checks, Playwright and read-only host probes. **Destructive dpkg downgrades, full host installation/rollback and historical Ubuntu performance comparisons are NOT TESTED in that job**, not counted as PASS.

## Verification snapshot

- **PASS:** parsed all eight YAML workflows; runner routing consistently Debian 13 self-hosted.
- **PASS:** Bash syntax for the Debian 13 package helper; Python syntax for migrated policy/evidence files.
- **PASS:** 12 Beta06/Beta07/Beta09 policy unit tests and 29 Stage III qualification unit tests in a local fixture (41/41).
- **PENDING:** actual GitHub Actions on registered Debian 13 runner. The most recent observed run was queued/pending on 27 November 2026, without final CI conclusions. See [Actions](https://github.com/adrianf87/dc24h.eu-twilight/actions).
- **NOT VERIFIED:** exact registration labels, online status, full on-host dependency/test suite and public-service stability after real runner jobs.

## Operational notes

- All modifications are in `feature/stage4-webadmin-dedicated-service`; `main` and active VPS services were not modified by this migration work.
- Some workflow triggers are unchanged: Stage III public checks that require `main` still skip on pull-request runs. A skipped job is not PASS.
- Automatic pull-request CI is enabled under the OWNER's explicit test-host authorization. Because `github-runner` has passwordless `sudo`, checked-out PR code and dependencies can have host-root impact. This is an accepted test-VPS tradeoff, not an assertion of isolation.
- Do not commit runner credentials, SMTP secrets, TLS private keys, database passwords or dumps.
