import { useEffect, useRef, useState } from "react";
import type { FormEvent, ReactNode } from "react";
import { Dashboard } from "../components/Dashboard.tsx";
import { ChatStatistics } from "../components/ChatStatistics.tsx";
import { SearchStatistics } from "../components/SearchStatistics.tsx";
import { TransferStatistics } from "../components/TransferStatistics.tsx";
import { KickStatistics } from "../components/KickStatistics.tsx";
import { MuteStatistics } from "../components/MuteStatistics.tsx";
import { ModerationRules } from "../components/ModerationRules.tsx";
import { Roles } from "../components/Roles.tsx";
import { StartupConfiguration } from "../components/StartupConfiguration.tsx";
import { ACCOUNT_ROLES } from "../lib/roles.ts";
import { BAN_FILTERS, banState } from "../lib/bans.ts";
import { pl } from "../languages/pl/index.ts";
import { us } from "../languages/us/index.ts";
import type { HubSnapshot } from "../lib/status.ts";
import type { PublicAdminInput, PublicAdminOperation, PublicAdminResult } from "../../server/public-admin.ts";

const polish = document.documentElement.lang.toLowerCase().startsWith("pl");
const local = (english: string, translated: string) => polish ? translated : english;
const t = polish ? pl : us;
const locale = polish ? "pl-PL" : "en-US";
const number = (value: number | undefined) => value === undefined ? "—" : new Intl.NumberFormat(locale).format(value);
const roleLabel = (role: keyof typeof roleRank) => t[`role_${role}`];
const roleRank = { registered: 1, vip: 2, moderator: 3, operator: 4, administrator: 5, owner: 6 };

class ApiError extends Error {
  constructor(readonly status: number, readonly outcomeUnknown: boolean) { super("API unavailable"); }
}

async function api<T>(endpoint: string, signal: AbortSignal, csrf?: string, input?: object): Promise<T> {
  const reply = await fetch("/stage4/v1/" + endpoint, {
    method: input ? "POST" : "GET", credentials: "same-origin", cache: "no-store", redirect: "error",
    signal: AbortSignal.any([signal, AbortSignal.timeout(18_000)]),
    ...(input ? { headers: { "Content-Type": "application/json", "X-DC24H-Stage4-Csrf": csrf ?? "" },
      body: JSON.stringify(input) } : {}),
  });
  if (!reply.headers.get("content-type")?.startsWith("application/json")) throw new ApiError(reply.status, !!input);
  const payload = await reply.json() as { schema?: number; data?: T; error?: string };
  if (!reply.ok || payload.schema !== 1 || payload.data === undefined)
    throw new ApiError(reply.status, payload?.error === "outcome_unknown");
  return payload.data;
}

const labels = {
  denied: local("Access denied or invalid request.", "Odmowa dostępu lub nieprawidłowe żądanie."),
  stale: local("The record changed. Reload it before making changes.", "Rekord się zmienił. Wczytaj go przed ponowną zmianą."),
  expired: local("Session expired. Sign in again.", "Sesja wygasła. Zaloguj się ponownie."),
  limit: local("Request limit reached. Wait a minute.", "Osiągnięto limit żądań. Poczekaj minutę."),
  unknown: local("Could not confirm the outcome. Reload the record before retrying.", "Nie można potwierdzić wyniku. Wczytaj rekord przed ponowną próbą."),
  down: local("API unavailable.", "API niedostępne."),
  loading: local("Loading…", "Wczytywanie…"),
  proof: local("Your current password", "Twoje aktualne hasło"),
  confirm: local("I confirm this change.", "Potwierdzam tę zmianę."),
  saved: local("Change saved.", "Zapisano zmianę."),
  refresh: local("Refresh", "Odśwież"),
};
function errorMessage(error: unknown, writes: boolean) {
  if (error instanceof ApiError) {
    if (error.status === 401) return labels.expired;
    if (error.status === 409) return labels.stale;
    if (error.status === 429) return labels.limit;
    if (error.outcomeUnknown) return labels.unknown;
    if ([400, 403, 404].includes(error.status)) return labels.denied;
  }
  return writes ? labels.unknown : labels.down;
}

function useOperation() {
  const current = useRef<AbortController | null>(null);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");
  useEffect(() => () => current.current?.abort(), []);
  async function run<T>(task: (signal: AbortSignal) => Promise<T>, apply: (value: T) => void, writes = false) {
    if (current.current) return;
    const request = new AbortController();
    current.current = request;
    setBusy(true);
    setMessage("");
    try {
      const result = await task(request.signal);
      if (!request.signal.aborted) { apply(result); if (writes) setMessage(labels.saved); }
    } catch (error) {
      if (!request.signal.aborted) setMessage(errorMessage(error, writes));
    } finally {
      current.current = null;
      if (!request.signal.aborted) setBusy(false);
    }
  }
  return { run, busy, message };
}

