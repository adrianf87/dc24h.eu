# Threat model — alpha-0.9.1

This analysis records how the hub and WebAdmin can be attacked, which controls already exist, and which risks remain. The branch starts from main `7872471`. The branch name is `alpha-0.9.1`.

The analysis does not add a protocol check, a schema migration, an Apache setting or a WebAdmin control. Schema v9 is unchanged. `configuration.manage` and `hub.manage` stay unimplemented. Browser kick and mute are still not dispatched. This is not a public authenticated gateway and it is not a penetration test.

alpha-0.9.9 system security, added after this analysis, ships one systemd example that runs the hub as a locked system user with an empty capability set and a strict read-only hierarchy. The build does not install or start that service. It does not close T07. Schema v9 is unchanged. See [System security](SYSTEM_SECURITY.md).

alpha-0.9.8 database security, added after this analysis, writes one consistent owner-only snapshot when the operator asks and restores it only into empty tables of the same schema. A lost commit acknowledgement is not replayed. It does not close T14. Schema v9 is unchanged. See [Database security](DATABASE_SECURITY.md).

alpha-0.9.7 API security, added after this analysis, requires the session id and the CSRF secret on every maintenance call except the password proof. A grant alone no longer reads. It does not close T04 or T05. Schema v9 is unchanged. See [API security](API_SECURITY.md).

alpha-0.9.6 account security, added after this analysis, refuses an ADC password identical to the account password and decrypts that password on the reactor only after the account attempt budget and an account ban check. It does not close T02 or T03. Schema v9 is unchanged. See [Account security](ACCOUNT_SECURITY.md).

Older sections of [Security](SECURITY.md) describe the release that wrote them. A sentence there that still says accounts, roles or bans are absent describes that earlier release. Current behavior is the set of documents linked below.

## Assets

- Hub listener state: connections, sessions, nicknames, CIDs, source addresses and routing tables.
- Account secrets: the scrypt verifier, the separate AES-256-GCM ADC password, and the external ADC key file. See [ADC Authentication](ADC_AUTHENTICATION.md).
- The process bearer token and the private Unix socket of the process that holds it. See [API](API.md).
- The maintenance grant, the public session id and the CSRF secret, held in the maintenance process. See [Security sessions](SECURITY_SESSIONS.md).
- MariaDB rows for accounts, bans and successful maintenance. See [Database](DATABASE.md) and [Audit logs](AUDIT_LOGS.md).
- TLS private keys for ADCS and for the Apache examples.
- Chat text, search criteria and transfer tokens while the hub routes them. The hub does not store a message archive.
- Configuration files and the MariaDB password file.

## Actors

- A remote ADC or ADCS client, including a guest and an authenticated account.
- A staff account that already passed GPA/PAS. Rank comes from the stored role, not from client INF. See [Permissions](PERMISSIONS.md).
- A person at the loopback WebAdmin browser. The Apache examples keep `Require local`.
- The host operator, who can read the service token, the ADC key, the database password file and a Unix socket of the same user.
- A network attacker who can send TCP, TLS or HTTP to an exposed listener.

The host operator and any process with the hub UID are inside the trust boundary of the socket and the secret files. This analysis does not treat that compromise as solved.

## Boundaries

1. The kernel accept path. Identity is the accepted sockaddr. Proxy headers are not a source of identity. See [Security](SECURITY.md).
2. The ADC parser, state machine, INF/PID/CID check and SUP policy, then GPA/PAS when the nickname or CID is registered.
3. Ready-session commands: chat, search, transfer coordination, kick, mute and ban admission.
4. Two private Unix sockets. The serving hub exposes status and health and requires the bearer. `dc24h-account-admin` is a separate process for account, role, ban and audit maintenance. The supported procedure stops serving hubs that share the database before those writes. There is no live configuration reload.
5. The loopback adapter. It is configured with one socket and one token file. It injects that bearer and ignores a browser `Authorization` header. Pointing it at the hub socket does not turn the hub into the maintenance process.
6. Apache. The static example denies `/admin-api` and `/api`. The proxy example forwards an allowlist and strips client bearer tokens, cookies and forwarded identity. See [Apache2 HTTPS](APACHE2_HTTPS.md) and [Apache2 proxy](APACHE2_PROXY.md).
7. The browser page. The language preference may be stored. The grant, the session id and the CSRF secret stay in page memory.

## Threats

### T01 Spoofed client identity

