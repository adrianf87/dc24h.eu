# Admin API in Alpha 0.1

The C++20 hub serves JSON under `/api/v1` when started with `--admin-api`. Boost.Beast listens on `127.0.0.1:8081` by default. One HTTP I/O thread and two workers handle up to 32 connections. HTTP reads and writes have five-second deadlines and requests are limited to 8 KiB of headers and 8 KiB of body. The workers handle MariaDB queries and password verification. The hub control strand provides bounded pages of online users.

## Database and account provisioning

Run `dc24h-hub --db migrate` with the existing E5 migration account and confirm that `--db status` reports `5 / 5`. Version 4 creates `admin_users` with a unique username, role, and encoded password hash. Version 5 creates `admin_sessions` with a binary token digest, account reference, and UTC creation and expiry times. Existing settings, bans, and audit events remain available through the same database. [E5 MariaDB setup](MARIADB.md)

Create local MariaDB accounts named `dc24h_api` and `dc24h_provision` with distinct generated passwords. Give the API account access to the following tables and use the provisioning account for local account creation and revocation.

```sql
GRANT SELECT ON dc24h.schema_migrations TO 'dc24h_api'@'localhost', 'dc24h_provision'@'localhost';
GRANT SELECT ON dc24h.admin_users TO 'dc24h_api'@'localhost';
GRANT SELECT, INSERT, DELETE ON dc24h.admin_sessions TO 'dc24h_api'@'localhost';
GRANT SELECT, INSERT, DELETE ON dc24h.bans TO 'dc24h_api'@'localhost';
GRANT SELECT, INSERT ON dc24h.audit_events TO 'dc24h_api'@'localhost';
GRANT SELECT (id, username), INSERT ON dc24h.admin_users TO 'dc24h_provision'@'localhost';
GRANT SELECT (admin_user_id), DELETE ON dc24h.admin_sessions TO 'dc24h_provision'@'localhost';
GRANT INSERT ON dc24h.audit_events TO 'dc24h_provision'@'localhost';
```

For an initial account, use the provisioner's MariaDB password file and a separate private file for the new administrator password. This shell example prompts without echoing the password.

```bash
export DC24H_DB_NAME=dc24h
export DC24H_DB_USER=dc24h_provision
export DC24H_DB_PASSWORD_FILE=/path/to/private/provisioner-db-password
umask 077
password_file=$(mktemp)
read -rsp 'New administrator password: ' password
printf '\n'
printf '%s\n' "$password" > "$password_file"
unset password
dc24h-hub --db admin create operator admin --password-file "$password_file"
unlink "$password_file"
```

Use a unique password of 12–128 bytes. Usernames contain 3–48 lowercase ASCII letters, digits, periods, underscores, or hyphens. Use `viewer` as the role for read-only accounts. A provisioner can use `dc24h-hub --db admin revoke operator` to delete all current sessions for an account. A later login with a valid password can create a new session. Admin API credentials are separate from ADC GPA/PAS identities.

## Enable the listener

The hub keeps the E5 read-only connection configured with `DC24H_DB_USER` and `DC24H_DB_PASSWORD_FILE`. The API uses `DC24H_API_DB_USER` and `DC24H_API_DB_PASSWORD_FILE` with the same `DC24H_DB_NAME` and `DC24H_DB_SOCKET`. The password files use mode `0400` or `0600`. Run the service as the dedicated unprivileged hub user with access to those files and the ADCS certificate and key.

```sh
export DC24H_API_DB_USER=dc24h_api
export DC24H_API_DB_PASSWORD_FILE=/etc/dc24h/secrets/api-db
dc24h-hub --serve --mariadb --admin-api \
  --admin-origin https://hub.example.test \
  --tls-cert /etc/dc24h/tls/fullchain.pem --tls-key /etc/dc24h/tls/key.pem
```

Use the browser's canonical HTTPS origin without a trailing slash. It can include a port other than 443. The loopback TCP port is configurable with `--admin-port`. For browser use, proxy the loopback listener through the HTTPS virtual host that serves Webadmin. Put the following directives in that virtual host with `mod_proxy` and `mod_proxy_http` active.

```apache
ProxyRequests Off
ProxyPass        "/api/v1/" "http://127.0.0.1:8081/api/v1/"
ProxyPassReverse "/api/v1/" "http://127.0.0.1:8081/api/v1/"
```

