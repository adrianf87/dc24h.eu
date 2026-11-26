# Supplemental Debian 13 runner tests

- `runner_preflight.py` — read-only, bounded host and runner check. Reports no
  raw IP addresses, secrets, private file contents or process command lines.
- `test_runner_contract.py` — unit tests for fail-closed runner selection,
  unknown/active services, and the manual-only workflow contract.

Local unit tests (no VPS access required):

```sh
python3 self-hosted-runner-debian13/tests/test_runner_contract.py
```

The manual workflow runs these tests plus the existing US documentation policy
and Stage II unit regressions. It deliberately **does not** call `sudo`, install
packages, start services, run broad CTest, or access private state.
