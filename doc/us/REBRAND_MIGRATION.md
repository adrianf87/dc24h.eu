# dc24h.eu service migration and rollback

Issue [#36](https://github.com/adrianf87/dc24h.eu/issues/36) tracks the rename. Use one reviewed build for the hub, auxiliary binaries, local and remote WebAdmin, and public Stage IV WebAdmin. Apply the same rule to rollback because the old and new WebAdmin status parsers accept different server identities, even at the same software version.

## Rebranding baseline

The source baseline is protected `main` at `cfd979d4a9f14b158dfc2f35f270ab2d6e2a055e`. GitHub now serves the repository as `adrianf87/dc24h.eu`. Update an existing checkout with `git remote set-url origin https://github.com/adrianf87/dc24h.eu.git`. Keep historical tags and evidence linked to the revisions and workflows that produced them.

- The brand is `dc24h.eu` in the CLI, ADC welcome, and WebAdmin. The installed binaries are `dc24h`, `dc24h-owner`, `dc24h-account-admin`, and `dc24h-service-controller`. The npm package is `dc24h-webadmin`.
- The C++ hub exposes the v1 status API to Node.js over the authenticated Unix socket. The local and remote status consumers require both `server.product_id = "dc24h"` and `server.name = "dc24h.eu"`. The previous Twilight parser requires the Twilight name. In a mixed deployment, the local and remote status views reject the response until the hub and WebAdmin match.
- Apache2 terminates public HTTPS and proxies to the loopback Node.js gateway. The React administration views share the public session established by the existing login flow. The Stage IV gateway uses MariaDB for identity and sessions and reaches C++ through its private bridge. Reuse the reviewed DNS, TLS, WebAuthn RP ID, allowed Origin, service accounts, socket paths, and persistent state for the instance.
- The Ubuntu 22.04, 24.04, and 26.04 workflows use the canonical repository for current qualification. Stage V covers source changes in pull requests and runs again on `main`. The older Beta artifact checkers verify their original repository and artifact contracts. [GitHub Actions CI](GITHUB_CI.md)

## Preflight and backup

1. In a disposable Ubuntu environment, record the source SHA, check the outputs of all four `--version` commands, and verify the replacement units with `systemd-analyze verify`. Use the same build and unit files in the change window.
2. Inventory the loaded fragments, drop-ins, and enablement for all units involved. A historical High Starter unit may already occupy `dc24h.service`. Review any unit using a target name, including inactive services, aliases, generated definitions, and local drop-ins. Check for separately launched hub processes and instance-specific services such as `dc24h-stage3-adcs.service` that share listeners or state.
3. Back up the installed application binaries and WebAdmin assets, the Apache site and its enablement, the Stage IV activation file, systemd unit files and drop-ins, the Polkit rule, and the set of enabled units. Take and verify a matched backup of private configurations, account and session state, and MariaDB using the instance's recovery procedure. Keep backups in owner-only storage.
4. Confirm the deployed Apache virtual host, public HTTPS port and Origin, Node loopback binding, and WebAuthn RP ID. Select `7178` for the approved Ubuntu deployment or `7278` for the historical Debian deployment. Use that selection in both the installer argument and `WEBADMIN_PUBLIC_PORT`. Retain the existing certificate paths, Apache client-IP overwrite, and private bridge. [Stage IV installation and upgrade](STAGE_IV_DEDICATED_SERVICE.md)

```bash
units=(dc24h.service dc24h-control.service dc24h-remote-gateway.service
       dc24h-stage4-{webadmin,bridge,auth}.service)
old_units=("${units[@]/dc24h/dc24h-twilight}")
sudo systemctl show -p Id -p FragmentPath -p DropInPaths -p ActiveState \
  -p UnitFileState "${units[@]}" "${old_units[@]}"
```

## Cutover

1. After Owner approval, stop the controllers and WebAdmin gateways before the hub and private auth services. Stop and disable the installed Twilight units, and confirm each affected process has exited before replacing files. Remove any old enablement links and review aliases and drop-ins. For a collision at a destination name, preserve the old files in the backup and explicitly retire the old definition. Recheck after `systemctl daemon-reload` that the destination name is vacant.
2. Install the new binaries and WebAdmin assets. For a vacant unit destination, use create-only publication, for example `sudo ln -T /etc/systemd/system/.dc24h.service.staged /etc/systemd/system/dc24h.service` after staging the reviewed file on the same filesystem with ownership `root:root` and mode `0644`. A pre-existing destination makes this command fail. Remove the staging link after publishing. The Stage IV installer checks both sets of units and preserves the existing Apache site when it matches the selected port. It aborts on a mismatched site. Bootstrap verifies the site before provisioning and records the matched `DC24H_STAGE4_ORIGIN`.
3. For the hub, control service and remote gateway, install the reviewed rule and units before creating the new root-owned `/etc/dc24h/dc24h-unit-migration-approved` marker. Keep the prior marker state in the rollback record. Reload systemd definitions and start only the installed components approved for the instance. Start the hub before enabling service management. For public Stage IV, start `dc24h-stage4-webadmin.service` to pull in its bridge and auth dependencies.
4. Verify the hub's ADC/ADCS connection and welcome text, matched status API on the private socket, local and remote WebAdmin, and public WebAdmin login, OTP, roles and read views as applicable. Verify `dc24h-stage4-auth.service` if using the TLS renewal control socket. Check that predecessor units are inactive before restoring traffic or enablement. Check the Apache configuration with `apache2ctl configtest` and verify the selected HTTPS Origin.

`Conflicts=` prevents the named unit pairs from remaining active together. Starting one can stop its counterpart, so use the explicit stop-and-verify sequence. `After=` orders start after the conflicting unit's stop. The hub's approval marker is an additional startup assertion. For custom units or processes, review and handle conflicts during preflight.

## Rollback

1. Stop entry points and disable the new units before stopping their backing services. Use the commands below for the units recorded as installed in this change. Check that their processes have exited before restoring files.

   ```bash
   sudo systemctl stop dc24h-control.service dc24h-remote-gateway.service
   sudo systemctl stop dc24h-stage4-webadmin.service
   sudo systemctl stop dc24h-stage4-bridge.service dc24h-stage4-auth.service dc24h.service
   # Disable only the new units actually enabled for this instance.
   sudo systemctl disable dc24h.service dc24h-stage4-webadmin.service
   sudo rm -- /etc/dc24h/dc24h-unit-migration-approved
   ```

2. Remove the unit files installed during this change and restore the saved unit definitions, drop-ins, Polkit rule, binaries, and WebAdmin assets from the matched backup. Restore the previous Apache site, its enablement, and the Stage IV activation file together if they changed. Confirm the former Origin and run `apache2ctl configtest` before restoring traffic. If the previous installation used the original Twilight marker, restore its recorded state. Restore private data only through the established recovery procedure if required.
3. Run `sudo systemctl daemon-reload`. Start the previous hub and verify it with the previous WebAdmin build before re-enabling the recorded previous services. For a Twilight installation the hub command is `sudo systemctl start dc24h-twilight.service`. Restore any public Stage IV and controller units applicable to that host.
