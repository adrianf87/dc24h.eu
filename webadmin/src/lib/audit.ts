export const AUDIT_ACTIONS = ["accounts.create","accounts.activate","accounts.disable","accounts.password","accounts.reset","accounts.role","ownership.bootstrap","ownership.transfer","bans.create","bans.revoke","configuration.write","configuration.restore"] as const;
const id = (value: unknown): value is string => typeof value === "string" && /^[1-9]\d{0,18}$/.test(value) && BigInt(value) <= 9223372036854775807n;
const time = (value: unknown): value is number => typeof value === "number" && Number.isSafeInteger(value) && value >= 0 && value <= 253402300799;
export interface AuditEvent {
  id: string;
  occurred_at: number;
  action: typeof AUDIT_ACTIONS[number];
  actor_id: string;
  target_kind: "account" | "ban" | "configuration";
  target_id: string;
  outcome: "started" | "succeeded" | "failed";
}
export function parseAuditEvent(value: unknown): AuditEvent {
  if (!value || typeof value !== "object") throw new Error("Invalid audit event");
  const row = value as Record<string, unknown>;
  const configuration = row.target_kind === "configuration";
  const configAction = row.action === "configuration.write" || row.action === "configuration.restore";
  if (!id(row.id) || !time(row.occurred_at) || typeof row.action !== "string" || !(AUDIT_ACTIONS as readonly string[]).includes(row.action) ||
      !id(row.actor_id) || !id(row.target_id) ||
      !(configuration ? configAction && row.target_id === "1" &&
        (row.outcome === "started" || row.outcome === "succeeded" || row.outcome === "failed") :
        (row.target_kind === "account" || row.target_kind === "ban") && !configAction && row.outcome === "succeeded")) {
    throw new Error("Invalid audit event");
  }
  return {id: row.id, occurred_at: row.occurred_at, action: row.action as AuditEvent["action"], actor_id: row.actor_id,
    target_kind: row.target_kind as AuditEvent["target_kind"], target_id: row.target_id, outcome: row.outcome as AuditEvent["outcome"]};
}
export function parseAudit(value: unknown) {
  if (!value || typeof value !== "object") throw new Error("Invalid audit history");
  const body = value as Record<string, unknown>;
  if (body.maintenance !== true || body.restart_required !== true || !Array.isArray(body.events) || body.events.length > 16 || !(body.next_after === null || id(body.next_after))) {
    throw new Error("Invalid audit history");
  }
  const events = body.events.map(parseAuditEvent);
  if (events.some((event, index) => index && BigInt(event.id) <= BigInt(events[index - 1].id)) ||
      (body.next_after !== null && (events.length !== 16 || body.next_after !== events.at(-1)?.id)) ||
      (body.next_after === null && events.length === 16)) {
    throw new Error("Invalid audit page");
  }
  return {maintenance: true as const, restart_required: true as const, events, next_after: body.next_after as string | null};
}
