# Beta 0.7.x — Fix Pass 3: offline recovery and disposable package rollback (0.7.9–0.7.12)

**Disposition:** Stage III code review and local regressions performed; exact-final-head GitHub-hosted Ubuntu CI must be checked separately. **Not accepted; NO-GO for merge, release or deployment.** One work unit only, on `beta-0.7.x-stage-full` and draft PR #239 under Issue #238.

## Confirmed defects and corrections

1. `stage4_private_recovery_drill.private_directory()` previously used `lstat` only on the final directory. An intermediate symlink was followed, so a private-looking leaf directory could pass source or restore-target preflight through an aliased ancestor. The revised check inspects **every path component** with `lstat` and refuses symlinked or non-directory ancestors; no target is created on refusal.
2. `read_private()` previously checked only the final file size after reading. In-place rewrites of equal length, or replacement of the opened file path, could evade this read-level check. It now checks the original file descriptor's size, modification/change timestamps and the path's device/inode identity after reading; the higher-level manifest and paired SHA-256 checks remain mandatory. A stopped writer is still essential: no filesystem-only check makes an online six-component snapshot atomic.

New negative tests cover an intermediate symlink on a private source, an aliased fresh restore target, and a same-size write during private-file reading. All refuse without creating a restore destination.

## Four-patch review ledger

| Patch | Reviewed test/code surface | Disposition and limits |
| --- | --- | --- |
| **0.7.9** Offline SQL + five stores | `stage1_recovery_bundle.py`, `stage4_private_recovery_drill.py`, `stage1_recovery_bundle_tests.py`: six source hashes, paired manifest, owner-only mode, incomplete and changed copies. **Fixed intermediate symlink acceptance and read-time same-size mutation.** Quiescence is an operator assertion, not cryptographically proven |
| **0.7.10** Non-overwriting preflight | `beta07_recovery_preflight.py`, `beta07_recovery_preflight_tests.py`: target isolation, wrong SHA, missing/partial or cross-generation files, migration row shape and redacted failures. **Added aliased-parent negative regression.** Preflight is read-only; only actual import and C++ database-status on fresh MariaDB can validate live schema semantics |
| **0.7.11** Disposable `.deb` | `beta07_ci_deb.py`, `beta07_ci_deb_tests.py`: fixed test-only package name/version, exact executable provenance, inert systemd unit, no normal maintainer scripts, archive overwrite refusal. No further confirmed defect in this local review; this is not a production package |
| **0.7.12** Upgrade failure and downgrade | `beta07_dpkg_process_tests.py`: previous/current binary hash and versions, genuine `dpkg` preinst refusal, corrupt archive, package status after failure, previous-version reinstallation and matched v11/five-file restore after MariaDB restart. Hosted Ubuntu VM execution is required; it is not a live-service or arbitrary post-install failure simulation |

## Tests and handoff boundary

- Locally run unit suites: `stage4_private_recovery_tests.py`, `beta07_recovery_preflight_tests.py`, `stage1_recovery_bundle_tests.py`, and `beta07_ci_deb_tests.py`; also run `git diff --check`. See the final PR/Issue handoff for exact counts and source SHA.
- The disposable MariaDB/dpkg executable transitions, C++/WebAdmin/Chromium, CodeQL, sanitizers, ADC/ADCS/TLS and transport benchmark matrix require **new exact-final-head runs on GitHub-hosted Ubuntu 22.04/24.04/26.04**. An earlier successful commit is not evidence of this fix commit; incomplete, failed, skipped-required or pending checks block acceptance.
- Never publish the `.deb`, upload private SQL/five-file fixtures, use a self-hosted runner or Debian/Xeon measurement, merge the draft PR, or deploy. After push and CI launch, **stop without waiting for results**. Fix Pass 4 requires a separate request.
