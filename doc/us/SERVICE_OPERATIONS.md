# Owner-gated hub service operations — beta-0.1.6

This development increment extends the independent beta-0.1.5 status controller with **Start, Stop and Restart** of one fixed systemd unit, `dc24h.service`. The serving ADC/ADCS hub is not the executor, and it can be stopped during a maintenance operation. No browser buttons, public action proxy, arbitrary unit names or remote shell are introduced. The WebAdmin UI is planned for beta-0.1.7.

## Trust boundaries and API

The separate `dc24h-account-admin` maintenance API accepts precisely:

| Method and path | Required form body | Expected verified state |
| --- | --- | --- |
| `POST /api/v1/management/hub/start` | `confirm=START` | `active` |
| `POST /api/v1/management/hub/stop` | `confirm=STOP` | `inactive` |
| `POST /api/v1/management/hub/restart` | `confirm=RESTART` | `active` |

The established process bearer token, live maintenance grant, matching session ID and CSRF secret are required. `AccessGate` reloads the actor's current database role per request and admits **Owner only**. The actor ID comes from the verified grant, never a form parameter. Query strings, other HTTP methods, extra form fields, unknown operations and arbitrary service names are rejected. The action routes are deliberately **not** added to the Apache2 or Node WebAdmin adapter allowlists in this stage.

The API subprocess must run under the distinct, non-root Unix identity `dc24h-maintenance` and must not share the hub user. It checks that the connected socket server is really owned by `dc24h-control` using kernel `SO_PEERCRED`. The controller independently checks that the requesting process has the exact dedicated maintenance **UID** for actions; ordinary `dc24h-admin` group peers may only read `STATUS`. The controller uses an allowlist of fixed `START`, `STOP`, `RESTART` messages and executes `/usr/bin/systemctl` with fixed `--system --no-pager --no-ask-password` arguments and `dc24h.service`, without a shell. Status remains available without the hub, without MariaDB and without this maintenance API. The maintenance API requires MariaDB to validate Owner and record audit intentions.

## Outcomes, audit and failure handling

- MariaDB migration **v11** extends the existing `dc24h_audit_events` action/target/outcome constraints. For each accepted confirmed request, the maintenance API first durably inserts a `service`/`1` event with `hub.start`, `hub.stop` or `hub.restart`, the real Owner ID and `started`. Only after this insert does it contact the controller.
- A manager exit success is checked against a subsequent bounded systemd state query. Confirmed operations become `succeeded`; definite failures become `failed`. A known conflict (already active/inactive or busy) yields HTTP `409`; unavailable or refused operations yield `503`. Success yields HTTP `200` with the operation, `outcome:succeeded` and verified `active_state`.
- Unavailable maintenance identity, missing controller socket or failed controller peer verification **before the command is sent** is a definite `503 service_unavailable`: the audit event becomes `failed`. A broken connection **after command delivery**, timeout, uncertain systemd response or failed audit completion is **`503 outcome_unknown`**. Its `started` record is intentionally preserved: the operation might have happened. Inspect `systemctl show dc24h.service` and the audit event before taking further action; **never automatically retry**. Request authentication/refusal before audit insertion does not create a service event.
- Each action subprocess has a bounded deadline and fixed executable/arguments. Non-Owner access is rejected before any controller call. The implementation makes no claim that the unit is healthy merely because systemd reports `active`.

## Manual deployment review

1. Review [controller sysusers](../../config/dc24h-control.sysusers.example), [controller service](../../config/dc24h-twilight-control.service.example) and the **example only** [Polkit rule](../../config/49-dc24h-control.rules.example). Provision separate `dc24h-control` and `dc24h-maintenance` system users; restrict the controller group and the parent directory of `/run/dc24h-control/status.sock` to those trusted identities. The real account-admin process must be started under `dc24h-maintenance` with a private configuration and private API/DB credentials; no sample auto-starts it.
2. If privileged service control is desired, install the reviewed Polkit rule under `/etc/polkit-1/rules.d` according to host policy. The rule grants **only** `org.freedesktop.systemd1.manage-units` for exact unit `dc24h.service` and verb `start`, `stop` or `restart` to user `dc24h-control`. Never give the controller broad root/sudo access or permission to manage other units.
3. Apply database migration v11 using the existing migration procedure before starting the new maintenance API. Test Owner, Administrator and unauthenticated requests, manager-unavailable behavior, journal/service state and the audit history in a disposable environment. Neither building nor installing the project enables a unit, applies a Polkit rule, changes host users or starts a service.

`hub.manage` remains marked as *not fully implemented* in the preexisting WebAdmin permission/catalog view until beta-0.1.7; the **three exact Owner-only local maintenance POST endpoints** described above are functional independently of that view. The old `POST /management/authorize` discovery contract still returns `501 not_implemented` and never executes an action. Existing ADC/ADCS networking is unchanged.
