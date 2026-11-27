# Runner security and inventory hygiene

The runner is hosted on a Debian 13 VPS which also serves public ADCS (TCP 1512) and public HTTPS WebAdmin (TCP 7278). The `github-runner` account has unrestricted, passwordless `sudo`. Downloaded or pull-request-controlled code executed by this account may obtain host-root privileges, including access to live hub secrets and MariaDB.

The OWNER previously accepted this co-location, but it remains a material risk. Routine CI must not implicitly mutate public service state.

## Never capture or commit

- GitHub runner `.credentials*`, registration tokens or personal access tokens.
- Passwords, OTP e-mail codes, cookies, auth tokens and private service configurations.
- `/etc/dc24h/*`, `/etc/letsencrypt/**/privkey.pem`, TLS private keys and SMTP configuration.
- MariaDB database exports, service data directories or user private files.
- Full process environment dumps, shell history or raw APT command histories.
- Unredacted system logs containing user data or credentials.

## Safe evidence

Package names and versions, structured `dpkg` events, OS/kernel, tool versions, CPU/RAM/disk summary, nonsecret service names and ports, and checksums are in scope. Review inventory before each push; do not use unattended credential uploads.

## CI controls

- Confirm exact approved source commit and runner labels.
- Privileged installation, service restart, backup/restore or rollback require a separately authorized manual workflow and a synthetic fixture.
- No arbitrary PR execution with unrestricted `sudo` on a public-service host.
- Do not mount host `/`, secret directories, production MariaDB state or Docker socket into build containers.
- Do not use `--privileged` containers for tests on this VPS.
- Do not restart public ADCS/WebAdmin for routine lint/build/unit tests.

For full details, see [runner operations](RUNNER.md).
