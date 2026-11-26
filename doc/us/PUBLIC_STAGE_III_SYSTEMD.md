# Optional Debian 13 ADCS service maintenance and recovery runbook

**Operational reference only — EXCLUDED FROM STAGE III QUALIFICATION by OWNER decision, November 26, 2026.** No controlled VPS reboot or intentional live `dc24h-stage3-adcs.service` restart test will be run for Issue #10 or PR #24. Neither the missing test result nor the readiness prerequisites below block Stage III acceptance; live recovery behavior remains unverified. This document is retained only for future separately authorized maintenance (such as certificate renewal) or incident recovery.

If such future maintenance is separately approved, the OWNER must authorize a window and identify the operator and rollback owner. Use two SSH sessions from an approved management network, verify provider-console access, and arrange an independent external observer. Stop if preflight fails or the rollback operator cannot reach the VPS.

## Before the window

From an independent network, confirm TCP 1512 accepts TLS with hostname and public CA validation. Open two guest sessions. Exchange a synthetic public chat marker in both directions and record the UTC timestamp, client versions, and test result. Keep one session open so its disconnect and recovery can be observed.

```bash
stage3_name=server-747585786.dc24h.eu
openssl s_client -connect 62.171.156.32:1512 -servername "$stage3_name" \
  -verify_hostname "$stage3_name" -verify_return_error -brief </dev/null
```

In an approved SSH session on the selected Debian 13 VPS, inspect the focused service. Check for unexpected drop-ins, a deleted running binary, a nonmatching UID, concurrent maintenance or a second hub. Review complete command output privately.

```bash
stage3_unit=dc24h-stage3-adcs.service
cat /etc/os-release
systemctl is-active "$stage3_unit"
systemctl show "$stage3_unit" -p Id -p FragmentPath -p DropInPaths -p User -p Group \
  -p ExecStart -p MainPID -p ActiveEnterTimestamp -p NRestarts -p Result
systemctl cat "$stage3_unit"
stage3_pid=$(systemctl show "$stage3_unit" -p MainPID --value)
stage3_binary=$(sudo -n readlink -e "/proc/$stage3_pid/exe")
printf '%s\n' "$stage3_binary"
sudo -n sha256sum "/proc/$stage3_pid/exe" "$stage3_binary"
sudo -n cat /opt/dc24h-stage3/manifest.json
sudo -n journalctl -u "$stage3_unit" --since '15 minutes ago' -n 80 --no-pager
```

Confirm the deployment manifest identifies the expected source SHA, service and endpoint. Compare the live binary hash with the reviewed deployment record and the copy at the resolved executable path. The deployed source and current qualification branch may have different SHAs. Identify both in the evidence.

Privately audit the live config for `listen=none`, the approved ADCS bind, and disabled database, account authentication and internal API. Review the Debian listening sockets and firewall together with the provider ingress policy, including direct address assignment or the approved NAT mapping. Verify SSH is reachable and restricted as approved. Confirm any HTTPS or ACME exception and inspect the relevant virtual host routing for WebAdmin or internal API exposure.

Securely identify the known-good systemd fragment, configuration, binary and certificate pair before the window. Confirm the operator can restore them if needed. A failed restart can normally be recovered with the same unchanged artifacts and a verified service start.

## Future separately authorized service maintenance only — not a qualification test

Once the OWNER authorizes the interruption and both observers are ready, record the baseline PID and start timestamp. Restart only the approved unit. Monitor runtime state and the bounded journal.

```bash
stage3_window=$(date -u '+%Y-%m-%d %H:%M:%S UTC')
sudo -n systemctl restart "$stage3_unit"
systemctl is-active "$stage3_unit"
systemctl show "$stage3_unit" -p MainPID -p ActiveEnterTimestamp -p NRestarts -p Result
sudo -n journalctl -u "$stage3_unit" --since "$stage3_window" -n 80 --no-pager
date -u +%FT%TZ
```

Confirm a fresh active timestamp and process identity. Recheck the binary hash and SSH from the second session. The external observer repeats public TLS verification, reconnects the original guest, logs in with a second guest, and verifies attributed chat in both directions. Record recovery duration. Use the OWNER before/after external snapshot for exact-commit evidence when available.

Stop and begin recovery if the service fails to return to `active` within 30 seconds, restarts repeatedly, fails TLS or guest chat verification, or if there is any unexpected listener or SSH failure. Stop further retries after one controlled recovery attempt. If the unit is unhealthy, collect `systemctl show` and the bounded journal, confirm the exact deployed artifacts and validate the service configuration under its account.

```bash
# Use the previously verified absolute executable path from the running identity.
sudo -n runuser -u dc24h-stage3 -- "$stage3_binary" \
  --config /etc/dc24h-stage3/dc24h.ini --check-config
```

If the verified configuration and deployed artifacts match the preflight, the authorized operator may recover this unit with `sudo -n systemctl reset-failed "$stage3_unit"` followed by `sudo -n systemctl start "$stage3_unit"` when it is inactive or failed. Restore a known-good artifact only within the OWNER-approved recovery scope and while the affected unit is stopped. Use the certificate runbook for TLS pair restoration. Repeat the active, TLS, guest chat and SSH checks. Escalate through the provider console if SSH is lost. Obtain separate authorization for wider recovery.

## Sanitized evidence record

| Field | Value |
| --- | --- |
| OWNER authorization reference, operator, UTC window | |
| Qualification SHA and running deployment SHA | |
| Running binary version and SHA-256 before and after | |
| Unit name, fragment checked, approved drop-ins and service identity | |
| Provider/Debian routing, listener exposure and ingress policy disposition | |
| SSH and console availability before and after | |
| Pre/post active state, distinct PID attestation and UTC transition times | |
| Independent pre/post TLS, guest reconnect and two-way chat results | |
| Public certificate fingerprint and expiration | |
| Recovery duration, sanitized journal event categories and rollback disposition | |
| Sanitized evidence locations, SHA-256 manifest and OWNER reviewer | |
| Stage III restart qualification | REMOVED BY OWNER; NOT RUN |
