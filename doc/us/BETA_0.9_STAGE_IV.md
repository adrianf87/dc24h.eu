# Beta 0.9.13–0.9.16 — Stage IV, private audit reliability and operator visibility

Issue [#242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242), development branch `beta-0.9.x-stage-full`, draft PR #243. **Stage IV is a distinct requested work unit, not acceptance of Stages II/III or a live audit format migration.** Implementation-stage sequencing and cross-stage integration still require a final-head check. This handoff is not permission to merge, tag, release, deploy, reset the audit or alter production private stores.

## 0.9.13 — proposed stopped-writer audit continuity (design only)

The current `remote-audit-store.ts` is limited to **8 MiB**, uses sequence numbers beginning at 1 and an unkeyed SHA-256 previous/digest chain. `remote-audit-archive.ts` preserves a verified copy without changing the source. It **cannot** safely start a new chain continuing the prior sequence, recover capacity or prove an operator actually stopped the writer. These fail-closed semantics are retained without change.

Candidate workflow, **blocked until a separate explicit Owner decision**:

1. Stop and independently verify **all** Node audit writers and protected service units; hold an exclusive maintenance lock, require matching private five-store/MariaDB recovery generation and a free preflight disk budget.
2. Pin and verify the exact source inode, entire chain, length and digest; write an immutable private archive under a unique name, fsync archive and its directory, retain the original and verify both again. A missing/altered archive aborts rollover.
3. Have a detached Owner-held key authenticate a checkpoint covering audit file digest, end sequence/tail, generation, predecessor checkpoint digest and key identifier. Commit an independent monotonic anti-reuse ledger **before** any new writer is allowed to authenticate.
4. Only after atomic, crash-recoverable metadata publication and explicit Owner go/no-go, create a versioned new audit file whose **first sequence and previous digest** continue the archived tail. Reject a lost archive, reused generation, partial checkpoint, wrong key, symlink, existing output or failed fsync. Recover by refusing authentication and restoring a matched checkpoint+archive+ledger; never initialize an empty log silently.

The existing production writer does not implement steps 3–4. In-memory `webadmin/tests/beta09-audit-continuity.ts` is a **test-only proposed verifier**, with no CLI, file writes, remote route or production import. Its two-segment sequence and previous-checkpoint linkage tests illustrate the intended invariant; they do **not** provide an approved migration or crash-atomic transaction.

## 0.9.14 — candidate authenticity and key lifecycle (design only)

The prototype uses an HMAC-SHA256 checkpoint bound to the complete archive SHA-256, chain tail, contiguous sequence, predecessor checkpoint and monotonically increasing generation. It rejects same-length changes to an otherwise internally recomputed unkeyed chain, missing/replaced archives, wrong key and replay against an independently supplied minimum generation. Tests use only a synthetic disposable 32-byte key.

**Owner decisions required before enabling:** independent key custody and rotation/offline recovery, hardware or separately sealed generation ledger and anti-rollback anchoring, machine restart consistency, audit/recovery cohort pairing, versioned v1→v2 checkpoint migration and rollback, exact privilege and stop-window protocol, key-loss refusal and storage volume capacity. An HMAC is **not public-key nonrepudiation**, and anyone with the key can forge it; a ledger stored beside the files alone cannot defeat a privileged offline rollback. None of these mechanisms is enabled in production.

## 0.9.15 — disposable host storage fault qualification

Added GitHub-hosted Ubuntu-only workflow `beta09-stage4-storage.yml` on **22.04/24.04/26.04**, using an exact PR-head SHA checkout, Node 22 and no raw report artifact uploads. It exercises:

| Fault | Negative proof / limitation |
| --- | --- |
| Actual kernel `ENOSPC` | Fill a uniquely mounted **4 MiB tmpfs** in CI, confirm syscall error, refuse audit append and lock the active writer. Cannot certify production quota/RPO. |
| `fsync` returns `EIO` | Compile a test-only `LD_PRELOAD` shim targeting exactly one disposable audit pathname; refuse success and all retries. Page-cache bytes after an `EIO` are **not** proof of durable audit. |
| `SIGKILL` after successful `fsync` | Crash disposable child without closing the descriptor; restart parses and continues a valid chain. Does not simulate a kill *during* the fsync or prove systemd/dpkg transition. |
| Unsafe modes, symlink/path swap, exact size cap, corrupted record | Existing audited regressions rerun; no silent reset or unverified recovery. |

The workflow isolates the mount inside `$RUNNER_TEMP`, checks a sentinel and Linux tmpfs magic before filling it, unmounts on exit and uploads **no private log/key**. If a runner disallows the mount, the job fails visibly; it must **not** be recorded as a passed ENOSPC test. Local runs without a disposable tmpfs intentionally **skip** that case.

## 0.9.16 — EN/PL source freshness and private incident UX

Existing gateway, incident-journal, parser and security-monitor regressions are rerun. A strengthened Playwright case starts with an older C++ source age plus a **retained, not-live** incident and then hangs refresh: at the 45-second total-age cutoff the UI hides **both** current counters and retained incident region in EN/PL, announces expiry, and never presents stale evidence as healthy. Keyboard-only local acknowledgment remains local; no new POST, public listener, grant or remote hub control is introduced.

## Status and evidence limits

Local Node, TypeScript and no-mount faults are runnable without credentials. **Actual ENOSPC and browser execution require GitHub-hosted runner evidence on an exact SHA**, plus integration with unfinished Stage II/III changes. Required full C++/WebAdmin/Chromium, VM/systemd, recovery/dpkg, sanitizers/security, and final-head workflow status cannot be inferred from this patch or a pending CI run. Do not mark accepted while checks are missing, skipped, failed or for the wrong commit. Per Owner: after committing and triggering CI, **stop without polling or waiting**.
