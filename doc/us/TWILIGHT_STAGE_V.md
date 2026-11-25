# dc24h.eu-twilight — Stage V: documentation, GitHub, provenance and communications

**Status: proposed in Draft PR #9, pending hosted CI and separate OWNER acceptance.** This is the **P1 documentation** stage; it does not change runtime version, API contracts or private state. [Issue #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) · [PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9). Stage VI **remains a release NO-GO**.

## Current versus historical facts

| Subject | Actual present state | Provenance and limit |
| --- | --- | --- |
| Name and repository | [`adrianf87/dc24h.eu-twilight`](https://github.com/adrianf87/dc24h.eu-twilight) | Independent development repository, protected default `main`. |
| Runtime source | `beta-0.9.20` forked from High Starter RC1 | Historic [`v0.9.20-rc.1`](https://github.com/adrianf87/dc24h.eu-high-starter/releases/tag/v0.9.20-rc.1), source commit `e4fcb8b5849d4afa4671f97a2ee9c3e63b085a18`. |
| Original OWNER RC acceptance | Yes — **High Starter only, for non-production evaluation only** | Original merged [PR #243](https://github.com/adrianf87/dc24h.eu-high-starter/pull/243) + [#244](https://github.com/adrianf87/dc24h.eu-high-starter/pull/244) + [#245](https://github.com/adrianf87/dc24h.eu-high-starter/pull/245). Preserve authentic source references. |
| Twilight Stages I–IV | Merged in protected Twilight `main` | [Twilight PRs #2–#7](https://github.com/adrianf87/dc24h.eu-twilight/pulls?q=is%3Apr+is%3Aclosed). Name-migration rollback CI was **synthetic**, not an existing-host migration. |
| Twilight Stages V–VI | Under review together in [Draft PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9) | Independent exact-head CI, documentation acceptance and separate OWNER release decision remain pending. |

## Current documentation and operator entry points

- [Repository README](../../README.md): current branding, nested Beta/Alpha historical release menus, current-`main` badge, five Twilight Actions workflow links and release boundaries.
- [Installation EN](INSTALLATION.md) / [PL](../pl/INSTALLATION.md): clone **Twilight**, enter `dc24h.eu-twilight`, invoke `./build/dc24h-twilight` and bind only loopback `127.0.0.1`.
- [Configuration](CONFIGURATION.md), [API](API.md), [database migrations](DATABASE_MIGRATIONS.md), [WebAdmin](WEBADMIN.md), [TLS](ADCS_TLS.md) and the [doc index](../README.md) reference current artifact names wherever they describe current code. Schema/data migration still requires separate authority.
- [Twilight GitHub Actions](https://github.com/adrianf87/dc24h.eu-twilight/actions) and [CI guide EN](GITHUB_CI.md) / [PL](../pl/GITHUB_CI.md): no source-repository CI links as current evidence. A green `main` badge is **not** final-head acceptance of PR #9.
- [Stage IV mapping and rollback EN](TWILIGHT_STAGE_IV.md) / [PL](../pl/TWILIGHT_STAGE_IV.md) and [Stage VI qualification EN](TWILIGHT_STAGE_VI.md) / [PL](../pl/TWILIGHT_STAGE_VI.md).

## Explicit exceptions: preserve the truth

1. **Source history:** historical Alpha/Beta entries, old „Draft PR #243” statements, authentic [High Starter](https://github.com/adrianf87/dc24h.eu-high-starter) issue/PR/CI links and old changelog sections retain their **original point-in-time meaning**. New provenance notes and a dated new changelog entry distinguish them from current Twilight status. Do not fabricate Twilight versions of historic URLs.
2. **Stable technical identifiers:** `namespace dc24h`, internal CMake target names, `DC24H_*` keys, API paths, MariaDB names and ADC/ADCS protocol fields are not branding. Renaming them requires a separately reviewed data/protocol migration.
3. **Infrastructure:** the `dc24h.eu` example host, TLS, WebAuthn `rpID`/`Origin`, service user `dc24h`, `/etc/dc24h` and `/var/lib/dc24h` remain unchanged. The visible `rpName` and strict v1 `server.name` were separately approved, with no old-name fallback.
4. **`LICENSE`, Git history and signed commits:** untouched. Historic `v0.9.20-rc.1` is neither moved nor claimed as a Twilight release.

## GitHub About and settings — manual review gate

Settings **are not versioned in this PR**, and the PR's existence cannot certify them. OWNER or another authorized reviewer must confirm on GitHub:

- **Read-only audit 2026-11-25:** current GitHub About still reads `Coded by ChatGPT (High reasoning level). A Direct Connect hub server supporting the ADC and ADCS network protocols.`; no topics are displayed. Updating this **remains an external settings task**, not a branch commit.
- **Verified live settings:** default branch `main`; **Main Branch Protection** ruleset **Active**, targeting `main`, no bypass entries, PR required, fresh branch required, 12 named status checks required, force pushes and branch deletion blocked. Installed apps listed as **ChatGPT Codex Connector** and **Cursor**. Checking each app's detailed permission scopes led to GitHub's additional account confirmation (sudo mode), so those scopes **remain unverified**; no access barrier was bypassed.
- **Proposed description:** `dc24h.eu-twilight — independent C++20 ADC/ADCS hub and React WebAdmin development fork; non-production release qualification.`
- **Proposed topics:** `dc24h`, `twilight`, `direct-connect`, `adc`, `adcs`, `cpp20`, `react`, `webadmin`, `linux`.
- **Website URL:** leave empty unless a real publicly approved site exists; a display rebrand does not relocate `dc24h.eu` DNS/TLS.
- **Default branch:** `main` (confirmed from GitHub repository metadata). Review its live rulesets and status checks, bypass policy, and GitHub App permissions in Settings before acceptance.
- **Repository copy boundaries:** Git mirrors do not transfer GitHub Issues, PRs, Releases, secrets, permissions or repository settings. Twilight's new issue/PR history starts with [Issue #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) and PRs #2–#9; original evidence stays in High Starter.

## Stage V acceptance checklist

1. All final-SHA PR #9 checks and a review of current links; documentation negatives must refuse old clone/binary instructions and stale current-status wording.
2. Manually confirm GitHub About/topics/rulesets/permissions, recording any follow-up action needed. Repository prose cannot prove live Settings.
3. OWNER reviews bilingual wording, unchanged historic links/changelog/license and the explicit compatibility/migration exceptions. **A separate merge authorization** is required; it does not authorize tagging or production.

**Current decision:** documentation ready for review, no formal Stage V acceptance yet. **Release and production NO-GO.**