Keep request bodies and credential headers out of proxy logs. The browser sends its normal `Origin` header on writes. [Apache reverse proxy documentation](https://httpd.apache.org/docs/2.4/en/mod/mod_proxy.html)

## Endpoints

| Method and path | Access and result |
| --- | --- |
| `POST /api/v1/auth/login` | HTTPS origin and JSON credentials. Returns `200` and a session cookie. |
| `POST /api/v1/auth/logout` | Admin or viewer with CSRF token. Returns `204` and clears the cookie. |
| `GET /api/v1/auth/me` | Admin or viewer. Returns username, role, and CSRF token. |
| `GET /api/v1/status` | Admin or viewer. Returns hub version, active connections, and online user count. |
| `GET /api/v1/users` | Admin or viewer. Lists current SIDs, nicknames, and CIDs. |
| `GET /api/v1/bans` | Admin or viewer. Lists persisted bans with reasons and timestamps. |
| `POST /api/v1/bans` | Admin with CSRF token. Adds a ban and returns `201` and its ID. |
| `DELETE /api/v1/bans/{id}` | Admin with CSRF token. Returns `204` or `404` for an unknown ID. |
| `GET /api/v1/audit` | Admin or viewer. Lists audit events. |

Send login credentials as JSON with `Content-Type: application/json` and the configured `Origin`. The password below is a placeholder.

```json
{"username":"operator","password":"<administrator password>"}
```

A successful login sets `__Host-dc24h_session` and returns the following shape. The browser keeps the cookie and sends it in subsequent same-origin requests. Read `GET /api/v1/auth/me` to recover the CSRF token after a page reload.

```json
{"username":"operator","role":"admin","csrf_token":"<64 lowercase hexadecimal characters>"}
```

Send `X-CSRF-Token` and the configured `Origin` on logout and both ban mutations. Logout and ban deletion take no body. For ban creation send JSON like this example. Use `cid` for a canonical ADC CID or `ip` for a single numeric address. Omit `duration_seconds` or send `null` for a permanent ban.

```json
{"kind":"ip","identifier":"192.0.2.25","reason":"Repeated spam","duration_seconds":3600}
```

A successful create returns `{"id":"42"}`. IDs use decimal strings to preserve 64-bit precision. Ban changes are audited with the authenticated username, and the hub applies them on its next admission check.

Use `?limit=25` on lists and pass the returned `next_cursor` as `after` for the next page. The default is 50 and the maximum is 100. The users cursor is a SID. The ban and audit cursors are decimal IDs. Online users can join or leave between pages.

```json
{"items":[{"sid":"AAAB","nickname":"example","cid":"ZXO4VT7KPNYLJBLFLOR5YP3A33SPNOHYMEDJ4MY"}],"next_cursor":null}
```

Errors use the same JSON shape across endpoints.

```json
{"error":{"code":"forbidden","message":"Administrator role is required"}}
```

## Authentication and troubleshooting

- `401` indicates missing, invalid, revoked, or expired authentication, or rejected login credentials. A session expires eight hours after issue. Login hashes use Argon2id with 64 MiB, three passes, one lane, and a fresh salt. Session identifiers use 32 random bytes and MariaDB stores only their SHA-256 digests. [RFC 9106](https://www.rfc-editor.org/rfc/rfc9106.html)
- `403` indicates an insufficient role or a failed CSRF or origin check. Cookies use `Secure`, `HttpOnly`, and `SameSite=Strict`. Login also checks the origin. Authenticated writes require a CSRF token derived separately for each session. [OWASP CSRF prevention guidance](https://cheatsheetseries.owasp.org/cheatsheets/Cross-Site_Request_Forgery_Prevention_Cheat_Sheet.html)
- `400` indicates invalid request data. `413` and `431` indicate body and header limits respectively. Send flat JSON objects with unique keys and the supported fields.
- `429` limits login attempts to five per username and 30 total in a one-minute window per process. Retry after the interval in `Retry-After`.
- `503` indicates that the API could not access MariaDB or obtain a live users page. Check the local socket, schema version, and API SQL grants. ADC/ADCS I/O continues during API database failures.

Successful login, logout, and ban changes write audit events without credential values. Backups of the full schema include both admin tables. Protect them like the existing MariaDB backups.

## Tests

```sh
cmake --preset debug -DDC24H_TEST_MARIADB=ON
cmake --build --preset debug
ctest --preset debug -L admin-api
```

The test starts a disposable MariaDB instance on its own Unix socket and exercises the CLI and HTTP routes using local connections. The full CTest suite includes the earlier ADC and ADCS tests.
