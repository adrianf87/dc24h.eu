# dc24h.eu service migration and rollback

Issue [#36](https://github.com/adrianf87/dc24h.eu/issues/36) tracks the rename. Use one reviewed build for the hub, auxiliary binaries, local and remote WebAdmin, and public Stage IV WebAdmin. Apply the same rule to rollback because the old and new WebAdmin status parsers accept different server identities, even at the same software version.

## Preflight and backup

1. In a disposable Ubuntu environment, record the source SHA, check the outputs of all four `--version` commands, and verify the replacement units with `systemd-analyze verify`. Use the same build and unit files in the change window.
2. Inventory the loaded fragments, drop-ins, and enablement for all units involved. A historical High Starter unit may already occupy `dc24h.service`. Review any unit using a target name, including inactive services, aliases, generated definitions, and local drop-ins. Check for separately launched hub processes and instance-specific services such as `dc24h-stage3-adcs.service` that share listeners or state.
3. Back up the currently installed application binaries and WebAdmin assets, systemd unit files and drop-ins, the Polkit rule, and the exact set of enabled units. Take and verify a matched backup of private configurations, account and session state, and MariaDB using the instance's established recovery procedure. Keep credentials and backups in owner-only storage.
4. Confirm the deployed Apache virtual host, public HTTPS port and Origin, Node loopback binding, and WebAuthn RP ID against the instance configuration. For example, a deployment using port 7178 needs its own approved configuration. The repository's 7278 example is for that specific endpoint. Retain the Apache client-IP overwrite and private bridge.

```bash
units=(dc24h.service dc24h-control.service dc24h-remote-gateway.service
       dc24h-stage4-{webadmin,bridge,auth}.service)
old_units=("${units[@]/dc24h/dc24h-twilight}")
sudo systemctl show -p Id -p FragmentPath -p DropInPaths -p ActiveState \
  -p UnitFileState "${units[@]}" "${old_units[@]}"
```

## Cutover

1. After Owner approval, stop the controllers and WebAdmin gateways before the hub and private auth services. Stop and disable the installed Twilight units, and confirm each affected process has exited before replacing files. Remove any old enablement links and review aliases and drop-ins. For a collision at a destination name, preserve the old files in the backup and explicitly retire the old definition. Recheck after `systemctl daemon-reload` that the destination name is vacant.
2. Install the new binaries and WebAdmin assets. For a vacant unit destination, use create-only publication, for example `sudo ln -T /etc/systemd/system/.dc24h.service.staged /etc/systemd/system/dc24h.service` after staging the reviewed file on the same filesystem with ownership `root:root` and mode `0644`. A pre-existing destination makes this command fail. Remove the temporary staging link after publishing. The Stage IV installer checks both sets of units and refuses to overwrite a different destination unit.
3. For the hub, control service and remote gateway, install the reviewed rule and units before creating the new root-owned `/etc/dc24h/dc24h-unit-migration-approved` marker. Keep the prior marker state in the rollback record. Reload systemd definitions and start only the installed components approved for the instance. Start the hub before enabling service management. For public Stage IV, start `dc24h-stage4-webadmin.service` to pull in its bridge and auth dependencies.
4. Verify the hub's ADC/ADCS connection and welcome text, matched status API on the private socket, local and remote WebAdmin, and public WebAdmin login, OTP, roles and read views as applicable. Verify `dc24h-stage4-auth.service` if using the TLS renewal control socket. Check that the predecessor units are inactive before restoring traffic or enablement. Recheck Apache syntax with `apache2ctl configtest` if its site configuration was staged for replacement.

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

2. Remove the unit files installed during this change and restore the saved unit definitions, drop-ins, Polkit rule, binaries, and WebAdmin assets from the matched backup. If the previous installation used the original Twilight marker, restore its recorded state. Restore private data only under the established database/private-state recovery procedure if required.
3. Run `sudo systemctl daemon-reload`. Start the previous hub and verify it with the previous WebAdmin build before re-enabling the recorded previous services. For a Twilight installation the hub command is `sudo systemctl start dc24h-twilight.service`. Restore any public Stage IV and controller units applicable to that host.