function Notice({ busy, message }: { busy: boolean; message: string }) {
  return <p role="status">{busy ? labels.loading : message}</p>;
}

function Proof() {
  return <label>{labels.proof}<input name="proof" type="password" required maxLength={1024} autoComplete="current-password"/></label>;
}
function Confirm() {
  return <label className="api-confirm"><input type="checkbox" required/>{labels.confirm}</label>;
}

function Pages({ next, busy, load }: { next: string | null; busy: boolean; load: (after: string) => void }) {
  return <div className="api-actions">
    <button type="button" disabled={busy} onClick={() => load("0")}>{t.accountsFirst}</button>
    <button type="button" disabled={busy || !next} onClick={() => next && load(next)}>{t.accountsNext}</button>
  </div>;
}

function useAdmin(csrf: string) {
  return <K extends PublicAdminOperation>(operation: K, input: PublicAdminInput<K>, signal: AbortSignal) =>
    api<PublicAdminResult<K>>("admin", signal, csrf, { ...input, operation });
}

function AccountsPanel({ csrf, role }: { csrf: string; role: "owner" | "administrator" }) {
  const request = useAdmin(csrf);
  const activity = useOperation();
  const [staff, setStaff] = useState(false);
  const [accounts, setAccounts] = useState<PublicAdminResult<"accounts.list"> | null>(null);
  const [selected, setSelected] = useState<PublicAdminResult<"accounts.detail">["account"] | null>(null);
  type Action = "create" | "activate" | "disable" | "reset" | "role";
  const [action, setAction] = useState<Action>("create");

  function load(after: string, staffOnly = staff) {
    setSelected(null);
    setAction("create");
    setAccounts(null);
    void activity.run(signal => request(staffOnly ? "accounts.staff" : "accounts.list", { after }, signal), setAccounts);
  }
  function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const form = event.currentTarget;
    const data = new FormData(form);
    form.reset();
    const proof = String(data.get("proof")), password = String(data.get("password") ?? "");
    const apply = ({ account }: PublicAdminResult<"accounts.detail">) => {
      setSelected(account);
      setAction("activate");
      setAccounts(null);
    };
    if (action === "create") {
      void activity.run(signal => request("accounts.create", { name: String(data.get("name")), proof, password }, signal), apply, true);
    } else if (selected) {
      const target = { id: selected.id, revision: selected.revision, proof };
      if (action === "reset") void activity.run(signal => request("accounts.reset", { ...target, password }, signal), apply, true);
      else if (action === "role") void activity.run(signal => request("accounts.role", { ...target, role: String(data.get("role")) }, signal), apply, true);
      else void activity.run(signal => request(action === "activate" ? "accounts.activate" : "accounts.disable", target, signal), apply, true);
    }
  }
  const permitted = selected && roleRank[selected.role] < roleRank[role];
  return <section>
    <h3>{t.accountsTitle}</h3>
    <label className="api-confirm"><input type="checkbox" checked={staff} disabled={activity.busy}
      onChange={event => { setStaff(event.target.checked); load("0", event.target.checked); }}/>{local("Staff only", "Tylko personel")}</label>
    <Pages next={accounts?.next_after ?? null} busy={activity.busy} load={load}/>
    {accounts && <div className="api-table"><table><thead><tr>
      <th>{t.accountsName}</th><th>{t.accountsRole}</th><th>{t.accountsStatus}</th><th>{t.accountsRevision}</th>
    </tr></thead><tbody>{accounts.accounts.map(account =>
      <tr key={account.id}><td><button type="button" disabled={activity.busy} onClick={() => {
        setSelected(null);
        void activity.run(signal => request("accounts.detail", { id: account.id }, signal),
          ({ account }) => { setSelected(account); setAction(roleRank[account.role] < roleRank[role] ? "activate" : "create"); });
      }}>{account.name}</button></td><td>{roleLabel(account.role)}</td>
        <td>{account.status === "active" ? t.enabled : t.disabled}</td><td>{account.revision}</td></tr>
    )}</tbody></table>{!accounts.accounts.length && <p>{t.accountsEmpty}</p>}</div>}
    {selected && <p>{t.accountsSelected} <strong>{selected.name}</strong> · {roleLabel(selected.role)} · {t.accountsRevision} {selected.revision}</p>}
    <form onSubmit={submit}>
      <fieldset disabled={activity.busy}>
        <legend>{t.accountsAction}</legend>
        <label>{t.accountsAction}<select value={action} onChange={event => setAction(event.target.value as Action)}>
          <option value="create">{t.accountsCreate}</option>
          {permitted && <>
            <option value="activate">{t.accountsActivate}</option><option value="disable">{t.accountsDisable}</option>
            <option value="reset">{t.accountsReset}</option><option value="role">{t.accountsChangeRole}</option>
          </>}
        </select></label>
        {action === "create" && <label>{t.accountsName}<input name="name" required maxLength={64} autoComplete="off"/></label>}
        {(action === "create" || action === "reset") && <label>{local("New account password", "Nowe hasło konta")}
          <input name="password" type="password" required minLength={15} maxLength={1024} autoComplete="new-password"/>
        </label>}
        {action === "create" && <p>{local("Creates a disabled account without email enrollment.", "Tworzy wyłączone konto bez rejestracji adresu e-mail.")}</p>}
        {action === "role" && <label>{t.accountsNewRole}<select name="role">
          {ACCOUNT_ROLES.filter(candidate => roleRank[candidate] < roleRank[role]).map(candidate =>
            <option key={candidate} value={candidate}>{roleLabel(candidate)}</option>)}
        </select></label>}
        <Proof/><Confirm/><button type="submit">{local("Save", "Zapisz")}</button>
      </fieldset>
    </form>
    <Notice {...activity}/>
  </section>;
}

