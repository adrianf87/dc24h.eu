# Public deployment Stage II — Debian 13 installation and recovery (P0)

**Tracking:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), Stage II.  
**Status:** engineering kickoff and read-only host-inventory tooling. **Stage II acceptance pending.**  
**Boundary:** no live installation, systemd activation, private-state migration, DNS/firewall changes, public hub or WebAdmin exposure, release or production GO.

**Supplemental runner:** The OWNER has designated a Debian 13 VPS runner for
future manual, non-production source and environment checks. See the
[runner profile and safety policy](../../self-hosted-runner-debian13/README.md).
The source-smoke workflow is OWNER-triggered and available on protected `main`.
Its first read-only run qualified the runner identity and basic environment,
but it does not replace installation, systemd, backup/restore, or rollback proof.

The OWNER subsequently authorized controlled passwordless `sudo` for the
`github-runner` account. A separate, proposed manual Stage II qualification
workflow uses it **only** for read-only privileged inventory; C++/WebAdmin
builds, temporary-prefix installation, synthetic backup/restore/rollback, and
monitoring checks remain unprivileged. It must pass review and be merged to
`main` before dispatch. Real service activation/conflict, private-state
recovery, power-loss and production RPO/RTO tests remain outside this VPS job.

Stage I's isolated Debian 13/ncdc evidence and OWNER acceptance are recorded in [the acceptance report](../../logs/tests/DEBIAN13-STAGE-I-FINAL-ACCEPTANCE.md). Stage II is a **separate** operating-system installation and recovery qualification; green CI and an inert `.deb` fixture do not certify an existing Debian host.

## 1. Safe host inventory: the first deliverable

`scripts/public_stage2_preflight.py` is **read-only** and prints a bounded JSON document to stdout. Run it only on a host the OWNER has authorized for inventory, with a pinned, previously reviewed checkout. Complete process and metadata visibility requires root privileges; non-root inventory fails closed:

```sh
stage2_sha="<exact-40-character-review-sha>"
test "$(git rev-parse HEAD)" = "$stage2_sha" || exit 2
sudo python3 scripts/public_stage2_preflight.py --source-dir . --expected-sha "$stage2_sha"
```

The script does **not** write files, install packages, start/stop services, query private databases, read secrets, create approval markers or change listeners. It captures:

- Source SHA, Debian 13 identification, kernel/architecture, CPU count, PID 1 and systemd manager availability.
- Installed tool **presence**, rather than complete package inventories or environment variables.
- `dc24h.service` and `dc24h-twilight.service` activation status, process counts by fixed executable name and the state of the explicit OWNER marker (never its content).
- `lstat`-only type, mode and UID/GID of the **fixed** legacy/new installation paths; symbolic links are flagged and never followed.
- Network address-family counts only (no IP values or raw interfaces). External address/NAT ownership **cannot** be established by the script.
- Exact source unit `Conflicts`, OWNER approval-marker guard and WebAdmin v1 `server.name` identity.

Exit code **0** means inventory is collected and contains no known blockers (`INVENTORY_CLEAR`); it is **not an installation or activation GO**. Exit code **2** and `BLOCKED` mean a missing precondition/unknown service state. Insufficient privileges, unreadable marker/path metadata or a transitional unit state also block the result. The report's `production_go`, `private_state_migration_go` and `public_listener_go` fields are **always `false`**. Redact and review the report before uploading; avoid publishing UID/GID values if your disclosure policy disallows them.

The known public IPv4 `51.255.80.233` can be a routed/NAT address; do not bind to it without an independent network/topology review. The host inventory is not a port scan and must not be presented as proof of Internet reachability.

## 2. Repeatable, isolated build/staged installation (not yet accepted)

On a **disposable Debian 13 VM/clone**, separately from a running hub, use the reviewed SHA and a newly created temporary directory. Example for a source checkout that has already been verified:

