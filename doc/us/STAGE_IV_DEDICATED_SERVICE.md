# Stage IV — dedicated WebAdmin service (review-only deployment runbook)

**Status: source and GitHub-hosted tests only. NOT INSTALLED on the Debian 13 VPS.**
Do not enable either unit or the Apache `:7278` vhost before independent
qualification and a separate `OWNER WEBADMIN PUBLIC ACTIVATION GO`. Existing
ADCS `:1512` must remain unchanged; ADC `:1511` must remain closed.

## Trust boundaries

| Component | Linux identity | Reachability / secrets |
| --- | --- | --- |
| Stage III ADCS hub | `dc24h-stage3` | Existing `:1512`, original C++ API socket and token remain `0700/0600` and same-UID only |
| Restricted auth bridge | UID `dc24h-stage3`, primary GID `dc24h-webadmin` | `/run/dc24h-stage4-bridge/bridge.sock` only, `0750` directory and `0660` socket; `SO_PEERCRED` requires exact WebAdmin UID |
| Public WebAdmin Node 24 | `dc24h-webadmin` | HTTP `127.0.0.1:4278` only; **no** hub token, socket or hub UID access |
| Apache TLS | existing Apache user | Approved hostname HTTPS `:7278`, fixed public allowlist only |

The bridge runs under the hub UID, so it **remains trusted code with hub-level
access** and must receive a security review. Its narrow API allowlist is only:
`POST /api/v1/admin/access`, `GET /api/v1/management`, and `GET /api/v1/status`.
It drops client-provided `Authorization` and substitutes the private C++ bearer
token. It does not provide a general-purpose Unix or HTTP proxy. The C++
same-UID gate stays intact and the Node gateway never sees that token.

## Provisioning order — manual provider console, not CI

1. Confirm the running service really is `dc24h-stage3-adcs.service` under
   `dc24h-stage3`, and verify the C++ API has been separately activated by
   OWNER. The bridge expects the actual private hub socket and token at
   `/run/dc24h/api.sock` and `/run/dc24h/api.token` in a **hub-owned 0700**
   parent; check their real paths before installation. Do not change their
   ownership, group or mode to make WebAdmin connect.
2. Review and install `config/dc24h-stage4-webadmin.sysusers.example` using
   `systemd-sysusers` to create `dc24h-webadmin`; verify that it differs from
   `dc24h-stage3` and root. Do not remove or rename the existing hub account.
3. Install a verified, root-owned Node **24 LTS** runtime under
   `/opt/dc24h/node24/bin/node` with executable permissions for the service.
   Verify the distribution's SHA-256 and publisher provenance. Debian's
   `/usr/bin/node` (observed v20) and `/root/.nvm` do not qualify this unit.
4. Install the exact reviewed source under `/opt/dc24h`; keep the bridge
   script, compiled `dist/public-main.mjs`, enrollment CLI and static panel
   root-owned/non-writable to WebAdmin. Verify `npm ci`, TypeScript, Python,
   C++ integration and Ubuntu CI on the exact commit before proceeding.
5. Stage the owner's provider-specific `msmtp` configuration in
   `/var/lib/dc24h-stage4/.msmtprc`, mode `0600`, owned by **only**
   `dc24h-webadmin`; its parent directory must be `0700` and owned by the
   same UID. Configure `account default`, authenticated SMTP, verified TLS,
   `from admin@dc24h.eu`, and the correct home.pl relay. Do not copy
   `/root/.msmtprc`, print credentials or place secrets in GitHub Actions.
   The sender invokes exactly `msmtp --file=/var/lib/dc24h-stage4/.msmtprc -t`
   without a shell; it rejects missing, symlinked or group-accessible config.
6. Separately provision owner-reviewed `/etc/dc24h/stage4-activation.env`
   as root-only `0600`. It must include
   `DC24H_STAGE4_STARTUP_ACK=OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO` and a
   **stable, unique**, lowercase 64-hex
   `DC24H_STAGE4_LIMITER_KEY` generated with `openssl rand -hex 32`.
   Do not reuse the hub API token or rotate the limiter key during an active
   authentication window. Keep the activation file absent until final GO.
7. Install examples as two separate reviewed systemd units, without enabling:
   `dc24h-twilight-stage4-bridge.service` **before**
   `dc24h-twilight-stage4-webadmin.service`. Use `systemd-analyze verify` and
   manual `systemctl show -p User -p Group` checks. The bridge depends on the
   existing Stage III hub; WebAdmin depends on the bridge. Neither has sudo.
8. With Node gateway stopped, perform the existing one-time verified mailbox
   enrollment on the provider console **as `dc24h-webadmin`** using the new
   bridge socket, Node 24 and account `Administrator` / `admin@dc24h.eu`.
   Type the C++ password and received e-mail code interactively (never in
   logs or chat). Create audit/rate-limit catalogs under
   `/var/lib/dc24h-stage4` as the dedicated UID; preserve the first-install
   `O_EXCL` and no-replacement rules.
9. Verify local authentication, SMTP delivery and actual mailbox receipt;
   negative OTP tests; role revocation; session/logout/CSRF; root/user
   isolation; concurrent requests; service restart and reversible rollback
   **before** any public listener is opened.
10. Apache's existing `config/apache2-webadmin-public.conf.example` is the
    HTTPS `:7278` proposal using the approved Let's Encrypt hostname. Enable
    only after exact-host/SAN, independent Internet ingress, cookie/path,
    TLS, port exposure (`1512`, `1511`, `4278`, `7278`) and backup review.
    This repository never edits `/etc/apache2`, certificates or host firewall.

## Safe checks and evidence

From the provider console, before deployment, gather only non-secret outputs:

```sh
python3 scripts/stage4_vps_preflight.py
systemctl show dc24h-stage3-adcs.service -p User -p Group -p ActiveState
getent passwd dc24h-stage3 dc24h-webadmin
ss -lnt '( sport = :1512 or sport = :1511 or sport = :4278 or sport = :7278 )'
```

For manual SMTP qualification, use the dedicated account and its exact config
path; do not run the real sender as root:

```sh
printf 'From: admin@dc24h.eu\nTo: admin@dc24h.eu\nSubject: Stage IV dedicated SMTP probe\n\nManual delivery check.\n' | \
  sudo -u dc24h-webadmin -H /usr/bin/msmtp --file=/var/lib/dc24h-stage4/.msmtprc -t
```

Success requires both exit status 0 and independent mailbox receipt. A root
SMTP test proves nothing about the WebAdmin account. Never send a real login
OTP in CI. Store only redacted pass/fail evidence.

## Recovery / rollback

Before activating Apache, snapshot exact binaries, both unit files, all
service-private catalogs, Apache configuration and certificate metadata
under an owner-controlled backup. The bridge itself retains no catalog.
If a check fails: disable/remove **only** the new WebAdmin Apache `:7278`
vhost, stop WebAdmin then stop the bridge, restore the reviewed configs and
catalogs, and rerun `stage4_vps_preflight.py`. Do not stop/replace the hub,
alter its `:1512` listener, change its original token permissions, or touch
the real Let's Encrypt private key. A systemd restart/rollback result from
Ubuntu CI is not a claim of a Debian 13 VPS test.

**Remaining owner-side checks:** live mailbox enrollment, actual SMTP relay,
hub role revocation, independent Internet TLS, Debian 13 dual-unit restarts,
backup/restore and rollback. None is considered PASS merely because CI passes.