function BansPanel({ csrf }: { csrf: string }) {
  const request = useAdmin(csrf);
  const activity = useOperation();
  const [rows, setRows] = useState<PublicAdminResult<"bans.list"> | null>(null);
  const [selected, setSelected] = useState<PublicAdminResult<"bans.create">["ban"] | null>(null);
  const [filter, setFilter] = useState("all");
  const [expiry, setExpiry] = useState("permanent");
  const load = (after: string, state = filter) => {
    setRows(null);
    setSelected(null);
    void activity.run(signal => request("bans.list", { after, state }, signal), setRows);
  };
  function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const form = event.currentTarget;
    const input = new FormData(form);
    const get = (key: string) => String(input.get(key) ?? "");
    form.reset();
    const apply = ({ ban }: PublicAdminResult<"bans.create">) => { setSelected(ban); setRows(null); };
    if (selected) void activity.run(signal => request("bans.revoke", {
      proof: get("proof"), id: selected.id, revision: selected.revision,
    }, signal), apply, true);
    else void activity.run(signal => request("bans.create", {
      proof: get("proof"), kind: get("kind"), subject: get("subject"),
      reason: get("reason"), expiry: expiry === "permanent" ? "permanent" : expiry === "duration" ?
        "duration:" + get("duration_seconds") : "until:" + Math.floor(new Date(get("expires_at")).getTime() / 1000),
    }, signal), apply, true);
  }
  return <section>
    <h3>{t.bansTitle}</h3>
    <p>{local("Changes are saved to the account database. Enforcement requires a hub using that database and a refreshed ban snapshot.",
      "Zmiany trafiają do bazy kont. Wyegzekwowanie bana wymaga huba korzystającego z tej bazy i odświeżenia listy banów.")}</p>
    <label>{t.bansFilter}<select value={filter} disabled={activity.busy} onChange={event => {
      const state = event.target.value; setFilter(state); load("0", state);
    }}>{BAN_FILTERS.map(state => <option key={state} value={state}>{t[`bansFilter_${state}`]}</option>)}</select></label>
    <Pages next={rows?.next_after ?? null} busy={activity.busy} load={load}/>
    {rows && <div className="api-table"><table><thead><tr><th>ID</th><th>{t.bansKind}</th><th>{t.bansSubject}</th>
      <th>{t.bansReason}</th><th>{t.accountsStatus}</th></tr></thead>
      <tbody>{rows.bans.map(ban => <tr key={ban.id}><td><button type="button" disabled={activity.busy}
        onClick={() => setSelected(ban)}>{ban.id}</button></td><td>{t[`ban_${ban.kind}`]}</td>
        <td>{ban.subject}</td><td>{ban.reason}</td><td>{t[`bansState_${banState(ban, rows.observed_at ?? null)}`]}</td></tr>)}</tbody>
    </table></div>}
    {selected && <div className="api-actions"><p>{t.accountsSelected} <strong>{selected.id}</strong> · {selected.subject}</p>
      <button type="button" disabled={activity.busy} onClick={() => setSelected(null)}>{t.bansCreate}</button></div>}
    <form onSubmit={submit}><fieldset disabled={activity.busy || !!selected?.revoked_at}>
      <legend>{selected ? t.bansRevoke : t.bansCreate}</legend>
      {!selected && <>
        <label>{t.bansKind}<select name="kind">{(["account", "cid", "ip"] as const).map(kind =>
          <option key={kind} value={kind}>{t[`ban_${kind}`]}</option>)}</select></label>
        <label>{t.bansSubject}<input name="subject" required maxLength={64}/></label>
        <label>{t.bansReason}<input name="reason" required maxLength={256}/></label>
        <label>{t.bansExpiryMode}<select value={expiry} onChange={event => setExpiry(event.target.value)}>
          <option value="permanent">{t.bansPermanent}</option><option value="duration">{t.bansForDuration}</option>
          <option value="until">{t.bansUntil}</option>
        </select></label>
        {expiry === "duration" && <label>{local("Duration in seconds", "Czas w sekundach")}
          <input name="duration_seconds" type="number" min="1" max="31536000" required/></label>}
        {expiry === "until" && <label>{t.bansExpiry}<input name="expires_at" type="datetime-local" required/></label>}
      </>}
      <Proof/><Confirm/><button type="submit">{selected ? t.bansRevoke : t.bansCreate}</button>
    </fieldset></form>
    <Notice {...activity}/>
  </section>;
}

