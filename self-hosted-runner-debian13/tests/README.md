# Supplemental Debian 13 runner tests

- `runner_preflight.py` — read-only, bounded host and runner check. Reports no
  raw IP addresses, secrets, private file contents or process command lines.
- `test_runner_contract.py` — unit tests for fail-closed runner selection,
  unknown/active services, and both manual-only workflow contracts.
- `stage2_isolated_proof.py` — synthetic-only backup/restore, injected failure,
  rollback, static systemd contract, and bounded process-exit monitoring.
- `test_stage2_isolated_proof.py` — positive and negative synthetic tests.
- `stage2_host_proof.py` — explicit OWNER-only root execution on the approved
  Debian 13 **test VPS**. It installs into a fresh system-owned `/opt` prefix,
  installs two uniquely named runtime systemd units, verifies the actual hub
  start/restart/crash recovery and test-unit conflict, observes the journal,
  then verifies fixture backup/restore and installed binary rollback. It stops
  and removes its units and `/opt` directory even after test failure.
- `test_stage2_host_proof.py` — host-proof guards and fixture regressions that
  never require `sudo` or modify the actual machine.

Local unit tests (no VPS access required):

```sh
python3 self-hosted-runner-debian13/tests/test_runner_contract.py
python3 self-hosted-runner-debian13/tests/test_stage2_isolated_proof.py
python3 self-hosted-runner-debian13/tests/test_stage2_host_proof.py
```

The original supplemental smoke runs the source-policy checks without `sudo`.
The manual Stage II workflow uses `sudo -n` for identity checking, read-only
inventory and the dedicated host-proof script. It also runs the real
`systemd.sandbox` CTest on the test VPS. No OS package is installed, no real
`dc24h.service` or `dc24h-twilight.service` is enabled, and no pre-existing
private data is accessed. The systemd units bind only inside a private network
namespace. An interrupted/failed run may require OWNER inspection of test-only
remnants; absence of evidence is never treated as PASS.