A client can present another nickname or CID. The INF check requires Tiger(PID) equal to CID and removes PD from the public snapshot. A registered nickname or CID that does not match the account is refused and does not fall back to guest. An unknown pair can still enter as guest. A stolen PID lets its holder compute a matching CID. The hub does not bind a CID to a TLS client certificate.

Current control: [ADC Authentication](ADC_AUTHENTICATION.md) and the INF rules in [Security](SECURITY.md). Residual risk: guest entry for an unknown identity, and no client-certificate binding.

### T02 Online password guessing

Repeated GPA/PAS attempts can target an account or an address. Shared token buckets limit attempts per second, per source IP and per account. Success does not refund a token. The refusal is generic. Buckets live in memory and reset when the process restarts. Clients behind one NAT share one IP budget. There is no cross-process or cross-host sharing.

Current control: [Anti-bruteforce](ANTI_BRUTEFORCE.md). Residual risk: a restart clears the buckets, and the limits are local to one hub process.

### T03 Captured ADC password

ADC PAS is Tiger(password concatenated with the challenge). The hub must read the ADC password, so it stores that password in AES-256-GCM, separate from the scrypt verifier. `require_tls` defaults to on. `require_tls=off` is an explicit operator opt-in and lets a captured PAS be attacked offline. The external key file is as sensitive as the passwords.

Current control: [ADC Authentication](ADC_AUTHENTICATION.md). Residual risk: an operator who disables the TLS requirement, or who exposes the key file, gives up this control.

### T04 Stolen maintenance grant

A grant is 64 hexadecimal characters in the maintenance process, with a 600-second lifetime and four slots. A new proof for the same actor drops the old slot. The password proof is the only maintenance body that returns the grant, the session id and the CSRF secret. WebAdmin does not put them in web storage. Every other maintenance call, including a read, requires all three from the same slot. A grant alone is refused with 403 `csrf_required`.

Current control: [API security](API_SECURITY.md), [Security sessions](SECURITY_SESSIONS.md) and [Admin API](ADMIN_API.md). Residual risk: copying the grant, the session id and the CSRF secret from page memory or from a local process still reads and writes maintenance data until the slot ends.

### T05 Cross-site maintenance write

A browser on the same machine can try to submit a maintenance call with a grant observed elsewhere. Reads and writes, other than the password proof, require the session id and the CSRF secret from the same slot. A mismatch returns 403 `csrf_required`. The adapter refuses a presented grant that lacks both values before it uses the Unix socket. The access proof does not require CSRF, because that response issues the secrets.

Current control: [API security](API_SECURITY.md) and [Security sessions](SECURITY_SESSIONS.md). Residual risk: the proof response still carries the three secrets, and a caller who copies all three can use them until the slot ends. This does not close T05.

### T06 Service token in the browser or in Apache

The adapter injects the bearer. Browser JavaScript, the static site and the proxy example do not contain it. The proxy strips a client Authorization header. If `Require local` is removed, a remote client can use the adapter as if it were loopback and reach the configured process with the adapter's token.

Current control: [API](API.md), [WebAdmin](WEBADMIN.md) and [Apache2 proxy](APACHE2_PROXY.md). Residual risk: removing `Require local` breaks this boundary.

### T07 Same-UID access to the Unix socket

The socket directory is private to the owner, and the peer credential must be the same effective UID. Root and any process of that user can connect and present the token if they can read the token file. The API has a small connection cap and a short request timeout. That cap is not an authorization boundary between programs of the same user.

Current control: [API](API.md) and [System security](SYSTEM_SECURITY.md). Residual risk: same-UID and root compromise of the service account stay inside the trust boundary. The example unit is not enabled by the build. That does not close T07.

### T08 Remote WebAdmin

Both Apache examples keep `Require local`. There is no cookie session and no remote administrator login. Certificate name and key correspondence are operator duties. A passing configuration test does not prove that correspondence.

Current control: [Apache2 HTTPS](APACHE2_HTTPS.md). Residual risk: a public authenticated gateway remains separate work. This is not a public authenticated gateway.

### T09 Forged proxy identity

The hub does not trust `X-Forwarded-For` or any other forwarded-identity header. Admission, the authentication buckets and the maintenance budget use the accepted sockaddr or the in-memory session, not a header. The proxy example removes those headers and sets `ProxyAddHeaders Off`. A trusted-proxy hop is not implemented. Clients behind one address share one address budget.