function AuditPanel({ csrf }: { csrf: string }) {
  const request = useAdmin(csrf);
  const activity = useOperation();
  const [page, setPage] = useState<PublicAdminResult<"audit.list"> | null>(null);
  function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const form = event.currentTarget;
    const data = new FormData(form, (event.nativeEvent as SubmitEvent).submitter);
    form.reset();
    setPage(null);
    void activity.run(signal => request("audit.list",
      { after: String(data.get("after") ?? "0"), proof: String(data.get("proof")) }, signal), setPage);
  }
  return <section><h3>{t.auditTitle}</h3>
    <form onSubmit={submit}><fieldset disabled={activity.busy}>
      <Proof/><div className="api-actions"><button type="submit" name="after" value="0">{t.auditShow}</button>
      {page?.next_after && <button type="submit" name="after" value={page.next_after}>{t.accountsNext}</button>}</div>
    </fieldset></form>
    {page && <div className="api-table"><table><thead><tr><th>{t.auditWhen}</th><th>{t.auditActor}</th>
      <th>{t.auditAction}</th><th>{t.auditTarget}</th><th>{t.auditOutcome}</th></tr></thead>
      <tbody>{page.events.map(event => <tr key={event.id}><td>{new Date(event.occurred_at * 1000).toLocaleString(locale)}</td>
        <td>{event.actor_id}</td><td>{event.action}</td><td>{event.target_kind} {event.target_id}</td><td>{event.outcome}</td></tr>)}</tbody>
    </table>{!page.events.length && <p>{t.auditEmpty}</p>}</div>}
    <Notice {...activity}/>
  </section>;
}

type Presence = { truncated: boolean; users: { nickname: string; role: keyof typeof roleRank | "guest";
  transport: "adc" | "adcs"; public_muted: boolean; private_muted: boolean }[] };

