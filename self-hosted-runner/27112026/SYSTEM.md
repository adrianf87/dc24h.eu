# Debian 13 host and service baseline

**Source:** OWNER's `screenfetch` and shell output; independent inventory from `2026-11-27T16:42:49Z`. These are point-in-time observations, not continuously monitored properties.

## Hardware and OS

| Field | Verified or OWNER-reported value |
| --- | --- |
| Runner display name / hostname | `server-747585786` |
| Public host / IPv4 | `server-747585786.dc24h.eu` / `62.171.156.32` |
| OS | Debian GNU/Linux **13** (Trixie), amd64 |
| Kernel | `6.12.117+deb13-cloud-amd64` |
| CPU | AMD EPYC, **16 logical CPUs**, approximate clock reported 3.195 GHz |
| Physical memory shown by `screenfetch` | 64,308 MiB total; 1,715 MiB used |
| `/proc/meminfo` at inventory | `MemTotal: 65852064 kB` |
| Disk reported by `screenfetch` | 278 GB nominal; 8.3 GB in use (4%) |
| Root filesystem recorded by collector | 257,815,288 1-KiB blocks total; 8,671,792 in use; 238,616,760 available |
| Shell | GNU Bash `5.2.37` |
| Uptime at OWNER capture | 9 days, 12 hours, 20 minutes |
| Installed Debian packages | **1,110**, from actual `dpkg-query` inventory |
| GitHub Actions account | `github-runner`, passwordless `sudo` |

Disk figures originate from different commands and units; do not present them as an exact identical measurement.

## Observed tool versions

The collector ran as `github-runner`, so these are versions that user could invoke at snapshot time:

| Tool | Observed version |
| --- | --- |
| Git | `2.47.3` |
| Bash | `5.2.37(1)-release` |
| Python 3 | `3.13.5` |
| Node.js | `v20.19.2` |
| npm | `9.2.0` |
| CMake | `3.31.6` |
| GNU C++ compiler | `g++ 14.2.0` |
| msmtp | `1.8.28` |
| MariaDB client | `11.8.9` |
| OpenSSL | `3.5.9` |

Earlier on `root`, Node.js `v26.13.1` was installed under `/root/.nvm`; this **does not make Node 26 available** to `github-runner`. No Docker or Podman version was reported by the 10-tool collector output. This is **not proof** that either package was previously removed.

Other observed installed software: **MariaDB server 11.8.9**, **Apache HTTP Server 2.4.71**, and **CMake 3.31.6**.

## Co-located services

| Service | Last observed port / binding | Notes |
| --- | --- | --- |
| Public ADCS | `62.171.156.32:1512` | `dc24h-stage3-adcs.service`, running under `dc24h-stage3` |
| Public WebAdmin HTTPS | `*:7278` | Apache reverse proxy |
| WebAdmin gateway | `127.0.0.1:4278` | Local Node.js application |
| MariaDB | `127.0.0.1:3306` | Do not expose publicly |
| Legacy ADC | `:1511` | No listener in earlier checks |

Stage IV service unit names observed running: `dc24h-twilight-stage4-auth.service`, `dc24h-twilight-stage4-bridge.service`, `dc24h-twilight-stage4-webadmin.service`.

## Read-only freshness checks

```bash
uname -a
id github-runner
node --version
npm --version
systemctl is-active dc24h-stage3-adcs.service
systemctl is-active dc24h-twilight-stage4-auth.service dc24h-twilight-stage4-bridge.service dc24h-twilight-stage4-webadmin.service
ss -lnt '( sport = :1512 or sport = :1511 or sport = :7278 or sport = :4278 or sport = :3306 )'
```

Never commit complete service environments, account secrets, runner credentials or passwords.