Current control: [Security](SECURITY.md) and [Apache2 proxy](APACHE2_PROXY.md). Residual risk: one NAT or one proxy collapses many clients into one budget.

### T10 Staff authority

Kick, mute, unmute and account, CID and IP bans use the stored role and the moderation rules. A client cannot raise its own rank through INF. Owner transfer needs two proofs through the local CLI. WebAdmin shows kick and mute decisions and does not send them. `configuration.manage` and `hub.manage` have no engine. A staff account that already holds a permission can use it. A stolen staff password carries that same authority until the account is disabled.

Current control: [Permissions](PERMISSIONS.md), [Moderation rules](MODERATION_RULES.md), [Kick](KICK.md), [Mute](MUTE.md) and [Ban core](BAN_CORE.md). Residual risk: an ordinary kick, mute or ban does not require a second person.

### T11 Missing audit

Successful account, ownership and ban maintenance can be listed by `audit.read`. Refusals, passwords, proofs, free-text reasons, live kick and live mute are not rows. Registration outside Admin Core does not add a row. Security log events are aggregates. There is no production audit of CSRF failures or of grant use.

Current control: [Audit logs](AUDIT_LOGS.md). Residual risk: the audit table cannot reconstruct a refused action or a live kick.

### T12 Hub sees routed client data

TLS ends at the hub for ADCS. The hub then routes chat text, search criteria and connection tokens in hub memory. A private message is delivered to the addressed session. The hub does not keep a message archive. The documented security and authentication events omit source addresses, nicknames, CID, PID, passwords and ADC text. See [Logging](LOGGING.md) and [Anti-bruteforce](ANTI_BRUTEFORCE.md). A process dump, a debugger or a compromised hub host still sees the clear text after TLS. Peer file bytes are not proxied. See [Transfer limits](TRANSFER_LIMITS.md).

Current control: those routing and logging rules. Residual risk: the hub is on the path of the messages it routes.

### T13 Resource exhaustion

Admission limits connections per IP, accepts per IP, frame rate, invalid frames, handshake time and partial frames. Search and transfer coordination have their own budgets. See [Search security](SEARCH_SECURITY.md) and [Transfer limits](TRANSFER_LIMITS.md). Maintenance accepts one POST per second and 24 requests in 10 seconds on a live session. These are local resource policies. They are not a bandwidth cap and they are not a distributed denial-of-service control. A client that passes admission can still spend CPU on TLS and on scrypt. A slow log disk can block the process.

Current control: [Security](SECURITY.md) and [Security sessions](SECURITY_SESSIONS.md). Residual risk: there is no byte-bandwidth cap and no cross-host sharing.

### T14 Secret files

The database password, the API token and the ADC key come from owner-controlled files, not from the command line. SQL uses bound parameters. Errors omit server text. A backup, a world-readable copy or a copy in a log defeats that separation. Migrations run only when an operator asks for them. Ordinary startup does not migrate.

Current control: [Database security](DATABASE_SECURITY.md) and [Database](DATABASE.md). Residual risk: off-host copies and MariaDB datadir permissions stay with the operator. The hub writes one owner-only snapshot only when asked. That does not close T14.

### T15 TLS policy gaps

ADCS on the hub listener uses TLS 1.2 or newer. The Apache examples select TLS 1.2 and 1.3 with AEAD ciphers and a short host-scoped HSTS policy. HTTP/2, OCSP and automatic certificate renewal are not implemented. Clients must check the hub certificate themselves. The hub does not request a client certificate.

Current control: [Apache2 HTTPS](APACHE2_HTTPS.md). Residual risk: those three TLS operations remain unimplemented.

### T16 Unfinished administrative surface

The maintenance catalog can name configuration and hub administration and then report them unimplemented. WebAdmin does not dispatch kick or mute. Account, role and ban writes belong to `dc24h-account-admin` after serving hubs that share the database are stopped. The serving hub's Unix API remains status and health.

Current control: the missing engines are absent, and the pages say so. Residual risk: this surface is unfinished work, not a hidden control.

## What remains separate work

A public authenticated gateway, remote administrator access, cookie sessions, HTTP/2, OCSP, automatic certificate renewal, production audit logging, an AppArmor or SELinux policy, a firewall, unattended package upgrades, `configuration.manage`, `hub.manage`, and browser kick or mute. This document does not mark any of them done.
