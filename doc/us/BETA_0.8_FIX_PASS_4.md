# Beta 0.8 Fix Pass 4 — Stage IV review (0.8.13–0.8.16)

Scope: [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), single branch `beta-0.8.x-stage-full`, draft PR #241. This review is a code and local-regression handoff, **not** Stage IV acceptance, production authorization or permission to merge.

## Findings repaired

1. **Optional incident journal generation:** the writer previously compared only SHA-256 content when checking an externally modified path. An identical-content replacement inode could impersonate the original journal. The reader and writer now bind the expected `(device, inode, content digest)` generation. Writer-owned atomic rename advances that identity; an outside same-byte path swap fails closed to unknown, and append will not overwrite the replacement.
2. **Stopped-writer audit snapshot:** the archive previously verified the audit hash chain, closed the verifier, then re-opened the source by pathname. An intervening pathname substitution could cause copying bytes that were never verified. The archive now pins a `O_NOFOLLOW` source descriptor, checks ownership/mode/link count and pathname-to-inode identity, compares snapshots before and after chain verification and archive fsync, and performs a final chain verification before publishing. The source remains unchanged; archive collision and invalid-chain refusal remain mandatory.
3. **Optional telemetry outage:** a throwing optional incident-journal adapter could propagate an exception during an otherwise fresh authenticated status read or audit-loss handling. Journal read/append/unavailable errors now yield `retained: { state: "unknown", events: [] }` without granting authority to the optional journal or masking C++ freshness and audit-failure semantics.

## Regression evidence and retained boundaries

- Local Node targeted suites: **58/58 passed**, covering authenticated freshness, parallel revocation, retained-history privacy, counter and clock resets, near-capacity/exact-cap audit refusal, corruption, archival continuity, matching-content inode replacement, journal outage, and bounded browser parsing.
- `npm run build` (TypeScript/Vite) and `npm run build:remote:ci` (Node gateway bundle) passed locally.
- Production remains **8 MiB fail-closed**, warning at 75%; optional incident history stays at 12 fixed-kind events, 10 minutes and 4 KiB, not part of the five authentication stores or recovery cohort. No raw audit or actor/IP/credential data is projected in EN/PL read-only views.
- Hash-chain validation is not a cryptographic signature against a privileged attacker. The tool **cannot prove a service is stopped**, or protect against concurrent malicious mutation after its final check. Owner must stop all writers; no automatic rotation, chain reset or source deletion is authorized. Real `ENOSPC` and full-scale production retention are not certified by reduced-limit tests.
- Local tests do not replace exact-head GitHub Actions evidence for **Ubuntu 22.04, 24.04 and 26.04**, C++/integration, Playwright, security tooling, recovery and packaging. The PR remains draft and unmerged. Under the Owner's stop-after-trigger instruction, do not poll CI or claim completion while checks are pending.
