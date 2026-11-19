import {useEffect, useRef, useState} from "react";
import {parseAudit} from "../lib/audit";
import type {AuditEvent} from "../lib/audit";
import {readAccountResponse} from "../lib/accounts";
import type {Copy} from "../languages/us";

export function AuditLog({t}:{t:Copy}) {
  const [items, setItems] = useState<AuditEvent[]>([]);
  const [next, setNext] = useState<string | null>(null);
  const [cursor, setCursor] = useState("0");
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");
  const proof = useRef("");
  const actor = useRef("");
  const form = useRef<HTMLFormElement>(null);
  const pending = useRef<AbortController | null>(null);
  useEffect(() => () => { proof.current = ""; pending.current?.abort(); }, []);
  async function load(after: string) {
    if (!actor.current || !proof.current) { setMessage(t.auditUnavailable); return; }
    pending.current?.abort();
    const controller = new AbortController();
    pending.current = controller;
    setBusy(true);
    setMessage("");
    const body = new URLSearchParams();
    body.set("actor_id", actor.current);
    body.set("proof", proof.current);
    body.set("after", after);
    const timer = setTimeout(() => controller.abort(), 15000);
    try {
      const response = await fetch("./admin-api/v1/audit", {
        method: "POST", cache: "no-store", credentials: "same-origin", redirect: "error", signal: controller.signal,
        headers: {"Content-Type": "application/x-www-form-urlencoded", "X-DC24H-Admin": "1"}, body,
      });
      const result = parseAudit(await readAccountResponse(response));
      if (pending.current !== controller) return;
      if (!response.ok) { setItems([]); setNext(null); setMessage(t.auditUnavailable); return; }
      setItems(result.events);
      setNext(result.next_after);
      setCursor(after);
      setMessage(t.auditLoaded);
    } catch {
      if (pending.current === controller) { setItems([]); setNext(null); setMessage(t.auditUnavailable); }
    } finally {
      body.forEach((_, key) => body.set(key, ""));
      clearTimeout(timer);
      if (pending.current === controller) setBusy(false);
    }
  }
  function submit(event: React.FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (busy) return;
    const data = new FormData(event.currentTarget);
    actor.current = String(data.get("actor_id") ?? "");
    proof.current = String(data.get("proof") ?? "");
    event.currentTarget.reset();
    void load("0");
  }
  return <section className="card accounts-card" data-testid="audit">
    <div className="card-heading"><div><h2>{t.auditTitle}</h2><p>{t.auditNote}</p></div></div>
    <p className="accounts-maintenance">{t.auditMaintenance}</p>
    {message && <p role="status">{message}</p>}
    <form ref={form} onSubmit={submit}>
      <fieldset disabled={busy}>
        <legend>{t.auditShow}</legend>
        <label>{t.auditActor}<input name="actor_id" required pattern="[1-9][0-9]{0,18}" inputMode="numeric" autoComplete="off" /></label>
        <label>{t.auditProof}<input name="proof" required type="password" maxLength={1024} autoComplete="off" /></label>
        <button className="button primary" type="submit">{busy ? t.accountsWorking : t.auditShow}</button>
      </fieldset>
    </form>
    <div className="accounts-table-wrap"><table><caption>{t.auditTable}</caption>
      <thead><tr><th>ID</th><th>{t.auditWhen}</th><th>{t.auditAction}</th><th>{t.auditTarget}</th><th>{t.auditOutcome}</th></tr></thead>
      <tbody>{items.length ? items.map(event => <tr key={event.id}><td>{event.id}</td><td>{new Date(event.occurred_at * 1000).toLocaleString()}</td><td><code>{event.action}</code></td><td>{t[event.target_kind === "account" ? "auditAccount" : "auditBan"]} {event.target_id}</td><td>{t.auditSucceeded}</td></tr>) : <tr><td colSpan={5}>{t.auditEmpty}</td></tr>}</tbody>
    </table></div>
    <div className="accounts-buttons">
      <button disabled={busy || cursor === "0" || !proof.current} onClick={() => void load("0")}>{t.accountsFirst}</button>
      <button disabled={busy || !next || !proof.current} onClick={() => void load(next!)}>{t.accountsNext}</button>
    </div>
  </section>;
}
