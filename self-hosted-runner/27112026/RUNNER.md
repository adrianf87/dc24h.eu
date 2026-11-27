# GitHub Actions runner — November 27, 2026

| Property | Value |
| --- | --- |
| Runner | `server-747585786` |
| GitHub settings | [Runner ID 22 for dc24h.eu-twilight](https://github.com/adrianf87/dc24h.eu-twilight/settings/actions/runners/22) |
| Repository | Private `adrianf87/dc24h.eu-twilight` |
| Operating system | Debian GNU/Linux 13 (Trixie), x86_64 |
| Execution account | `github-runner` |
| Privilege | Passwordless `sudo` (effectively root-capable) |
| Host classification | **OWNER-approved test VPS** running publicly reachable test ADCS and WebAdmin |

**Not yet independently confirmed:** GitHub runner registration labels, online/offline status, binary version, installation path and exact `systemd` service unit. The OWNER provided the registration UI URL and confirmed the system account; do not infer the unknown fields.

## Inspect registration without exposing credentials

```bash
id github-runner
sudo -n -u github-runner id
systemctl list-units --type=service --all 'actions.runner.*'
systemctl list-unit-files 'actions.runner.*'
```

The service path and version should be documented **only after reading non-secret outputs**. Do not copy `.credentials`, `.credentials_rsaparams`, registration tokens, GitHub PATs or full process environments.

## Debian 13-only CI configuration

The OWNER approved **automatic self-hosted Debian 13 CI**, including builds, CTest, ncdc, database fixtures, WebAdmin, public test-hub checks, and `sudo` package installation, on this shared test VPS. No new OWNER approval gate was added for normal pull-request CI.

All eight project workflows now route via `runs-on: [self-hosted, Linux, X64]` with a Debian 13/hostname/user verification step. Debian 12 and Ubuntu 22.04/24.04/26.04 are no longer runner targets on this feature branch. The `main` branch was not modified; results on the real runner are pending. See [CI_WORKFLOWS.md](CI_WORKFLOWS.md).

Because this is a shared host and the CI account has unrestricted `sudo`, test jobs can modify system packages or services. This is an explicit OWNER acceptance of risk, **not** a claim that CI is isolated. The current Stage IV workflow intentionally avoids overwriting the installed WebAdmin while testing fixtures; its full host rollback proof remains NOT TESTED.

Use sanitized inventory reports. Never commit credentials, TLS private keys, MariaDB secrets, SMTP passwords, or session data.

## Change tracking

On each approved runner/tooling change, add a dated note and an updated sanitized inventory under `self-hosted-runner/DDMMYYYY/`. Include OS/runner/tool versions, packages added or removed and which service was affected. Preserve old snapshots as historical evidence.