```sh
stage2_root="$(mktemp -d -t twilight-stage2-XXXXXXXX)"
umask 022
cmake -S . -B "$stage2_root/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$stage2_root/prefix" \
  -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build "$stage2_root/build" --parallel 2
ctest --test-dir "$stage2_root/build" --output-on-failure
"$stage2_root/build/dc24h-twilight" --version
"$stage2_root/build/dc24h-twilight" --config config/dc24h.ini.example --check-config
cmake --install "$stage2_root/build"
sha256sum "$stage2_root/build/dc24h-twilight" \
  "$stage2_root/prefix/bin/dc24h-twilight"
```

Check that **both hashes match** and the installed WebAdmin status parser requires exactly `server.name = dc24h.eu-twilight` (the internal binary and C++ targets have intentionally different names). If building WebAdmin, use the lockfile with Node.js 22+, `npm ci`, `npm run build`, and `DC24H_INSTALL_WEBADMIN=ON` on the **disposable checkout** only; do not publish the generated assets. The example above does not enable WebAdmin installation by default.

**Do not** run `cmake --install` with `/usr` or `/usr/local` as the prefix on the existing VPS, and do not run a real `dpkg -i` for the CI-only packages. The current Debian `.deb` is an inert fixture, **not** a production installer. A repeatable production package/install and controlled upgrades remain Stage II engineering tasks.

## 3. Safe systemd upgrade/rollback design (not yet executed)

- Preserve existing `dc24h` user/group and stable `/etc/dc24h`, `/var/lib/dc24h`, `/run/dc24h`, `/var/log/dc24h` paths. Never create a second writer or silently change permissions.
- Verify the **shipped** `dc24h-twilight.service` with `systemd-analyze verify` using a temporary copy. It must keep `Conflicts=dc24h.service` and `AssertPathExists=/etc/dc24h/twilight-unit-migration-approved`.
- Do **not** create the marker, enable/start either real unit or stop an existing unit. A systemd rehearsal must use a clean, isolated OS/VM and synthetic names until a separate OWNER-approved cutover plan is in place.
- On an isolated clone with **synthetic** data, record original file hashes, UID/GID, modes, ACL/xattrs, baseline services and database version. Take and **restore-test** a backup before attempting upgrade.
- Require a cold-start/checkpoint preflight and fail-fast validation of missing/incorrect keys, missing DB, stale schema, unit conflicts, wrong ownership and interrupted install. Do not assume atomic v1→v2 private-state migration or rollback is implemented.
- Inject a failure between staging and activation and prove that the old binary/service and state remain recoverable; test rollback after a successful synthetic upgrade. Record measured RPO/RTO and that no two hub services were active together.

## 4. Monitoring and stop criteria

Record synthetic/test-only CPU/RSS, FD/socket count, restart rates, journal warnings/errors and backend readiness, keeping passwords, tokens, private keys, actual state contents and user identifiers out of published logs. **STOP / NO-GO** on unknown live services, schema/key mismatch, failed restore, permission drift, concurrent old/new units, undocumented skipped tests, non-zero unresolved failures or any unreviewed change outside the isolated prefix.

## 5. Completion requirements

Stage II remains **open** until the following are independently demonstrated on Debian 13 with exact SHA and sanitized SHA-256 evidence:

1. Actual OWNER-reviewed VPS inventory (not guessed from a previously shared IP); no production mutation.
2. Reproducible C++ and WebAdmin build, staged clean installation and `--check-config`, exact binary/API/status identity.
3. Isolated real systemd installation rehearsal with old/new unit conflict and explicit marker refusal/acceptance (synthetic marker in a disposable OS only).
4. Private synthetic backup, **verified restore**, cold start, controlled upgrade, injected failure, fail-fast and actual rollback preserving ownership/ACLs.
5. Monitoring, failure/stop criteria and measured recovery outcomes.
6. Exact-head GitHub-hosted CI, negative tests and separate `OWNER STAGE II ACCEPTANCE` and `OWNER MERGE GO`.

These conditions do **not** grant production migration, public hub activation or WebAdmin Internet exposure; those require separate OWNER decisions under [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10).
