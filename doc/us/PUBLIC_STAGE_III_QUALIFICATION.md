# Stage III — OWNER-only public ADCS acceptance qualification

## Scope and identity

- Repository: `adrianf87/dc24h.eu-twilight`; run the workflow from protected `main` only.
- Public **ADCS/TLS** hub: `adcs://server-747585786.dc24h.eu:1512`, IPv4 `62.171.156.32`.
- Dedicated Debian 13 runner: `server-747585786`, account `github-runner`, with existing passwordless `sudo`. The OWNER accepted the co-location risk.
- No plaintext public ADC on 1511; no public WebAdmin; guest-only and no private/registered-user data.
- The retired Stage III address and hostname must never be used.

## Manual dispatch after a reviewed OWNER-approved merge

Workflow: `.github/workflows/debian13-stage3-full-qualification.yml`.

1. Confirm the public guest hub is already active and management SSH remains available. Verify the OWNER has approved a brief interruption of the live guest service.
2. From protected `main`, select **Run workflow**, set `confirmation=RUN_STAGE3_PUBLIC_QUALIFICATION`, and check `production_restarts_ack`.
3. First, the GitHub-hosted Ubuntu 24.04 runner installs distro-packaged `ncdc` into its temporary environment. It opens a fresh ncdc profile and validates login, chat and search; an independent peer monitors its ADC frames. Two synthetic TLS clients validate two-way public and private chat, broadcast search, direct RES, and CTM/RCM signaling without actually exchanging files. A pre-login chat attempt must be rejected. Three bounded guest reconnections are checked.
   - The initial guest `I40.0.0.0` is normalized by the hub to the actual observed client IPv4. To test TCP-active signaling, the client sends only the incremental `BINF <SID> SUTCP4` update. Never announce the *hub's* public IPv4 as if it were the external client's address; the hub correctly rejects such a mismatch.
   - On failure, the external runner reports the specific step and, for ADC denial, only the numeric `ISTA` status and validated four-letter `FC` command. Raw ADC responses, chat text, session IDs and private tokens must not be logged.
4. The external job also checks DNS A matches the OWNER IPv4, default CA chain and SAN validation, and the bounded forbidden TCP list `1511, 3000, 3306, 5173, 5432, 6379, 8000, 8080, 9090`. This is an authorized *focused* probe, not a 65k-port Internet scan.
5. Only after the external gate passes, the dedicated Debian host job verifies the exact source SHA, OWNER actor, runner/root identity, deployed Stage III manifest and unit, and `ss -H -ltn` listener inventory. Non-loopback TCP listeners must be limited to the documented allowance: SSH 22, existing Apache/ACME 80/443 and guest ADCS 1512. A different actual SSH/Apache port requires explicit policy adjustment before running.
6. The host job runs three separate `systemctl restart dc24h-stage3-adcs.service` cycles, checking a new CA/SAN TLS and ADC handshake after each, then rehearses **same-pair** replacement of only the service-owned certificate/key *copies* and restarts only that service. Existing key/cert bytes are backed up before replacement and restored on error. The Let's Encrypt live/archive contents are never rewritten.
7. A final independent GitHub-hosted job checks public TLS, guest handshake and forbidden ports again. Short, sanitized job summaries are retained as GitHub Actions artifacts for 14 days.

## Trust and limitations

- ncdc's explicitly supplied `?kp=SHA256/...` pin is calculated from a *separately CA/SAN-validated* TLS connection in this same run. Pin validation by ncdc **does not itself prove CA/SAN validation by ncdc**; CA/SAN is separately verified by Python/OpenSSL. Never obtain a pin solely from an unauthenticated URL.
- The TLS rehearsal uses the **same existing certificate/key pair**. It verifies copy permissions and service reload/restart, **not** a Let's Encrypt ACME renewal, replacement with a genuinely new leaf, stale-keypin migration, or auto-renewal. OWNER remains responsible for monitoring expiry and manually updating both service copies before expiry.
- Restart cycles briefly disconnect current guests. No public WebAdmin, DB, maintenance API, SSH or host firewall configuration is modified. The workflow never automatically rolls back the existing deployed service or uninstalls it.
- Only the approved TCP allowlist and inventory are covered, not full upstream firewall/NAT proof; provider ingress rules need independent review. Shared runner root privileges remain an accepted risk.
- The measured public test here is a bounded Stage III functional smoke/stability acceptance; full 20-cycle endurance, physical reboot/power failure, load/DoS, and 24-hour monitoring remain Stage VI.
- Failure is **FIX REQUIRED**; do not mark Stage III accepted from PR CI or a partially completed workflow. Review all three successful jobs, logs, actual source SHA and artifacts before separate OWNER acceptance and Issue #10 update.
