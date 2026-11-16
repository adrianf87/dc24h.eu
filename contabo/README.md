# Contabo worker

Existing runner: `server-747585786`, repository runner ID 27, Ubuntu 24.04,
Linux x86_64, account `github-runner`. Reuse its enabled systemd service:
`actions.runner.adrianf87-dc24h.eu.server-747585786.service`.
Do not register a duplicate runner or replace its registration.

Required labels: `self-hosted`, `Linux`, `X64`, `server-747585786`, `ubuntu-24-04`.
Keep the server label exclusive. The workflow checks both runner and host.
Builds and tests use six parallel workers. See [CI documentation](../doc/us/CONTABO_CI.md)
and [Polish documentation](../doc/pl/CONTABO_CI.md).

An administrator provisions packages once on the exact host from a trusted
copy of the branch:

```sh
sudo python3 contabo/scripts/dependencies.py install
python3 contabo/scripts/dependencies.py check
systemctl is-active actions.runner.adrianf87-dc24h.eu.server-747585786.service
systemctl is-enabled actions.runner.adrianf87-dc24h.eu.server-747585786.service
```

The helper refuses another host or OS. It preserves an existing service-start
policy; otherwise it temporarily blocks package-triggered daemon starts.
Tests launch their own isolated MariaDB and Apache instances. CI requires no
sudo password or NOPASSWD rule. Node 22 is installed by setup-node; Chromium is
installed for the runner user. Never commit passwords, tokens or runner credentials.

The folder is excluded from source archives and included by normal Git checkout.
