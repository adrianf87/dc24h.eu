# Automatic TLS certificate renewal

WebAdmin exposes the certificate and schedule under **Settings → Security → TLS certificates**. Owners and Administrators can view the status and run the readiness check. An Owner must enter the current password and use a session issued within five minutes of the existing eight-digit email OTP verification to change the schedule.

## Ubuntu deployment scope

The Ubuntu deployment uses WebAdmin HTTPS on TCP 7178 and public ADCS on TCP 1512. Apache proxies the public API to Node at 127.0.0.1:4278. MariaDB and the two C++ APIs use private Unix sockets. Retain TCP 7278 for the historical Debian 13 WebAdmin deployment.

The Ubuntu 24.04 inspection found Certbot 2.9.0, a single standalone HTTP-01 lineage, an explicit IPv4 bind address, and a disabled distribution timer. The existing public certificate has the exact DNS SAN `server-747585786.dc24h.eu`. Apache used the live Certbot lineage. The serving hub used separate copies and had `CanReload=no`. TCP 80 had no local listener at inspection time. Verify DNS ownership and external TCP 80 reachability before approving the method.

This implementation supports one public DNS SAN, one pre-existing apt Certbot lineage, the Let's Encrypt production directory, and standalone HTTP-01 bound to the approved IPv4 address on TCP 80. Use a separate Owner decision for other ACME methods, DNS changes, provider credentials, additional names, or port and firewall changes. Confirm the intended challenge against Let's Encrypt staging in an approved disposable setup. The WebAdmin readiness check validates local prerequisites and serving endpoints without placing an ACME order. [Certbot user guide](https://eff-certbot.readthedocs.io/en/stable/using.html#renewing-certificates)

## Prepare an approved installation

1. Record the accepted source SHA. Back up the serving configuration, hub and gateway binaries and assets, existing certificate pair, Apache vhost, systemd units, and protected Certbot data. Record tested recovery access and the verified expiry of the certificate that would be restored.
2. Review the selected Certbot lineage and the global `cli.ini`. Keep the standalone authenticator, explicit HTTP-01 IPv4, production CA, and default archive and work paths. The worker accepts the Ubuntu package's log and renewal defaults plus its own fixed hook. Review other Certbot jobs before making this controller the scheduler for the lineage. A non-dc24h lineage needs its own approved scheduling plan before transferring control of a shared timer.
3. Provision Python 3 with the Ubuntu Certbot, ConfigObj and cryptography packages. Install [the TLS units](../../config/tls-renewal/) as root-owned systemd units. Keep the renewal timer disabled. Install `scripts/tls_renewal.py` as `/usr/local/libexec/dc24h-tls/tls_renewal.py` and `scripts/tls_certbot_deploy_hook.sh` as `/usr/local/libexec/dc24h-tls/certbot-deploy-hook`. The latter must be executable. Use root-owned directories and files without group or world write.
4. Create `/etc/dc24h-tls` mode 0755 and `/var/lib/dc24h-tls` mode 0700, owned by root. Install `renewal.json.example` as `/etc/dc24h-tls/renewal.json` mode 0600. Set the approved hostname, Certbot lineage and IPv4 address. Set `approval` to `OWNER_TLS_HTTP01_ACTIVATION_GO` after the Owner approves this topology and the ACME checks. Run the following once to prepare the existing certificate for both services.

   ```sh
   sudo /usr/bin/python3 -I /usr/local/libexec/dc24h-tls/tls_renewal.py bootstrap
   ```

5. In the approved rollout, set Apache's `SSLCertificateFile` and `SSLCertificateKeyFile` in the existing `dc24h-stage4-webadmin.conf` vhost to the corresponding `/etc/dc24h-tls/served/current/fullchain.pem` and `privkey.pem`. Keep TCP 7178 in Listen, VirtualHost, Host/Origin rules and the Node origin. Apply the exact Stage IV proxy rules from the revised example. Run `apache2ctl configtest` before a graceful reload.
6. Set the two hub `[tls]` paths to the same managed bundle. Enable the private `[api]` with `socket = /run/dc24h-stage3-api/hub.sock` and `token_file = /var/lib/dc24h-stage3/api.token`. Provision a 64-hex-character token owned by `dc24h-stage3` with mode 0600 or 0400. Put `DC24H_TLS_RELOAD_HOSTNAME=<approved DNS name>` into root-owned `/etc/dc24h-tls/hub.env`. Install the ADCS drop-in from `config/tls-renewal/adcs-reload.conf.example`. Install the new hub binary and the matching C++ account service, Stage IV bridge, Node gateway, and WebAdmin assets. Plan the initial software cutover with the Owner. Routine renewals use `ExecReload`.
7. Reload the systemd unit definitions and enable the control **socket** with `systemctl enable --now dc24h-tls-control.socket`. Verify the private C++ services, public TLS endpoints and Apache graceful reload command. The serving hub's authenticated Unix reload must be available, and the two listeners must present the prepared certificate. The normal timer stays disabled until the Owner uses WebAdmin.

## Enable, observe, and disable

Choose **Check readiness**. Address any reported prerequisite before enabling. Choose **Enabled**, provide the current password, then apply the setting. The response reports the actual persisted timer state and next check. The timer runs at 03:00 and 15:00 UTC with up to 30 minutes of jitter, and catches a missed calendar run after boot. Certbot decides when the lineage is due. A run accepts only the fixed successful deploy-hook receipt for a changed lineage and reuses an eligible newer lineage on a later check when deployment must be retried.

The worker verifies the system CA chain, exact SAN, validity and private-key match. It publishes the pair through one atomic symlink change, validates Apache's configuration, and verifies the expected leaf independently at both TLS listeners. The hub retains the native contexts for existing connections. Certificate dates come from the leaf's validity fields. Application status reflects the last recorded probe.

Choose **Disabled** to remove the schedule. The active bundle stays available to both listeners. An in-progress renewal holds the operation lock; repeat a change after the reported busy state clears. Monitor the latest verification, expiry, timer state and last successful renewal. Root-only records are in `/var/lib/dc24h-tls/audit.jsonl`, the journal for the renewal and control units, and Certbot's protected log directory.

## Recover

- On a failed deployment the worker restores the previous bundle link, reloads services that need it and verifies both listeners. The protected pending state supports recovery after a killed worker. Check both service rows and the audit. If rollback is unverified, investigate the service reload and certificate validity before the next cutover. Retain the pending state for recovery.
- For a maintenance rollback, disable the schedule, confirm the worker has finished and preserve the audit and bundle state. Restore the verified certificate and configuration together. Roll Apache back with `apache2ctl configtest` and a graceful reload. Use the new hub's certificate reload while the managed paths and software remain in service. An older hub binary or a different path configuration requires an approved software rollback window.
- Verify a fresh trusted TLS connection to each public listener with the expected DNS SAN and leaf fingerprint. Keep any independently distributed ncdc KEYP fingerprints synchronized through the approved out-of-band channel. The Certbot lineage can be ahead of the serving bundle after a rollback. Preserve the Certbot archive and accounts for reconciliation.
