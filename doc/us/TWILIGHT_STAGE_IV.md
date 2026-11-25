# dc24h.eu-twilight — Stage IV: technical identity inventory and OWNER decision

Status: **proposed naming contract only; no migration or activation**. OWNER authorized starting Stage IV on 2026-11-25. This inventory reflects the actual post-Stage III `main`; proposed names below require a **separate explicit naming decision** before implementation.

## Current names → proposed names

| Area | Existing | Proposal / policy |
| --- | --- | --- |
| Product UI and v1 `server.name` | `dc24h.eu-twilight` | **Keep**; completed in Stages II–III. |
| CMake project and executable targets | `project(dc24h)`; `dc24h`, `dc24h-owner`, `dc24h-account-admin`, `dc24h-service-controller` | Keep the internal targets; subject to OWNER approval, output executables as `dc24h-twilight`, `dc24h-twilight-owner`, `dc24h-twilight-account-admin`, `dc24h-twilight-service-controller`. Update all consumers. |
| npm identity | `dc24h-webadmin` in `webadmin/package.json` and two lockfile fields | Proposed `dc24h-twilight-webadmin`, synchronized without dependency updates. |
| Main systemd service | `dc24h.service`; `config/dc24h.service.example` | Proposed `dc24h-twilight.service` only with explicitly tested upgrades, no simultaneous old/new activation, and rollback. |
| systemd controller | `dc24h-control.service` and example; `dc24h-service-controller` | Proposed `dc24h-twilight-control.service` and renamed binary, with an independently checked least-privilege Polkit allowlist and socket. |
| Remote Node gateway service | `dc24h-remote-gateway.service` and example | Proposed `dc24h-twilight-remote-gateway.service`, preserving isolation, Origin, `rpID` and restart semantics. |
| Service users/groups | `dc24h`, `dc24h-control`, `dc24h-maintenance`, `dc24h-admin`; `config/*.sysusers.example` | **Keep** existing identities, UID/GID and permissions absent a separate account-migration decision. |
| Installation/state paths | `/etc/dc24h`, `/var/lib/dc24h`, `/run/dc24h`, `/var/log/dc24h`, `/opt/dc24h/webadmin`, `/usr/local/share/dc24h/webadmin` | **Keep** until a separately authorized backed-up state migration/rollback. Never create two active writers. |
| Internal/protocol identities | `DC24H_*`, C++ `dc24h` namespace and `include/dc24h`, MariaDB `dc24h` schema, ADC/ADCS | **Keep**; branding does not imply protocol, database or API-key migration. |
| CI-only `.deb` | `dc24h-high-starter-ci-fixture` and inert service, in `scripts/beta07_ci_deb.py` | Proposed `dc24h-twilight-ci-fixture`; a different Debian `Package:` is **not** an in-place upgrade. Require dedicated transition, conflict and downgrade tests. |
| Trust and public networking | `dc24h.eu` example hosts/Origin, TLS/certificates, WebAuthn `rpID` | **Keep**; requires separate infrastructure authority. |

## Recommended OWNER choice

1. Adopt `dc24h-twilight` for **new executable and systemd unit names**, retaining stable C++ target/library names, data paths, DB/API names, UID/GID, trust identifiers and domains.
2. Rename the private npm package to `dc24h-twilight-webadmin` in both manifest and lockfile without changing dependency versions.
3. Fail closed if a legacy hub service/process or conflicting state writer exists. Renaming must never broaden Polkit rights or allow the legacy and new services to run together.
4. Test clean install, controlled upgrade, injected failure refusal, actual old/new package distinction, rollback and service hardening on GitHub-hosted Ubuntu 22.04/24.04/26.04 at the exact source SHA.

## Scope and acceptance boundary

- Stage IV kickoff approval does **not** authorize touching production installs, moving private state, reassigning users/groups, changing DNS/TLS/WebAuthn `rpID`/Origin, migrating SQL or activating a public listener.
- The existing `.deb` fixture is explicitly CI-only and inert; its package-name change alone cannot validate production migration. Model a real old/new package transition separately and refuse concurrent service activation.
- Preserve historical source/tag provenance including `v0.9.20-rc.1`.
- Implement the chosen names only after explicit OWNER confirmation. Each migration/failure/rollback invariant needs negative tests, exact-head CI and separate OWNER acceptance before merging; CI is never production GO.
