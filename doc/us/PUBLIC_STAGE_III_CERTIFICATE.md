# Stage III manual Let's Encrypt certificate replacement and rollback

**Status: live certificate replacement NOT PERFORMED.** Synthetic certificate verification and pair rollback are exercised separately on GitHub-hosted Ubuntu. The OWNER has removed intentional live VPS/service restart testing from Stage III qualification (November 26, 2026). This procedure is for future independently authorized certificate maintenance only and is not a Stage III acceptance test. For such maintenance, the OWNER must explicitly authorize live replacement, any required service interruption and a rollback window. Verify SSH, provider-console and operator availability, and the preflight checks in [the optional service maintenance runbook](PUBLIC_STAGE_III_SYSTEMD.md) first.

The service reads `/etc/dc24h-stage3/tls/fullchain.pem` and `/etc/dc24h-stage3/tls/privkey.pem`. Certbot and Let's Encrypt control their own live and archive directories. Obtain the renewed pair through the OWNER's authorized renewal process, and use its resolved, trusted source files as read-only input for this procedure. Keep shell tracing disabled and use the private Debian operator session for filesystem work.

## Inspect and prepare

Run the following in the authorized operator's root shell on Debian 13. Use the executable path verified in the systemd preflight. Review `stat` locally and confirm ordinary, non-symlink destination files, root ownership, the `dc24h-stage3` group, mode `0640`, and a root-owned group-readable TLS directory with mode `0750`. Stop on a mismatch.

```bash
set -o pipefail
set +x
umask 077
stage3_unit=dc24h-stage3-adcs.service
stage3_name=server-747585786.dc24h.eu
stage3_dir=/etc/dc24h-stage3/tls
stage3_cert=$stage3_dir/fullchain.pem
stage3_key=$stage3_dir/privkey.pem

stat -c '%F %U:%G %a %n' "$stage3_dir" "$stage3_cert" "$stage3_key"
check_stage3_paths() {
  local file
  test -d "$stage3_dir" && test ! -L "$stage3_dir" || return 1
  test "$(stat -c '%U:%G:%a' "$stage3_dir")" = 'root:dc24h-stage3:750' || return 1
  for file in "$stage3_cert" "$stage3_key"; do
    test -f "$file" && test ! -L "$file" || return 1
    test "$(stat -c '%U:%G:%a' "$file")" = 'root:dc24h-stage3:640' || return 1
  done
}
check_stage3_paths
openssl x509 -in "$stage3_cert" -noout -issuer -dates -ext subjectAltName -fingerprint -sha256
openssl x509 -in "$stage3_cert" -noout -checkend $((30 * 86400))
```

A failed 30-day check means renewal is due. The rollback pair must still be valid for the maintenance window. If the current pair is already invalid or does not match the live endpoint fingerprint, stop and obtain incident recovery authorization.

Define a pair validator. Both key commands hash only the derived public key. Review the proposed certificate's public metadata and verify it changes from the current leaf as intended.

```bash
validate_stage3_pair() {
  local leaf=$1 key=$2 cert_digest key_digest
  openssl x509 -in "$leaf" -noout -ext subjectAltName |
    tail -n +2 | tr ',' '\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' |
    grep -Fx "DNS:$stage3_name" >/dev/null || return 1
  openssl x509 -in "$leaf" -noout -issuer -nameopt RFC2253 |
    grep -E "(^issuer=|,)O=Let's Encrypt(,|$)" >/dev/null || return 1
  openssl verify -purpose sslserver -verify_hostname "$stage3_name" \
    -CAfile /etc/ssl/certs/ca-certificates.crt -untrusted "$leaf" "$leaf" || return 1
  cert_digest=$(openssl x509 -in "$leaf" -noout -pubkey |
    openssl pkey -pubin -outform DER | sha256sum) || return 1
  key_digest=$(openssl pkey -in "$key" -pubout -outform DER | sha256sum) || return 1
  test "$cert_digest" = "$key_digest"
}
```

Set `stage3_renewed_cert` and `stage3_renewed_key` to the operator-verified absolute source paths. Confirm the candidate certificate covers the full maintenance window and advances the expiration date. Keep the backup in a root-controlled location on the VPS for the approved retention window. Record the backup reference in the private change record.

