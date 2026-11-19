# Apache2 HTTPS reverse proxy — alpha-0.8.14

The shipped [proxy example](../../config/apache2-webadmin-https-proxy.conf.example) is a separate HTTPS site from the [static HTTPS example](APACHE2_HTTPS.md). It keeps TLS 1.2/1.3, the fixed HTTP-to-HTTPS redirect, local access, secret-file denial and the same security headers. It additionally reverse-proxies an allowlisted WebAdmin API to a loopback HTTP adapter.

This file does not activate a host site, obtain a certificate, start the adapter, reload Apache or change the C++ hub. The static example still denies `/admin-api` and `/api`. The hub still does not trust proxy headers for admission or rate limits.

## What is protected

- HTTP port 80 only redirects to the fixed `https://dc24h.eu/` destination. It does not proxy.
- `ProxyRequests Off` disables forward-proxy use. `TraceEnable Off` disables TRACE.
- The upstream is `http://127.0.0.1:4173`. Replace that port with the loopback adapter. Do not point it at the C++ Unix socket, and do not put a service token or an admin grant in the file. The adapter injects its own bearer.
- Client `Authorization`, `Cookie`, `Forwarded`, `Proxy` and `X-Forwarded-*` / `X-Real-IP` headers are removed. `ProxyPreserveHost` and `ProxyAddHeaders` stay off, so the adapter sees `Host: 127.0.0.1:4173` rather than a client-supplied name.
- A canonical `Origin: https://dc24h.eu` is rewritten to `http://127.0.0.1:4173`, which is the origin the adapter compares with that Host. Any other origin is denied before proxying. Reads may omit Origin. Writes may not.
- `X-DC24H-Admin` is forwarded only when its value is exactly `1`. `X-DC24H-Admin-Grant` is forwarded only when it is 64 hexadecimal characters. `X-DC24H-Session` is forwarded only when it is 32 lowercase hexadecimal characters. `X-DC24H-Csrf` is forwarded only when it is 64 hexadecimal characters. Apache does not invent these headers. A maintenance write still has to satisfy the session check in [Security sessions](SECURITY_SESSIONS.md).
- `Sec-Fetch-Site` must be absent, `same-origin` or `none`. `cross-site` is denied.
- The API `Host` must be exactly `dc24h.eu`. Chunked `Transfer-Encoding` is denied. API bodies are limited to 7000 bytes. Proxied responses are `Cache-Control: no-store`.
- Paths outside the allowlist, including `/api/`, unknown `/admin-api/` paths and the wrong method, are denied and are not proxied. Hidden files, secret suffixes and symlinks stay denied.

## Allowlist

GET, with an empty body: `/admin-api/v1/status`, `/health`, `/users`, `/admin`, `/management`, `/accounts`, `/accounts/{id}` and `/bans`. `{id}` is a decimal string from 1 through 19 digits. Query strings on those exact paths are forwarded.

POST, with `Content-Type: application/x-www-form-urlencoded`, `X-DC24H-Admin: 1`, the canonical origin and a body of 1 through 7000 bytes: `/admin-api/v1/admin/access`, `/audit`, `/accounts`, `/accounts/{id}/{action}`, `/bans` and `/bans/{id}/revoke`. `{action}` is `activate`, `disable`, `reset`, `password` or `role`.

`/accounts/{id}` without an action is a read, not a write. `/bans/{id}` without `/revoke` is not proxied. The hub process still publishes only health, status and users. Account, ban, audit, discovery and management routes remain on the maintenance process. Status and health stay bearer-only inside the adapter. This example does not add cookie sessions, remote administrator access, configuration writes, live hub administration or browser kick/mute. Page-memory maintenance sessions are specified in [Security sessions](SECURITY_SESSIONS.md) and still require `Require local`.

`Require local` remains on the static directory and on the API gate. Removing it would let a remote client use the adapter's service token, because the adapter treats the Apache connection as loopback. Do not do that. A public authenticated gateway remains separate work.

The example does not configure HTTP/2, OCSP stapling, automatic certificate renewal or production audit logging. `apache2ctl configtest` does not prove that the certificate matches the key; the proxy regression starts Apache with a mismatched pair and expects failure. No host site, trust store or service is changed.

## Customize

Install the example next to the static template, enable `ssl`, `headers`, `proxy` and `proxy_http`, then edit the hostname, asset path, certificate paths and the three `127.0.0.1:4173` occurrences together. The adapter must already be listening on that loopback port and must be the only process that reads the service token. Keep `Require local`.

```sh
ctest --test-dir build -R '^apache2.https.proxy$' --output-on-failure
```

The test loads the shipped file under a temporary ServerRoot, on ephemeral loopback ports, with a disposable certificate and a disposable upstream. Only paths, ports and the upstream address are substituted. It checks the allowlist, header stripping, origin rewrite, denial of `/api` and unknown routes, body limits, legacy TLS rejection and mismatched-key startup. It does not claim an external scan or a production deployment.