function HubPanel() {
  const activity = useOperation();
  const [status, setStatus] = useState<HubSnapshot | null>(null);
  const [presence, setPresence] = useState<Presence | null>(null);
  const [health, setHealth] = useState<"ok" | "unavailable">("unavailable");
  const [ready, setReady] = useState<"ready" | "unavailable">("unavailable");
  function load() {
    void activity.run(async signal => {
      const [snapshot, users, liveness, readiness] = await Promise.allSettled([
        api<HubSnapshot>("hub/status", signal), api<Presence>("hub/users", signal),
        api<{ status: "ok" }>("hub/health", signal), api<{ status: "ready" }>("hub/ready", signal),
      ]);
      return { snapshot, users, liveness, readiness };
    }, ({ snapshot, users, liveness, readiness }) => {
      setStatus(snapshot.status === "fulfilled" ? snapshot.value : null);
      setPresence(users.status === "fulfilled" ? users.value : null);
      setHealth(liveness.status === "fulfilled" ? liveness.value.status : "unavailable");
      setReady(readiness.status === "fulfilled" ? readiness.value.status : "unavailable");
    });
  }
  const sections: [string, ReactNode][] = status ? [
    [local("Overview and diagnostics", "Przegląd i diagnostyka"), <Dashboard snapshot={status} t={t} locale={locale}/>],
    [local("Chat", "Czat"), <ChatStatistics snapshot={status} t={t} number={number}/>],
    [local("Search", "Wyszukiwanie"), <SearchStatistics snapshot={status} t={t} number={number}/>],
    [local("Transfers", "Transfery"), <TransferStatistics snapshot={status} t={t} number={number}/>],
    [local("Moderation", "Moderacja"), <><KickStatistics snapshot={status} t={t}/><MuteStatistics snapshot={status} t={t}/>
      <ModerationRules snapshot={status} t={t}/></>],
    [local("Effective configuration and roles", "Efektywna konfiguracja i role"),
      <><StartupConfiguration snapshot={status} t={t} number={number}/><Roles snapshot={status} t={t}/></>],
  ] : [];
  return <section>
    <div className="api-heading"><h3>{local("Live ADCS hub", "Hub ADCS na żywo")}</h3>
      <button type="button" disabled={activity.busy} onClick={load}>{labels.refresh}</button></div>
    <p>{local("Liveness", "Żywotność")} {health === "ok" ? "OK" : "—"} · {local("Readiness", "Gotowość")} {ready === "ready" ? "OK" : "—"}</p>
    {!status && !activity.busy && <p>{local("Live hub telemetry is unavailable.", "Dane bieżące huba są niedostępne.")}</p>}
    {sections.map(([title, contents]) => <details key={title}><summary>{title}</summary>{contents}</details>)}
    <details><summary>{local("Online users", "Użytkownicy online")}</summary>
      {!presence && <p>{labels.down}</p>}
      {presence?.truncated && <p>{local("This is a bounded snapshot.", "Lista zawiera ograniczoną migawkę.")}</p>}
      {presence?.users.map((user, index) => <p key={index}>{user.nickname} · {user.transport.toUpperCase()} ·
        {" "}{t[`role_${user.role}`]}{user.public_muted || user.private_muted ? " · " + t.mute : ""}</p>)}
    </details>
    <Notice {...activity}/>
  </section>;
}

function PermissionsPanel({ csrf }: { csrf: string }) {
  const request = useAdmin(csrf);
  const activity = useOperation();
  const [catalog, setCatalog] = useState<PublicAdminResult<"management"> | null>(null);
  const [stats, setStats] = useState<PublicAdminResult<"admin"> | null>(null);
  return <section><h3>{local("Permissions and diagnostics", "Uprawnienia i diagnostyka")}</h3>
    <button type="button" disabled={activity.busy} onClick={() => {
      setCatalog(null);
      setStats(null);
      void activity.run(signal => Promise.all([request("management", {}, signal), request("admin", {}, signal)]),
        ([catalog, stats]) => { setCatalog(catalog); setStats(stats); });
    }}>{labels.refresh}</button>
    {catalog && <ul>{catalog.areas.map(area => <li key={area.key}>{area.key} — {
      area.key === "configuration" || area.key === "hub" ? local("Operator controls restricted to private tools", "Sterowanie dostępne w narzędziach prywatnych") :
        area.allowed ? t.enabled : t.disabled}</li>)}</ul>}
    {stats && <ul>{Object.entries(stats.statistics).map(([key, value]) => <li key={key}>{key}: {value}</li>)}</ul>}
    <Notice {...activity}/>
  </section>;
}

export function Operations({ csrf, role }: { csrf: string; role: "owner" | "administrator" }) {
  const names = { hub: local("Live hub", "Hub na żywo"), accounts: t.accountsTitle, bans: t.bansTitle,
    audit: t.auditTitle, permissions: local("Permissions", "Uprawnienia") };
  const [section, setSection] = useState<keyof typeof names>("hub");
  return <>
    <nav className="api-actions" aria-label={local("Administration", "Administracja")}>
      {(Object.keys(names) as (keyof typeof names)[]).map(key =>
        <button type="button" key={key} aria-pressed={section === key} onClick={() => setSection(key)}>{names[key]}</button>)}
    </nav>
    {section === "hub" && <HubPanel/>}
    {section === "accounts" && <AccountsPanel csrf={csrf} role={role}/>}
    {section === "bans" && <BansPanel csrf={csrf}/>}
    {section === "audit" && <AuditPanel csrf={csrf}/>}
    {section === "permissions" && <PermissionsPanel csrf={csrf}/>}
  </>;
}
