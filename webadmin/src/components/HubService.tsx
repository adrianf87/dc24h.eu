import {useEffect, useRef, useState} from "react";
import type {Copy} from "../languages/us";
import {adminHeaders} from "../lib/adminAccess";
import {parseHubServiceResult, parseHubServiceStatus, serviceConfirmation} from "../lib/hub-service";
import type {HubServiceStatus, ServiceOperation} from "../lib/hub-service";
import type {HubSnapshot} from "../lib/status";

const path = "./admin-api/v1/management/hub/";
type Notice = "hubServiceAuth" | "hubServiceUnavailable" | "hubServiceRefused" | "hubServiceConflict" |
  "hubServiceUnknown" | "hubServiceSucceeded" | "hubServiceVerifyUnknown";

export function HubService({t, snapshot, checked}: {t: Copy; snapshot: HubSnapshot | null; checked: number | null}) {
  const [service, setService] = useState<HubServiceStatus | null>(null);
  const [selected, setSelected] = useState<ServiceOperation | null>(null);
  const [confirmed, setConfirmed] = useState(false);
  const [phase, setPhase] = useState<"idle" | "working" | "verifying">("idle");
  const [notice, setNotice] = useState<Notice | null>(null);
  const [lastResult, setLastResult] = useState<ServiceOperation | null>(null);
  const pending = useRef<AbortController | null>(null);
  const loadEpoch = useRef(0);
  useEffect(() => {
    void load();
    return () => {
      // StrictMode replays mount effects: invalidate the aborted request and
      // release its slot so the second setup can fetch a fresh status.
      loadEpoch.current++;
      const controller = pending.current;
      pending.current = null;
      controller?.abort();
    };
    // The grant is supplied by Admin Core; a manual refresh is available after login.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  async function request<T>(endpoint: string, operation: ServiceOperation | null, parse: (value: unknown) => T): Promise<T> {
    const headers = adminHeaders();
    if (!headers["X-DC24H-Admin-Grant"]) throw new Error("auth");
    const controller = new AbortController();
    pending.current = controller;
    const timer = setTimeout(() => controller.abort(), operation ? 36000 : 7000);
    try {
      const response = await fetch(path + endpoint, {
        method: operation ? "POST" : "GET",
        body: operation ? new URLSearchParams({confirm: serviceConfirmation(operation)}) : undefined,
        signal: controller.signal, cache: "no-store", credentials: "same-origin", redirect: "error",
        headers: {...headers, ...(operation ? {"Content-Type": "application/x-www-form-urlencoded", "X-DC24H-Admin": "1"} : {})},
      });
      if (!response.ok) {
        let error = "";
        try { const value: unknown = await response.json(); if (value && typeof value === "object" && "error" in value) error = String(value.error); }
        catch { /* Unknown result; do not repeat a write. */ }
        throw new Error(response.status === 401 || response.status === 403 ? "auth" :
          response.status === 409 ? "conflict" : error === "outcome_unknown" ? "unknown" : "refused");
      }
      return parse(await response.json());
    } finally {
      clearTimeout(timer);
      if (pending.current === controller) pending.current = null;
    }
  }

  function errorNotice(error: unknown, action: boolean): Notice {
    const reason = error instanceof Error ? error.message : "";
    if (reason === "auth") return "hubServiceAuth";
    if (reason === "conflict") return "hubServiceConflict";
    if (reason === "refused") return "hubServiceRefused";
    return action ? "hubServiceUnknown" : "hubServiceUnavailable";
  }

  async function load() {
    if (pending.current) return;
    const epoch = ++loadEpoch.current;
    setNotice(null); setSelected(null); setConfirmed(false);
    try {
      const observed = await request("status", null, parseHubServiceStatus);
      if (loadEpoch.current === epoch) setService(observed);
    } catch (error) {
      if (loadEpoch.current === epoch) { setService(null); setNotice(errorNotice(error, false)); }
    }
  }

  async function execute() {
    if (phase !== "idle" || pending.current || !selected || !confirmed || !service?.available || service.load_state !== "loaded") return;
    if (selected === "start" && service.active_state !== "inactive") return;
    if (selected !== "start" && service.active_state !== "active") return;
    const action = selected;
    setPhase("working"); setNotice(null); setService(null); setSelected(null); setConfirmed(false); setLastResult(null);
    try {
      await request(action, action, parseHubServiceResult);
      setLastResult(action);
      setPhase("verifying");
      try {
        const observed = await request("status", null, parseHubServiceStatus);
        setService(observed);
        setNotice(observed.available && observed.active_state === (action === "stop" ? "inactive" : "active")
          ? "hubServiceSucceeded" : "hubServiceVerifyUnknown");
      }
      catch { setNotice("hubServiceVerifyUnknown"); }
    } catch (error) { setNotice(errorNotice(error, true)); }
    finally { setPhase("idle"); }
  }

  const ready = service?.available === true && service.load_state === "loaded";
  const active = ready && service.active_state === "active";
  const inactive = ready && service.active_state === "inactive";
  const online = active && snapshot?.server.core_running && checked !== null && Date.now() - checked < 15000 ? snapshot.users.online : null;
  const actions: ServiceOperation[] = ["start", "stop", "restart"];
  const label = (action: ServiceOperation) => action === "start" ? t.hubServiceStart : action === "stop" ? t.hubServiceStop : t.hubServiceRestart;

  return <section className="card hub-service" data-testid="hub-service" aria-label={t.hubServiceTitle}>
    <div className="card-heading"><div><h2>{t.hubServiceTitle}</h2><p>{t.hubServiceIntro}</p></div></div>
    <p className="configuration-alert" role="note">{t.hubServiceLocal}</p>
    <div className="accounts-buttons"><button type="button" disabled={phase !== "idle" || !!pending.current} onClick={() => void load()}>{t.hubServiceRefresh}</button></div>
    <dl className="hub-service-status">
      <div><dt>{t.hubServiceState}</dt><dd data-testid="hub-service-state">{service?.available ? service.active_state : t.unavailable}</dd></div>
      <div><dt>{t.hubServiceSubstate}</dt><dd>{service?.available ? service.sub_state : "—"}</dd></div>
      <div><dt>{t.hubServiceUsers}</dt><dd data-testid="hub-service-online">{online ?? "—"}</dd></div>
    </dl>
    <p className="configuration-footnote">{t.hubServiceUsersNote}</p>
    {service?.available === false && <p role="status">{t.hubServiceUnavailable}</p>}
    <div className="accounts-buttons">
      {actions.map(action => <button key={action} type="button" data-testid={`hub-${action}`}
        disabled={phase !== "idle" || !!pending.current || !(action === "start" ? inactive : active)}
        onClick={() => {setSelected(action);setConfirmed(false);setNotice(null);}}>{label(action)}</button>)}
    </div>
    {selected && <div className="hub-service-confirm" data-testid="hub-service-confirm" role="group" aria-label={t.hubServiceConfirmTitle}>
      <h3>{t.hubServiceConfirmTitle}: {label(selected)}</h3>
      <p role="alert">{t.hubServiceWarning}</p>
      <label><input type="checkbox" disabled={phase !== "idle"} checked={confirmed} onChange={e => setConfirmed(e.target.checked)}/>{t.hubServiceConfirm}</label>
      <div className="accounts-buttons">
        <button type="button" disabled={!confirmed || phase !== "idle"} onClick={() => void execute()}>{t.hubServiceExecute}</button>
        <button type="button" onClick={() => {setSelected(null);setConfirmed(false);}}>{t.hubServiceCancel}</button>
      </div>
    </div>}
    {phase !== "idle" && <p role="status" data-testid="hub-service-progress">{phase === "working" ? t.hubServiceWorking : t.hubServiceVerifying}</p>}
    {notice && <p role={notice === "hubServiceUnknown" ? "alert" : "status"} data-testid="hub-service-notice">{t[notice]}{lastResult && notice === "hubServiceSucceeded" ? ` (${label(lastResult)})` : ""}</p>}
    <div className="hub-service-history"><h3>{t.hubServiceHistory}</h3><p>{t.hubServiceHistoryNote}</p><a href="#audit">{t.hubServiceOpenAudit}</a></div>
  </section>;
}
