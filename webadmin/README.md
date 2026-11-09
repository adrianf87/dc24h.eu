# dc24h.eu WebAdmin — alpha-0.1.10

React + TypeScript + Vite, with a responsive EN/US and PL read-only panel.
Unavailable data is shown explicitly; no fake live metrics or browser token storage.

```sh
npm ci
npm run build
npm run dev
```

Requires Node.js `^20.19.0 || >=22.12.0`. Default: http://127.0.0.1:5173.
Optional real C++ connection: server-only `DC24H_API_SOCKET` and
`DC24H_API_TOKEN_FILE` absolute paths, same effective UID; keep the token outside
this WebAdmin root. Never use `VITE_*` for credentials.

`npm run preview` serves a static build with API disabled. Production web
sessions, roles, mutations and an authenticated Apache gateway remain planned.

See [setup and architecture (US)](../doc/us/WEBADMIN.md) or
[instrukcja PL](../doc/pl/WEBADMIN.md) for API connection, Apache2, installation
and verification. Do not expose the Vite development server to a network.
