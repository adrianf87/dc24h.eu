# Supplemental Debian 13 runner tests

- `runner_preflight.py` — read-only, bounded host and runner check. Reports no
  raw IP addresses, secrets, private file contents or process command lines.
- `test_runner_contract.py` — unit tests for fail-closed runner selection,
  unknown/active services, and both manual-only workflow contracts.
- `stage2_isolated_proof.py` — synthetic-only backup/restore, injected failure,
  rollback, static systemd contract, and bounded process-exit monitoring.
- `test_stage2_isolated_proof.py` — positive and negative synthetic tests.

Local unit tests (no VPS access required):

```sh
python3 self-hosted-runner-debian13/tests/test_runner_contract.py
python3 self-hosted-runner-debian13/tests/test_stage2_isolated_proof.py
```

The original supplemental smoke runs the source-policy checks without `sudo`.
The proposed separate Stage II workflow uses `sudo` only for read-only root
inventory and ID verification; it never installs OS packages, starts real
hub services, runs host-mutating full CTest, or accesses private state.