```bash
stage3_backup=$(mktemp -d /root/dc24h-stage3-tls-backup.XXXXXXXX)
install -m 0600 "$stage3_cert" "$stage3_backup/fullchain.pem" &&
  install -m 0600 "$stage3_key" "$stage3_backup/privkey.pem" &&
  validate_stage3_pair "$stage3_backup/fullchain.pem" "$stage3_backup/privkey.pem"
validate_stage3_pair "$stage3_renewed_cert" "$stage3_renewed_key"
openssl x509 -in "$stage3_renewed_cert" -noout -issuer -dates -ext subjectAltName -fingerprint -sha256

stage3_staging=$(mktemp -d "$stage3_dir/.replacement.XXXXXXXX")
install -m 0600 "$stage3_renewed_cert" "$stage3_staging/fullchain.pem" &&
  install -m 0600 "$stage3_renewed_key" "$stage3_staging/privkey.pem" &&
  validate_stage3_pair "$stage3_staging/fullchain.pem" "$stage3_staging/privkey.pem"
```

Require success for every validation and backup step before proceeding. Keep the backup, staging directory and expected public fingerprints available to the recovery operator.

## Replace both service copies and restart

Run only in the authorized window when the independent observer is ready. Stop the unit before changing either service copy. If a command fails, stop the sequence and use rollback while the unit is inactive. Recheck the staged pair's root ownership and group-readable mode after applying it.

```bash
systemctl stop "$stage3_unit" &&
  test "$(systemctl show "$stage3_unit" -p ActiveState --value)" = inactive &&
  chown root:dc24h-stage3 "$stage3_staging/fullchain.pem" "$stage3_staging/privkey.pem" &&
  chmod 0640 "$stage3_staging/fullchain.pem" "$stage3_staging/privkey.pem" &&
  mv -T -- "$stage3_staging/fullchain.pem" "$stage3_cert" &&
  mv -T -- "$stage3_staging/privkey.pem" "$stage3_key" &&
  runuser -u dc24h-stage3 -- "$stage3_binary" --config /etc/dc24h-stage3/dc24h.ini --check-config &&
  systemctl start "$stage3_unit" &&
  systemctl is-active "$stage3_unit"
```

On the independent network, recheck CA/SAN/validity and compare the public leaf fingerprint with the approved replacement. Use packaged ncdc and another guest to confirm reconnect and bidirectional attributed chat. If using optional KEYP, distribute the new fingerprint over the OWNER's independently authenticated channel. A returning client with a stale pin needs that verified update.

## Restore the known-good pair on failure

Begin rollback if either file install fails, configuration validation fails, the service does not recover within the maintenance bound, or trusted TLS and guest chat cannot be independently verified. With the authorized rollback owner, stage both backup files in the same service filesystem, validate the pair, stop the unit, replace both files, validate configuration, and start the same unit.

```bash
stage3_restore=$(mktemp -d "$stage3_dir/.restore.XXXXXXXX")
install -o root -g dc24h-stage3 -m 0640 "$stage3_backup/fullchain.pem" "$stage3_restore/fullchain.pem" &&
  install -o root -g dc24h-stage3 -m 0640 "$stage3_backup/privkey.pem" "$stage3_restore/privkey.pem" &&
  validate_stage3_pair "$stage3_restore/fullchain.pem" "$stage3_restore/privkey.pem" &&
  systemctl stop "$stage3_unit" &&
  test "$(systemctl show "$stage3_unit" -p ActiveState --value)" = inactive &&
  mv -T -- "$stage3_restore/fullchain.pem" "$stage3_cert" &&
  mv -T -- "$stage3_restore/privkey.pem" "$stage3_key" &&
  runuser -u dc24h-stage3 -- "$stage3_binary" --config /etc/dc24h-stage3/dc24h.ini --check-config &&
  systemctl reset-failed "$stage3_unit" &&
  systemctl start "$stage3_unit" &&
  systemctl is-active "$stage3_unit"
```

Recheck the previous public fingerprint, trusted TLS, both guest logins and attributed chat from the independent network. If rollback fails, stop repeated attempts, retain the backup and bounded journal, and escalate to the OWNER with the current unit and SSH status. Keep evidence to public leaf fingerprint, expiry, SAN/issuer, pair match and mode attestations, UTC times, SSH availability, service state, external client results, and rollback disposition. Record evidence SHA-256 and reviewer. Treat actual replacement and actual rollback as separate results until the operator observes each one.
