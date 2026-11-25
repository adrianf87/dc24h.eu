# dc24h.eu-twilight — Stage V: documentation, GitHub, provenance and communications

**Status: bilingual documentation and GitHub settings verified on 2026-11-25; final CI and formal OWNER acceptance pending.** This is the **P1 documentation** stage; it does not change runtime version, API contracts or private state. [Issue #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) · [Draft PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9). Stage VI **remains a release NO-GO**.

## Current versus historical facts

| Subject | Actual present state | Provenance and limit |
| --- | --- | --- |
| Name and repository | [`adrianf87/dc24h.eu-twilight`](https://github.com/adrianf87/dc24h.eu-twilight) | Independent development repository, protected default `main`. |
| Runtime source | `beta-0.9.20` forked from High Starter RC1 | Historic [`v0.9.20-rc.1`](https://github.com/adrianf87/dc24h.eu-high-starter/releases/tag/v0.9.20-rc.1), source commit `e4fcb8b5849d4afa4671f97a2ee9c3e63b085a18`. |
| Original OWNER RC acceptance | Yes — **High Starter only, for non-production evaluation only** | Original merged [PR #243](https://github.com/adrianf87/dc24h.eu-high-starter/pull/243) + [#244](https://github.com/adrianf87/dc24h.eu-high-starter/pull/244) + [#245](https://github.com/adrianf87/dc24h.eu-high-starter/pull/245). Preserve authentic source references. |
| Twilight Stages I–IV | Merged in protected Twilight `main` | [Twilight PRs #2–#7](https://github.com/adrianf87/dc24h.eu-twilight/pulls?q=is%3Apr+is%3Aclosed). Name-migration rollback CI was **synthetic**, not an existing-host migration. |
| Twilight Stages V–VI | Under review together in [Draft PR #9](https://github.com/adrianf87/dc24h.eu-twilight/pull/9) | Stage V: documentation and GitHub settings verified; final CI and OWNER acceptance pending. Stage VI: release NO-GO. |

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

## GitHub About and settings — verified 2026-11-25

Settings **are not versioned in this PR**. They were inspected in the live GitHub UI, rather than inferred from the PR:

- **Description (saved and verified):** `dc24h.eu-twilight — independent C++20 ADC/ADCS hub and React WebAdmin development fork; non-production release qualification.`
- **Topics (saved and verified):** `dc24h`, `twilight`, `direct-connect`, `adc`, `adcs`, `cpp20`, `react`, `webadmin`, `linux`.
- **Website URL:** empty; no unapproved DNS/TLS change.
- **Branch protection:** default `main`, **Main Branch Protection** ruleset **Active**, empty bypass list; pull request, fresh branch and 12 named status checks required; force pushes and branch deletion blocked.
- **GitHub App ChatGPT Codex Connector:** selected three repositories (`dc24h.eu-high-starter`, `dc24h.eu`, `dc24h.eu-twilight`); reads checks, commit statuses and metadata; writes Actions, code, issues, pull requests and workflows.
- **GitHub App Cursor:** has access to **All repositories** (existing and future); reads administration, commit statuses, deployments, metadata, packages and pages; writes Actions, checks, code, discussions, issues, merge queues, pull requests and workflows. **This is broad access that deserves a conscious OWNER review of possible restriction**; no app permissions were changed.
- **Repository copy boundaries:** Git mirrors do not transfer GitHub Issues, PRs, Releases, secrets, permissions or repository settings. Twilight's new issue/PR history starts with [Issue #1](https://github.com/adrianf87/dc24h.eu-twilight/issues/1) and PRs #2–#9; original evidence stays in High Starter.

## Stage V acceptance checklist

1. **1,498/1,498 relative Markdown file links** in active documentation resolved; documentation negatives run. Final-head PR #9 CI must still complete; this audit does not certify every external HTTP URL.
2. Live GitHub About/topics/ruleset/app scopes reviewed. Cursor's unusually broad access is recorded for OWNER assessment; repository prose cannot prove live Settings.
3. OWNER reviews bilingual wording, unchanged historic links/changelog/license and the explicit compatibility/migration exceptions. **A separate merge authorization** is required; it does not authorize tagging or production.

**Current decision:** documentation and About configuration work done; **formal acceptance awaits final CI and OWNER**. **Release and production NO-GO.**
