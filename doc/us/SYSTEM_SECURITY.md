# System security — alpha-0.9.9

Debian and Ubuntu service hardening. The branch starts from main `21865ac`. The branch name is `alpha-0.9.9`.

The hub still runs in the foreground and stops on SIGTERM. It does not daemonize, install a unit, or start a service. This increment adds one example unit and one locked system account. Copying the files does not enable them.

## Unit

`config/dc24h.service.example` is `Type=simple`. `ExecStart` is `/usr/local/bin/dc24h --config /etc/dc24h/dc24h.ini`. Change that path when the install prefix is not `/usr/local`. The user and group are `dc24h`. The capability set is empty, so a listen port below 1024 is refused. `NoNewPrivileges`, `ProtectSystem=strict`, `ProtectHome`, `PrivateTmp` and `MemoryDenyWriteExecute` are on. Address families are only `AF_UNIX`, `AF_INET` and `AF_INET6`. The system-call filter is `@system-service`. A failure restarts at most 5 times in 60 seconds. A clean SIGTERM does not restart.

`ProtectSystem=strict` leaves `/usr`, `/etc` and the rest of the hierarchy read-only. The writable exceptions are the directories systemd creates for this service:

- `/var/lib/dc24h`, mode `0700`, for state and for owner-only secret files
- `/run/dc24h`, mode `0700`, for the API socket
- `/var/log/dc24h`, mode `0750`, for a log file. The hub appends that file only when the file itself is mode `0600` and owned by the service user. See [Logging security](LOGGING_SECURITY.md).

`/etc/dc24h` is mode `0755` and is not a secret store. Do not put the database password, the API token or the ADC key there. Keep those files mode `0600` under `/var/lib/dc24h`. Point the API socket at `/run/dc24h`, which matches the existing private-directory rule. A backup path must also be under the writable state directory. Membership of the MariaDB socket group stays with the operator.

## Account

`config/dc24h.sysusers.example` describes system user `dc24h` with shell `/usr/sbin/nologin` and home `/var/lib/dc24h`. The build does not run `systemd-sysusers` on the host. Create the account only when enabling the unit.

## Unchanged

The example is not copied into `/etc/systemd/system` and `systemctl enable` is not run. There is no AppArmor or SELinux policy, no firewall and no unattended upgrade. This does not close residual threat T07: root and any other process of user `dc24h` can still open the socket. Schema v9 is unchanged. No status counter, WebAdmin page, INI key or Apache setting is added. `Require local` stays.

`systemd.sandbox` checks the shipped directives, verifies the unit, creates the account only inside a private root, starts the hub under those restrictions as `nobody`, accepts a TCP connection, binds a mode `0600` API socket, drops effective capabilities, refuses writes outside the service directories, and stops on SIGTERM. When the test is not root and passwordless sudo works, it re-executes with the same hub binary and the same two example paths. Without root and without sudo, the live service checks are skipped and the directive checks still run.
