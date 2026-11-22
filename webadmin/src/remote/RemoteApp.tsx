import { useEffect, useRef, useState } from "react";

type Language = "us" | "pl";
type Summary = {
  schema: 1;
  mode: "read-only";
  server: { version: string; running: boolean; uptime_seconds: number };
  network: { active_connections: number; accepted_connections: number };
  users: { online: number; peak_online: number };
  chat: { received: number; rejected: number };
  search?: { queries: number; results: number };
  transfers?: { ctm: number; rcm: number };
};
type Session = { schema: 1; authenticated: true; mode: "read-only"; csrf: string; expires_in: number };
type Status = "loading" | "unauthorized" | "authorized" | "forbidden" | "unavailable" | "expired" | "rate-limited";

const labels = {
  us: {
    title: "Remote WebAdmin", mode: "Read-only · Local access", id: "Administrator account ID",
    password: "Account password", signIn: "Sign in", signOut: "Sign out",
    loginHint: "Use an active administrator or Owner account. No service token is stored in the browser.",
    loading: "Loading current status…", unauthorized: "Sign in to read approved hub metrics.",
    forbidden: "This account is not authorized for remote read-only access.",
    unavailable: "The gateway or hub API is unavailable. Access is denied until it recovers.",
    expired: "Your session expired or was revoked. Sign in again.",
    limited: "Too many attempts. Wait before trying again.",
    version: "Hub version", running: "Hub running", uptime: "Uptime (seconds)",
    connections: "Active connections", accepted: "Accepted connections",
    online: "Online users", peak: "Peak online", chat: "Chat received",
    rejected: "Chat rejected", queries: "Search queries", results: "Search results",
    ctm: "CTM requests", rcm: "RCM requests", refresh: "Refresh",
    yes: "Yes", no: "No", noData: "Optional metrics unavailable",
  },
  pl: {
    title: "WebAdmin zdalny", mode: "Tylko odczyt · Dostęp lokalny", id: "ID konta administratora",
    password: "Hasło konta", signIn: "Zaloguj", signOut: "Wyloguj",
    loginHint: "Użyj aktywnego konta administratora lub Ownera. Token usługi nie jest zapisany w przeglądarce.",
    loading: "Pobieranie aktualnego stanu…", unauthorized: "Zaloguj się, aby odczytać zatwierdzone dane huba.",
    forbidden: "To konto nie ma uprawnienia do zdalnego odczytu.",
    unavailable: "Brama lub API huba jest niedostępne. Dostęp jest odrzucony do czasu naprawy.",
    expired: "Sesja wygasła albo została unieważniona. Zaloguj się ponownie.",
    limited: "Zbyt wiele prób. Poczekaj przed kolejną.",
    version: "Wersja huba", running: "Hub działa", uptime: "Czas działania (sekundy)",
    connections: "Aktywne połączenia", accepted: "Przyjęte połączenia",
    online: "Użytkownicy online", peak: "Szczyt online", chat: "Wiadomości odebrane",
    rejected: "Wiadomości odrzucone", queries: "Zapytania wyszukiwania", results: "Wyniki wyszukiwania",
    ctm: "Żądania CTM", rcm: "Żądania RCM", refresh: "Odśwież",
    yes: "Tak", no: "Nie", noData: "Opcjonalne dane niedostępne",
  },
} as const;

async function call(path: string, init?: RequestInit): Promise<{ status: number; data: unknown }> {
  const answer = await fetch("/remote/v1/" + path, {
    credentials: "same-origin", cache: "no-store",
    ...init,
  });
  const data: unknown = await answer.json();
  return { status: answer.status, data };
}
const validSession = (value: unknown): value is Session => {
  if (!value || typeof value !== "object") return false;
  const s = value as Record<string, unknown>;
  return s.schema === 1 && s.authenticated === true && s.mode === "read-only" &&
    typeof s.csrf === "string" && /^[0-9a-f]{64}$/.test(s.csrf) &&
    typeof s.expires_in === "number" && s.expires_in > 0 && s.expires_in <= 600;
};
const validSummary = (value: unknown): value is Summary => {
  if (!value || typeof value !== "object") return false;
  const s = value as Record<string, unknown>;
  return s.schema === 1 && s.mode === "read-only" && !!s.server && !!s.network &&
    !!s.users && !!s.chat;
};
function failure(status: number): Status {
  if (status === 403) return "forbidden";
  if (status === 429) return "rate-limited";
  if (status === 401) return "expired";
  return "unavailable";
}

