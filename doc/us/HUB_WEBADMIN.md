# Local hub service management — beta-0.1.7

This development increment adds an Owner-only service panel to the existing React WebAdmin **Management** page. It builds on the fixed, independent systemd controller from beta-0.1.5 and the audited local service actions from beta-0.1.6. It does not expose public remote administration.

## Local workflow

1. Configure the separate maintenance process, its private API socket, the dedicated `dc24h-maintenance`/`dc24h-control` identities and the fixed `dc24h.service` policy as documented in [Service operations](SERVICE_OPERATIONS.md). No host unit is installed by this increment.
2. Open the WebAdmin **locally**, prove an active **Owner** account in Admin Core, and open Management. Administrator and lower roles cannot access service status or actions.
3. Refresh service state. `GET /admin-api/v1/management/hub/status` uses the existing private bearer adapter, grant, session and CSRF binding. The C++ maintenance API reads the fixed `STATUS` command through the peer-verified controller socket. Only the fixed unit `dc24h.service` can be returned. The panel does not depend on the hub process remaining online.
4. Start is available only when systemd reports `inactive`. Stop and Restart are available only when systemd reports `active`. Unknown, transitional, unloaded or unavailable states disable all actions. Choose an action, read the disconnection/recovery warning, tick the confirmation checkbox, then confirm. The browser submits only `confirm=START`, `confirm=STOP` or `confirm=RESTART` to the exact corresponding route. The server rechecks the current Owner and systemd state; browser controls are **not** authorization.
5. During an action the controls are disabled. After an acknowledged result, the panel fetches systemd status again. A timeout, lost response or `outcome_unknown` is **not automatically retried**. Inspect systemd and the durable audit before taking further action. A success response is not itself a claim about subsequent availability of the service.

## Users and history

Online user count is displayed only from a recent (no older than 15 seconds) *separate hub status API observation*, and only while the controller reports an active unit. A missing count is shown as unavailable; it is not inferred from systemd or from the last recorded peak. Status polling does not read private user details.

The panel links to **Administrative history**. The existing `/api/v1/audit` route requires the additional actor ID and account proof; it retains its 16-event pagination and v11 append-only service events (`hub.start`, `hub.stop`, `hub.restart`, target `service/1`, outcomes `started`, `succeeded`, `failed`). The service-only checkbox filters the *visible page*, not the database or all pages. Interrupted/uncertain actions can remain `started` for operator investigation. Secrets are never included in the panel or audit output.

## Security boundary and limitations

- Exact hub routes are forwarded only by the loopback Vite/Node maintenance adapter. The shipped Apache HTTPS proxy allowlist **does not** include them. Do not broaden it or expose a browser-facing internet gateway without a separate security review.
- The Node adapter checks same-origin loopback access, the fixed grant/session/CSRF headers, form content type, bounded body and upstream response. The C++ API independently checks the Owner's current role, and the controller independently checks peer credentials. There is no shell command, arbitrary unit name or browser-selected socket.
- The controller reads systemd for a bounded time; actions have finite timeouts and audited outcomes. The maintenance API sends completed replies with a separate bounded output deadline to allow slow but bounded systemd operations.
- The generic `POST /management/authorize` contract remains a non-executing compatibility endpoint. Configuration still has a restricted editor, not unrestricted live reload. ADC/ADCS behavior is unchanged.
- No schema migration is added in beta-0.1.7: migration **v11** already records service operations. No deployment, privilege change, host activation, automatic recovery or disaster-recovery validation is performed by this increment. Recovery remains an operator procedure described in [Service operations](SERVICE_OPERATIONS.md).

Verify with `npm run typecheck`, `npm test`, `npm run build`, WebAdmin browser tests and the C++/process suites on the supported GitHub-hosted CI matrix before merging.
