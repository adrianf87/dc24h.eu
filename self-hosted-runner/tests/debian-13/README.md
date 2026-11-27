# Debian 13 test snapshot — November 27, 2026

This directory contains a **complete copy of the 267 file entries from repository-root `tests/`** at source commit `15bec73f3ca947b3168f8ddc5e83dfc20f9bbbe2`. It includes C++, Python, fixture JSON, text fuzz corpora and manifest data. The source `tests/` files remain in the repository for compatibility with CMake and tooling.

**13 environment-specific Python files have been ported for Debian GNU/Linux 13, a self-hosted GitHub runner and the OWNER-approved test VPS**. All other files are copied byte-for-byte, including historical Ubuntu *negative fixtures and regression evidence* that must not be silently rewritten into fake Debian provenance. See [SOURCE_MANIFEST.tsv](SOURCE_MANIFEST.tsv) for an exact original blob inventory and ported-file markers.

## How the CI uses these tests

Every workflow job with `actions/checkout` runs:

```bash
bash self-hosted-runner/scripts/activate-debian13-tests.sh
```

This validates Debian 13 and overlays this snapshot onto the job's ephemeral checkout `tests/` **before** any CMake/CTest/Python/WebAdmin test code runs. The original tracked tests in Git are unchanged. It refuses to continue when new root tests are missing from this snapshot; sync them when new tests are added.

The snapshot is **not an independent Debian distribution package**: fixtures also load the project's `scripts/`, `config/`, C++ and WebAdmin sources. Keep those modules current. OS-specific historical performance and artifact policy fixtures may still describe Ubuntu on purpose; this is test input, not a Debian runtime claim.

## Test acceptance

- All 267 files were copied from recorded blob SHAs; no test disappeared.
- The 13 environment-specific copies had their Python syntax checked.
- All eight workflow YAML files parsed and activate this snapshot after checkout.
- No real Debian 13 GitHub runner run has yet passed after this change. Cancellation requested for existing queued/pending runs per OWNER.
- **NOT TESTED**: a Debian 13 historical performance baseline; the ported Stage IV loopback probe reports the unavailable comparison as `NOT_TESTED`, never a fabricated PASS.
- No running public ADCS/WebAdmin service was modified by creating this snapshot.

To use the snapshot manually from the project checkout on Debian 13, run the activation script then invoke the desired CTest or Python commands. It overlays only `tests/`; it does not run system services itself.