export function RemoteApp() {
  const [language, setLanguage] = useState<Language>(navigator.language.toLowerCase().startsWith("pl") ? "pl" : "us");
  const [state, setState] = useState<Status>("loading");
  const [csrf, setCsrf] = useState("");
  const [actor, setActor] = useState("");
  const [password, setPassword] = useState("");
  const [summary, setSummary] = useState<Summary | null>(null);
  // An older HTTP response must never modify a newer authenticated session.
  const sessionEpoch = useRef(0);
  const t = labels[language];

  const invalidate = (next: Status) => {
    sessionEpoch.current++;
    setSummary(null);
    setCsrf("");
    setState(next);
  };
  useEffect(() => {
    let canceled = false;
    call("session").then(({ status, data }) => {
      if (canceled) return;
      if (status === 200 && validSession(data)) { setCsrf(data.csrf); setState("authorized"); }
      else setState(status === 401 ? "unauthorized" : failure(status));
    }).catch(() => { if (!canceled) setState("unavailable"); });
    return () => { canceled = true; };
  }, []);
  const load = async () => {
    const epoch = sessionEpoch.current;
    try {
      const { status, data } = await call("summary");
      if (epoch !== sessionEpoch.current) return;
      if (status === 200 && validSummary(data)) { setSummary(data); return; }
      invalidate(status === 200 ? "unavailable" : failure(status));
    } catch {
      if (epoch === sessionEpoch.current) invalidate("unavailable");
    }
  };
  useEffect(() => {
    if (state !== "authorized") return;
    void load();
    const timer = window.setInterval(() => { void load(); }, 15000);
    return () => {
      window.clearInterval(timer);
      sessionEpoch.current++;
    };
    // Session changes drive polling; no retry is scheduled on auth failure.
  }, [state]);
  const login = async (event: React.FormEvent) => {
    event.preventDefault();
    const proof = password;
    setPassword(""); // Do not retain a password after a failed challenge.
    invalidate("loading");
    try {
      const challenge = await call("challenge");
      if (challenge.status !== 200 || !challenge.data || typeof challenge.data !== "object" ||
        !("csrf" in challenge.data) || typeof challenge.data.csrf !== "string") {
        setState(failure(challenge.status)); return;
      }
      const result = await call("login", {
        method: "POST", headers: {
          "Content-Type": "application/json", "X-DC24H-Remote-CSRF": challenge.data.csrf,
        }, body: JSON.stringify({ actor_id: actor, password: proof }),
      });
      if (result.status === 200 && validSession(result.data)) {
        setCsrf(result.data.csrf); setState("authorized");
      } else { invalidate(result.status === 200 ? "unavailable" : failure(result.status)); }
    } catch { invalidate("unavailable"); }
  };
  const logout = async () => {
    if (!csrf) return;
    const token = csrf;
    invalidate("loading");
    try {
      const result = await call("logout", {
        method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": token },
        body: "{}",
      });
      if (result.status !== 200) { invalidate(failure(result.status)); return; }
      invalidate("unauthorized"); setActor("");
    } catch { invalidate("unavailable"); }
  };
  const rows: [string, string | number][] = summary ? [
    [t.version, summary.server.version], [t.running, summary.server.running ? t.yes : t.no],
    [t.uptime, summary.server.uptime_seconds], [t.connections, summary.network.active_connections],
    [t.accepted, summary.network.accepted_connections], [t.online, summary.users.online],
    [t.peak, summary.users.peak_online], [t.chat, summary.chat.received],
    [t.rejected, summary.chat.rejected],
    ...(summary.search ? [[t.queries, summary.search.queries], [t.results, summary.search.results]] as [string, number][] : []),
    ...(summary.transfers ? [[t.ctm, summary.transfers.ctm], [t.rcm, summary.transfers.rcm]] as [string, number][] : []),
  ] : [];
  return (
    <main className="remote-shell">
      <header className="remote-head">
        <div><strong>dc24h.eu</strong><h1>{t.title}</h1><p>{t.mode}</p></div>
        <div className="remote-actions">
          <button type="button" aria-label="English" onClick={() => setLanguage("us")}>EN</button>
          <button type="button" aria-label="Polski" onClick={() => setLanguage("pl")}>PL</button>
          {state === "authorized" && <button type="button" onClick={() => void logout()}>{t.signOut}</button>}
        </div>
      </header>
      <section className="remote-panel" aria-live="polite">
        {state !== "authorized" && <p role="status">{state === "loading" ? t.loading : state === "unauthorized" ? t.unauthorized :
          state === "forbidden" ? t.forbidden : state === "expired" ? t.expired :
          state === "rate-limited" ? t.limited : t.unavailable}</p>}
        {state !== "authorized" && state !== "loading" && <form onSubmit={(event) => { void login(event); }}>
          <label>{t.id}<input name="actor" inputMode="numeric" autoComplete="username"
            required pattern="[1-9][0-9]*" value={actor} onChange={(event) => setActor(event.target.value)} /></label>
          <label>{t.password}<input name="password" type="password" autoComplete="current-password"
            required value={password} onChange={(event) => setPassword(event.target.value)} /></label>
          <p className="remote-hint">{t.loginHint}</p>
          <button type="submit">{t.signIn}</button>
        </form>}
        {state === "authorized" && <>
          <div className="remote-toolbar"><span>{t.mode}</span><button type="button" onClick={() => void load()}>{t.refresh}</button></div>
          {summary ? <dl className="remote-grid">{rows.map(([key, value]) =>
            <div key={key}><dt>{key}</dt><dd>{value}</dd></div>)}</dl> : <p>{t.loading}</p>}
          {summary && !summary.search && !summary.transfers && <p className="remote-hint">{t.noData}</p>}
        </>}
      </section>
    </main>
  );
}
