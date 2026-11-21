export type ServiceOperation = "start" | "stop" | "restart";

export type HubServiceStatus =
  | {schema: 1; available: false; error: "systemd_unavailable" | "invalid_systemd_response"}
  | {schema: 1; available: true; unit: "dc24h.service"; load_state: string; active_state: string; sub_state: string; result: string};

export interface HubServiceResult {
  operation: ServiceOperation;
  outcome: "succeeded";
  active_state: "active" | "inactive";
}

const record = (value: unknown): Record<string, unknown> => {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid service response");
  return value as Record<string, unknown>;
};
const state = (value: unknown): value is string => typeof value === "string" && /^[a-z-]{1,48}$/.test(value);

export function parseHubServiceStatus(value: unknown): HubServiceStatus {
  const data = record(value);
  if (data.schema !== 1) throw new Error("Invalid service response");
  if (data.available === false && (data.error === "systemd_unavailable" || data.error === "invalid_systemd_response"))
    return {schema: 1, available: false, error: data.error};
  if (data.available !== true || data.unit !== "dc24h.service" || !state(data.load_state) ||
      !state(data.active_state) || !state(data.sub_state) || !state(data.result)) throw new Error("Invalid service response");
  return {schema: 1, available: true, unit: "dc24h.service", load_state: data.load_state,
    active_state: data.active_state, sub_state: data.sub_state, result: data.result};
}

export function parseHubServiceResult(value: unknown): HubServiceResult {
  const data = record(value);
  if (data.outcome !== "succeeded" || !["start", "stop", "restart"].includes(String(data.operation)) ||
      data.active_state !== (data.operation === "stop" ? "inactive" : "active"))
    throw new Error("Invalid service response");
  return {operation: data.operation as ServiceOperation, outcome: "succeeded", active_state: data.active_state as "active" | "inactive"};
}

export const serviceConfirmation = (operation: ServiceOperation): string => operation.toUpperCase();
