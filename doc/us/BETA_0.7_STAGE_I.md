# Beta 0.7.x — Stage 1 implementation and acceptance ledger

Issue [#238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), single development branch `beta-0.7.x-stage-full`. This is work in progress, **not** an approved release, completed Stage 1, or permission to merge or deploy. Stage 2 is one later consolidated 20-patch review/fix pass. The release version stays `beta-0.6.16` until the explicit `beta-0.7.20` metadata patch.

## Baseline and boundaries

- Branch base: protected `main` at `620026216f2f7e19cb55d832b0c91aa26f94d8b2` (merged beta-0.6.x PR #237).
- C++20 remains the password/grant authority. WebAdmin is an authenticated, loopback-only, read-only projection behind private Apache TLS; never passwordless. MariaDB schema v11 and `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json` and `rate-limits.json` remain coupled for stopped-writer recovery.
- Only disposable, synthetic GitHub-hosted Ubuntu 22.04/24.04/26.04 evidence counts. No Debian, privately managed runner, Xeon or production access. Preserve owner-only private state and redacted diagnostics.

## Ordered implementation ledger

| Patch | Scope | State | Acceptance/evidence |
| --- | --- | --- | --- |
| 0.7.1 | Independent ADC client evidence | Implementation committed; hosted proof pending | `scripts/beta07_client_evidence.py`, negative `tests/beta07_client_evidence_tests.py`, hosted `cpp` CTest/apt package checksum; DC++ and AirDC++ explicitly untested |
| 0.7.2 | Identity and negotiation | New negative TCP/OpenSSL regressions; hosted proof pending | Real SUP/SID/INF and password tests remain in `adc.authentication.process`/ncdc suite; added Unicode/rejected duplicate SUP, pre-auth MSG and no-hash cases in `adc.login.process` |
| 0.7.3 | Search and peer transfer | Expanded independent ncdc capability gate; hosted proof pending | Eight more ncdc negative search/transfer cases; `adc.sch.process`, `adc.ctm.process`, `adc.rcm.process` cover IPv4/IPv6 and invalid/replayed frames |
| 0.7.4 | ADCS TLS trust/downgrade | Version-specific TLS trust regression added; hosted proof pending | `adcs.tls_security.process` now covers both TLS 1.2 and 1.3 wrong-host rejection and valid trusted sessions; retained expired/mismatched-key/cipher/downgrade/cutover regressions |
| 0.7.5 | Browser WebAuthn restored login | Isolated real Chromium ceremony implemented; full C++/gateway and matched SQL/five-file restore pending | `webadmin/tests/beta07-webauthn-restore.spec.ts` performs real registration, fresh owner-only passkey catalog reopening, signature verification, replay, wrong-origin and revoked-key refusals; it **does not** substitute for a full password+C++ authorization or matched six-component restore |
| 0.7.6 | Multiple passkeys and revoke | Multi-key file-backed regression added; hosted proof and real second authenticator pending | `remote-passkeys.test.ts` checks two actor-owned keys and a separate account, monotone counters, backup flags, rename, one-key and actor-wide revocation across restart; browser-signature and audited logout proof still required |
| 0.7.7 | TOTP rotation and replay | Recovery integrity regression added; hosted proof and matched SQL/five-store capture pending | `remote-mfa.test.ts` rejects clock rollback, partial append and rotation with damaged replay, then tests byte-exact offline replay preservation, new seed proof and old-factor refusal |
| 0.7.8 | Concurrent sessions and CSRF | Cross-tab invalidation implemented; hosted browser proof pending | `RemoteApp.tsx` uses a fixed non-sensitive BroadcastChannel signal on confirmed logout/passkey revocation; browser test asserts sibling-tab metrics and actions vanish; further C++ grant-race checks pending |
| 0.7.9 | Matched SQL/private snapshot | Existing six-component hash pairing reused, new mismatch regression; actual stopped-writer coordinator pending | `stage1_recovery_bundle.py` cryptographically pairs SQL and five-store manifest; new `beta07_recovery_preflight_tests.py` rejects mismatched commit/generation and incomplete private state; independent writer-quiescence proof still required |
| 0.7.10 | Non-destructive preflight | Read-only fresh-target checker added; SQL-v11 interpretation and hosted proof pending | `beta07_recovery_preflight.py` checks exact commit and six hashes, owner-only parents, existing/dangling symlink and overlapping SQL/private targets **without writing**; explicitly says it has not checked imported schema or stopped writers |
| 0.7.11 | Disposable .deb prototype | Pending | Test-only dpkg install/upgrade/unit ownership |
| 0.7.12 | Real dpkg downgrade | Pending | Old binary plus matched SQL/private rollback |
| 0.7.13 | Trusted read freshness | Pending | Versioned C++ observation/receipt-age contract |
| 0.7.14 | Redacted event timeline | Pending | Aggregate bounded stream, no actor/IP identities |
| 0.7.15 | Aggregate security alerts | Pending | Threshold/cooldown, privacy and outage bound |
| 0.7.16 | Accessible EN/PL workflow | Pending | Keyboard, mobile, local-only ack and privilege tests |
| 0.7.17 | Comparable multi-transport benchmarks | Pending | 3 windows, SHA and p50/p95/p99/RPS/CPU/RSS |
| 0.7.18 | Exhaustion and drain | Pending | Bounded TLS/queue/timeout/fault + ASan/UBSan |
| 0.7.19 | Supply chain and provenance | Pending | Reviewable pins/SBOM/advisories/exact-head checks |
| 0.7.20 | Release qualification | Pending | Consistent versions, changelog, risk and final hosted CI |

## Patch 0.7.1 evidence semantics

The independent ncdc binary is executed by existing ADC/ADCS CTest process cases. The added gate requires **all eighteen named cases to appear and pass in the actual CTest JUnit result**; absent, skipped, duplicated or failed tests invalidate the proof. The expanded matrix includes token scope, search/transfer authorization refusals, transfer rate limits and error recovery, not just guest login. It also checks an exact upstream Ubuntu package version per hosted OS, `dpkg --verify` for installed files, SHA-256 of the downloaded exact-version `.deb` against the authenticated apt package index, and records the installed executable's SHA-256. This is a bounded reproducibility check, not certification of an external product.

Pinned `ncdc` Ubuntu packages (amd64): jammy 22.04 `1.22.1-3`, noble 24.04 `1.23.1-1build2`, resolute 26.04 `1.23.1-1.1`. Changes in Ubuntu metadata must be reviewed rather than accepted as compatible by default. The report never copies raw client traffic, login names, passwords, addresses or complete log text. DC++ and AirDC++ rows remain `untested`, because their native binaries are not executed in the permitted hosted fixtures.

## Stage 1 stop/go

**NO-GO** while any ledger row, required synthetic real-process test, numeric performance/recovery evidence or same-head hosted CI is missing. Do not check Issue #238 Stage 1, mark the draft PR ready, merge, close the issue or deploy. Record the final exact head SHA and all Ubuntu/CodeQL/browser/sanitizer/systemd/ADC/ADCS/recovery/dpkg evidence before declaring this work unit complete.
