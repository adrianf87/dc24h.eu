# GitHub Actions runner — November 27, 2026

| Property | Value |
| --- | --- |
| Runner | `server-747585786` |
| GitHub settings | [Runner ID 22 for dc24h.eu-twilight](https://github.com/adrianf87/dc24h.eu-twilight/settings/actions/runners/22) |
| Repository | Private `adrianf87/dc24h.eu-twilight` |
| Operating system | Debian GNU/Linux 13 (Trixie), x86_64 |
| Execution account | `github-runner` |
| Privilege | Passwordless `sudo` (effectively root-capable) |
| Host classification | VPS with live public ADCS and HTTPS WebAdmin; **not disposable** |

**Not yet independently confirmed:** GitHub runner registration labels, online/offline status, binary version, installation path and exact `systemd` service unit. The OWNER provided the registration UI URL and confirmed the system account; do not infer the unknown fields.

## Inspect registration without exposing credentials

```bash
id github-runner
sudo -n -u github-runner id
systemctl list-units --type=service --all 'actions.runner.*'
systemctl list-unit-files 'actions.runner.*'
```

The service path and version should be documented **only after reading non-secret outputs**. Do not copy `.credentials`, `.credentials_rsaparams`, registration tokens, GitHub PATs or full process environments.

## Debian 13-only CI policy

OWNER has withdrawn Ubuntu 22.04, Ubuntu 24.04, Ubuntu 26.04 and Debian 12 from desired future tests. Test matrix and required-check migration in `.github/workflows/` must still be implemented safely and verified; merely recording the runner does not change workflow scheduling.

Because code built by the runner can reach unrestricted `sudo` and live service files:

1. Do not schedule automatic checkout/build/dependency-install of untrusted pull requests on this VPS.
2. Restrict high-privilege host tests to explicitly approved OWNER-only manual workflow runs and a fixed, reviewed commit.
3. Separate read-only checks from host installs, network changes, `systemd` restarts and restore/rollback.
4. Use synthetic fixtures. Never overwrite production database state or private keys.
5. Do not confuse skipped/not-run tests with PASS.
6. Do not expose the runner socket, Docker socket, or host secret directories to build containers.

## Change tracking

On each approved runner/tooling change, add a dated note and an updated sanitized inventory under `self-hosted-runner/DDMMYYYY/`. Include OS/runner/tool versions, packages added or removed and which service was affected. Preserve old snapshots as historical evidence.
