import { useEffect, useRef, useState } from "react";
import { startAuthentication, startRegistration } from "@simplewebauthn/browser";
import { parseRemoteSummaryResponse } from "../lib/remote-status.ts";
import type { Summary } from "../lib/remote-status.ts";

type Language = "us" | "pl";
type Session = { schema: 1; authenticated: true; mfa: true; mode: "read-only"; csrf: string; expires_in: number };
type Status = "loading" | "unauthorized" | "authorized" | "forbidden" | "unavailable" | "expired" | "rate-limited";
type Device = { id: string; name: string; device_type: "singleDevice" | "multiDevice";
  backed_up: boolean; created_at: number; last_used_at: number | null };

const labels = {
  us: {
    title: "Remote WebAdmin", mode: "Read-only · Local access", id: "Administrator account ID",
    password: "Account password", otp: "Authenticator code (6 digits)", signIn: "Sign in", signOut: "Sign out",
    loginHint: "Use an active administrator or Owner account. Sign in with password + TOTP, or password + an enrolled passkey. No service token is stored in the browser.",
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
    passkeySignIn: "Sign in with passkey", devices: "Passkeys / trusted devices",
    loadDevices: "Refresh devices", addPasskey: "Add passkey", rename: "Rename",
    remove: "Revoke", deviceName: "Device name", noDevices: "No passkeys enrolled yet.",
    passkeyHelp: "Passkeys require your administrator password plus an origin-bound device confirmation. Adding or changing devices requires a fresh sign-in (within 60 seconds).",
    actionFailed: "Passkey operation was refused. Sign in again if recent verification is required.",
    devicesUnavailable: "Device list is unavailable. No cached device data is shown; retry after the gateway recovers.",
    sessionRemaining: "Session time remaining (approximate)", sessionUnverified: "Session lifetime is being revalidated.",
    minutes: "min", created: "Added", lastUsed: "Last used", neverUsed: "Not used yet",
    confirmRevoke: "Confirm revocation", cancel: "Cancel",
    revokeWarning: "Revoke this passkey? This immediately ends active sessions for the account and cannot be undone here.",
    actionDone: "Passkey operation completed.", revoked: "The key was revoked and sessions were ended. Sign in again.",
    incidentTitle: "Private incident overview", gatewayReady: "Authenticated gateway read",
    serviceReady: "Hub reported running", lastUpdated: "Read age", chatDenied: "Rejected chat total",
    maintenance: "Recovery / maintenance", maintenanceUnknown: "Not exposed by approved read-only projection",
    incidentReady: "Verified", incidentDegraded: "Degraded", stale: "Read expired; old counters are hidden until a verified refresh.",
    sourceUnknown: "Unknown",
    timeline: "Recent aggregate service events", noEvents: "No verified transitions recorded in this process.",
    incidentUnknown: "Incident history is unavailable or not yet verified; this does not mean the service is healthy.",
    retainedTitle: "Retained local incident history (not live)",
    retainedUnknown: "Retained incident history is unavailable or untrusted.",
    retainedEmpty: "No retained aggregate events. This is not a complete forensic record.",
    retainedLimit: "Owner-only, capped local history. A gateway restart or lost audit does not verify service health.",
    alertTitle: "Aggregate security signals", hubStarted: "Hub started", hubStopped: "Hub stopped",
    transportRefused: "Transport security refusal threshold reached", loginRefused: "Authentication refusal threshold reached",
    rateLimitRefused: "Rate-limit refusal threshold reached", secondsAgo: "seconds ago",
    acknowledge: "Acknowledge locally", acknowledged: "Acknowledged on this screen only; no server action was sent.",
  },
  pl: {
    title: "WebAdmin zdalny", mode: "Tylko odczyt · Dostęp lokalny", id: "ID konta administratora",
    password: "Hasło konta", otp: "Kod uwierzytelniający (6 cyfr)", signIn: "Zaloguj", signOut: "Wyloguj",
    loginHint: "Użyj aktywnego konta administratora lub Ownera. Zaloguj się hasłem + TOTP albo hasłem + zarejestrowanym passkey. Token usługi nie jest zapisany w przeglądarce.",
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
    passkeySignIn: "Zaloguj przez passkey", devices: "Passkeys / zaufane urządzenia",
    loadDevices: "Odśwież urządzenia", addPasskey: "Dodaj passkey", rename: "Zmień nazwę",
    remove: "Unieważnij", deviceName: "Nazwa urządzenia", noDevices: "Brak dodanych kluczy passkey.",
    passkeyHelp: "Passkeys wymagają hasła administratora i potwierdzenia urządzeniem powiązanym z domeną. Zmiana urządzeń wymaga świeżego logowania (do 60 sekund).",
    actionFailed: "Operacja passkey odrzucona. Zaloguj się ponownie, jeśli wymagane jest świeże potwierdzenie.",
    devicesUnavailable: "Lista urządzeń jest niedostępna. Nie pokazujemy starych danych; spróbuj po przywróceniu bramy.",
    sessionRemaining: "Pozostały czas sesji (orientacyjny)", sessionUnverified: "Trwa ponowna weryfikacja czasu sesji.",
    minutes: "min", created: "Dodano", lastUsed: "Ostatnio użyto", neverUsed: "Jeszcze nie użyto",
    confirmRevoke: "Potwierdź unieważnienie", cancel: "Anuluj",
    revokeWarning: "Unieważnić ten passkey? Spowoduje to zakończenie aktywnych sesji konta; tej operacji nie da się tutaj cofnąć.",
    actionDone: "Operacja passkey zakończona.", revoked: "Klucz unieważniono i zakończono sesje. Zaloguj się ponownie.",
    incidentTitle: "Prywatny przegląd incydentów", gatewayReady: "Uwierzytelniony odczyt bramy",
    serviceReady: "Hub zgłasza pracę", lastUpdated: "Wiek odczytu", chatDenied: "Łączna liczba odrzuconych wiadomości",
    maintenance: "Odtwarzanie / konserwacja", maintenanceUnknown: "Niedostępne w zatwierdzonym widoku tylko do odczytu",
    incidentReady: "Zweryfikowano", incidentDegraded: "Pogorszenie", stale: "Odczyt nieaktualny; poprzednie liczniki ukryto do czasu weryfikacji.",
    sourceUnknown: "Nieznane",
    timeline: "Ostatnie zbiorcze zdarzenia usługi", noEvents: "Brak zweryfikowanych zmian stanu w tym procesie.",
    incidentUnknown: "Historia incydentów jest niedostępna albo jeszcze niezweryfikowana; nie oznacza to poprawnego działania usługi.",
    retainedTitle: "Zachowana lokalna historia incydentów (nie na żywo)",
    retainedUnknown: "Zachowana historia incydentów jest niedostępna lub niewiarygodna.",
    retainedEmpty: "Brak zachowanych zdarzeń zbiorczych. To nie jest kompletny dziennik śledczy.",
    retainedLimit: "Prywatna, ograniczona historia. Restart bramki lub utrata audytu nie potwierdzają poprawnej pracy usługi.",
    alertTitle: "Zbiorcze sygnały bezpieczeństwa", hubStarted: "Hub uruchomiony", hubStopped: "Hub zatrzymany",
    transportRefused: "Próg odmów bezpieczeństwa transportu został osiągnięty",
    loginRefused: "Próg odmów uwierzytelniania został osiągnięty",
    rateLimitRefused: "Próg odmów limitera został osiągnięty", secondsAgo: "sekund temu",
    acknowledge: "Potwierdź lokalnie", acknowledged: "Potwierdzono tylko na tym ekranie; nie wysłano żadnej akcji do serwera.",
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
  return s.schema === 1 && s.authenticated === true && s.mfa === true && s.mode === "read-only" &&
    typeof s.csrf === "string" && /^[0-9a-f]{64}$/.test(s.csrf) &&
    Number.isSafeInteger(s.expires_in) && (s.expires_in as number) > 0 && (s.expires_in as number) <= 600;
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
  const [otp, setOtp] = useState("");
  const [summary, setSummary] = useState<Summary | null>(null);
  const [summaryFetchedAt, setSummaryFetchedAt] = useState<number | null>(null);
  const [devices, setDevices] = useState<Device[] | null>(null);
  const [deviceLabels, setDeviceLabels] = useState<Record<string, string>>({});
  const [pendingRevoke, setPendingRevoke] = useState<string | null>(null);
  const [deviceMutationBusy, setDeviceMutationBusy] = useState<string | null>(null);
  const [sessionExpiresAt, setSessionExpiresAt] = useState<number | null>(null);
  // A wall-clock adjustment must not extend the displayed lifetime of a
  // server-issued session. Use a monotonic browser clock for the countdown.
  const [clock, setClock] = useState(() => performance.now());
  const [actionMessage, setActionMessage] = useState("");
  const [incidentAcknowledged, setIncidentAcknowledged] = useState(false);
  // An older HTTP response must never modify a newer authenticated session.
  const sessionEpoch = useRef(0);
  // A prior refresh must also not replace the latest result within a session.
  const summaryRequest = useRef(0);
  const deviceRequest = useRef(0);
  // Same-origin tabs can hold an already-rendered snapshot when a sibling
  // signs out. Broadcast only a fixed invalidation signal, never session or
  // CSRF material. The server still independently authorizes every request.
  const sessionChannel = useRef<BroadcastChannel | null>(null);
  const t = labels[language];
  const displayTime = (time: number | null) => time === null ? t.neverUsed :
    new Date(time).toLocaleString(language === "pl" ? "pl-PL" : "en-US");
  const updateSessionLifetime = (seconds: number) => {
    const now = performance.now();
    setClock(now);
    setSessionExpiresAt(now + seconds * 1000);
  };

  const invalidate = (next: Status) => {
    sessionEpoch.current++;
    summaryRequest.current++;
    deviceRequest.current++;
    setSummary(null);
    setSummaryFetchedAt(null);
    setDevices(null);
    setPendingRevoke(null);
    setDeviceMutationBusy(null);
    setSessionExpiresAt(null);
    setActionMessage("");
    setIncidentAcknowledged(false);
    setCsrf("");
    setState(next);
  };
  useEffect(() => {
    if (typeof BroadcastChannel === "undefined") return;
    const channel = new BroadcastChannel("dc24h-remote-session");
    sessionChannel.current = channel;
    channel.onmessage = event => {
      if (event.data === "session-revoked") invalidate("expired");
    };
    return () => { sessionChannel.current = null; channel.close(); };
  }, []);
  useEffect(() => {
    let canceled = false;
    call("session").then(({ status, data }) => {
      if (canceled) return;
      if (status === 200 && validSession(data)) {
        setCsrf(data.csrf); updateSessionLifetime(data.expires_in);
        setState("authorized");
      }
      else setState(status === 401 ? "unauthorized" : failure(status));
    }).catch(() => { if (!canceled) setState("unavailable"); });
    return () => { canceled = true; };
  }, []);
  useEffect(() => {
    // Incident and metric reads expire after 45 seconds. Re-evaluate freshness
    // every second so a hung refresh cannot extend the display by 30 seconds.
    const timer = window.setInterval(() => setClock(performance.now()), 1_000);
    return () => window.clearInterval(timer);
  }, []);
  const load = async () => {
    const epoch = sessionEpoch.current;
    const requestId = ++summaryRequest.current;
    try {
      const { status, data } = await call("summary");
      if (epoch !== sessionEpoch.current || requestId !== summaryRequest.current) return;
      const parsed = status === 200 ? parseRemoteSummaryResponse(data) : null;
      if (parsed) {
        const fetchedAt = performance.now();
        setClock(fetchedAt); setSummaryFetchedAt(fetchedAt); setSummary(parsed);
        // An acknowledgment belongs to what was on screen, not to a future
        // source refresh. Never persist it or send a mutation to the hub.
        setIncidentAcknowledged(false);
        return;
      }
      invalidate(status === 200 ? "unavailable" : failure(status));
    } catch {
      if (epoch === sessionEpoch.current && requestId === summaryRequest.current) invalidate("unavailable");
    }
  };
  const acceptSessionRotation = (nextCsrf: string) => {
    // A privileged passkey change replaced the server-side cookie. Reads
    // issued with the old cookie may still finish with 401/403: they must
    // not clear the newly authenticated session or its new CSRF secret.
    const epoch = ++sessionEpoch.current;
    summaryRequest.current++;
    deviceRequest.current++;
    setCsrf(nextCsrf);
    setSessionExpiresAt(null); // Rotation may change the expiry; never invent a new lifetime.
    setPendingRevoke(null);
    setDeviceMutationBusy(null);
    setSummary(null);
    setSummaryFetchedAt(null);
    setIncidentAcknowledged(false);
    // Only the server can establish the lifetime of the rotated cookie. A
    // stale or failed inspection cannot overwrite the new CSRF identifier.
    void call("session").then(({ status, data }) => {
      if (epoch !== sessionEpoch.current) return;
      if (status === 200 && validSession(data) && data.csrf === nextCsrf)
        updateSessionLifetime(data.expires_in);
      else if (status === 401) invalidate("expired");
    }).catch(() => { /* Remain unknown until the next explicit sign-in. */ });
    void load();
    return epoch;
  };
  useEffect(() => {
    if (state !== "authorized") return;
    void load();
    const timer = window.setInterval(() => { void load(); }, 15000);
    return () => {
      window.clearInterval(timer);
      // invalidate() and session rotation already advance the epoch. Advancing
      // it here races a pending logout when authorized -> loading rerenders.
    };
    // Session changes drive polling; no retry is scheduled on auth failure.
  }, [state]);
  const login = async (event: React.FormEvent) => {
    event.preventDefault();
    const proof = password;
    const code = otp;
    setPassword(""); // Do not retain a password after a failed challenge.
    setOtp("");
    invalidate("loading");
    const epoch = sessionEpoch.current;
    try {
      const challenge = await call("challenge");
      if (epoch !== sessionEpoch.current) return;
      if (challenge.status !== 200 || !challenge.data || typeof challenge.data !== "object" ||
        !("csrf" in challenge.data) || typeof challenge.data.csrf !== "string") {
        setState(failure(challenge.status)); return;
      }
      const result = await call("login", {
        method: "POST", headers: {
          "Content-Type": "application/json", "X-DC24H-Remote-CSRF": challenge.data.csrf,
        }, body: JSON.stringify({ actor_id: actor, password: proof, otp: code }),
      });
      if (epoch !== sessionEpoch.current) return;
      if (result.status === 200 && validSession(result.data)) {
        // A fresh cookie can replace the authority shared by sibling tabs.
        // Invalidate their cached metrics without broadcasting any session,
        // account, CSRF, password or TOTP material.
        sessionChannel.current?.postMessage("session-revoked");
        setCsrf(result.data.csrf); updateSessionLifetime(result.data.expires_in);
        setState("authorized");
      } else { invalidate(result.status === 200 ? "unavailable" : failure(result.status)); }
    } catch { if (epoch === sessionEpoch.current) invalidate("unavailable"); }
  };
  const logout = async () => {
    if (!csrf) return;
    const token = csrf;
    invalidate("loading");
    const epoch = sessionEpoch.current;
    try {
      const result = await call("logout", {
        method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": token },
        body: "{}",
      });
      if (epoch !== sessionEpoch.current) return;
      if (result.status !== 200) { invalidate(failure(result.status)); return; }
      sessionChannel.current?.postMessage("session-revoked");
      invalidate("unauthorized"); setActor("");
    } catch { if (epoch === sessionEpoch.current) invalidate("unavailable"); }
  };
  const passkeyRequest = (path: string, data: unknown, token: string) => call(path, {
    method: "POST", headers: { "Content-Type": "application/json", "X-DC24H-Remote-CSRF": token },
    body: JSON.stringify(data),
  });
  const passkeyLogin = async () => {
    const proof = password;
    setPassword(""); setOtp("");
    invalidate("loading");
    const epoch = sessionEpoch.current;
    try {
      const issued = await call("challenge");
      if (epoch !== sessionEpoch.current) return;
      if (issued.status !== 200 || !issued.data || typeof issued.data !== "object" ||
          !("csrf" in issued.data) || typeof issued.data.csrf !== "string") {
        invalidate(failure(issued.status)); return;
      }
      const token = issued.data.csrf;
      const options = await passkeyRequest("passkey/login/options", { actor_id: actor, password: proof }, token);
      if (epoch !== sessionEpoch.current) return;
      if (options.status !== 200) { invalidate(failure(options.status)); return; }
      const answer = await startAuthentication({ optionsJSON: options.data as Parameters<typeof startAuthentication>[0]["optionsJSON"] });
      if (epoch !== sessionEpoch.current) return;
      const result = await passkeyRequest("passkey/login/finish", { response: answer }, token);
      if (epoch !== sessionEpoch.current) return;
      if (result.status === 200 && validSession(result.data)) {
        sessionChannel.current?.postMessage("session-revoked");
        setCsrf(result.data.csrf); updateSessionLifetime(result.data.expires_in);
        setState("authorized");
      } else invalidate(failure(result.status));
    } catch { if (epoch === sessionEpoch.current) invalidate("unavailable"); }
  };
  const loadDevices = async () => {
    const epoch = sessionEpoch.current;
    const requestId = ++deviceRequest.current;
    try {
      const result = await call("passkey/devices");
      if (epoch !== sessionEpoch.current || requestId !== deviceRequest.current) return;
      if (result.status === 401 || result.status === 403) { invalidate(failure(result.status)); return; }
      if (result.status !== 200 || !result.data || typeof result.data !== "object" ||
          !("devices" in result.data) || !Array.isArray(result.data.devices)) throw Error();
      const list = result.data.devices as Device[];
      if (list.length > 8 || list.some(d => !d || typeof d.id !== "string" || typeof d.name !== "string")) throw Error();
      setDevices(list); setActionMessage("");
      setPendingRevoke(previous => list.some(device => device.id === previous) ? previous : null);
    } catch {
      if (epoch === sessionEpoch.current && requestId === deviceRequest.current) {
        setDevices(null); setPendingRevoke(null); setActionMessage(t.devicesUnavailable);
      }
    }
  };
  const enroll = async () => {
    if (!csrf) return;
    const epoch = sessionEpoch.current;
    setActionMessage("");
    try {
      const options = await passkeyRequest("passkey/enroll/options", {}, csrf);
      if (options.status !== 200) throw Error();
      const answer = await startRegistration({ optionsJSON: options.data as Parameters<typeof startRegistration>[0]["optionsJSON"] });
      if (epoch !== sessionEpoch.current) return;
      const result = await passkeyRequest("passkey/enroll/finish", { response: answer }, csrf);
      if (result.status !== 200) throw Error();
      if (epoch !== sessionEpoch.current) return;
      if (!result.data || typeof result.data !== "object" || !("csrf" in result.data) ||
          typeof result.data.csrf !== "string" || !/^[0-9a-f]{64}$/.test(result.data.csrf)) throw Error();
      const rotatedEpoch = acceptSessionRotation(result.data.csrf);
      await loadDevices();
      if (rotatedEpoch === sessionEpoch.current) setActionMessage(t.actionDone);
    } catch { if (epoch === sessionEpoch.current) setActionMessage(t.actionFailed); }
  };
  const changeDevice = async (id: string, revoke: boolean) => {
    if (!csrf || deviceMutationBusy) return;
    const epoch = sessionEpoch.current;
    setDeviceMutationBusy(id);
    try {
      const result = await passkeyRequest(revoke ? "passkey/device/revoke" : "passkey/device/rename",
        revoke ? { id } : { id, name: deviceLabels[id] ?? "" }, csrf);
      if (epoch !== sessionEpoch.current) return;
      if (result.status !== 200) throw Error();
      if (revoke) {
        sessionChannel.current?.postMessage("session-revoked");
        invalidate("unauthorized"); setActionMessage(t.revoked);
      }
      else {
        if (!result.data || typeof result.data !== "object" || !("csrf" in result.data) ||
            typeof result.data.csrf !== "string" || !/^[0-9a-f]{64}$/.test(result.data.csrf)) throw Error();
        const rotatedEpoch = acceptSessionRotation(result.data.csrf);
        setDeviceLabels(previous => ({ ...previous, [id]: "" }));
        await loadDevices();
        if (rotatedEpoch === sessionEpoch.current) setActionMessage(t.actionDone);
      }
    } catch { if (epoch === sessionEpoch.current) setActionMessage(t.actionFailed); }
    finally { if (epoch === sessionEpoch.current) setDeviceMutationBusy(null); }
  };
  // Independently validated C++ source age is optional for legacy v1; advance
  // it monotonically from receipt and never invent a recovery state.
  const freshSummary = summary && summaryFetchedAt !== null &&
    clock >= summaryFetchedAt &&
    clock - summaryFetchedAt + (summary.observation?.source_age_ms ?? 0) <= 45_000 ? summary : null;
  const rows: [string, string | number][] = freshSummary ? [
    [t.version, freshSummary.server.version], [t.running, freshSummary.server.running ? t.yes : t.no],
    [t.uptime, freshSummary.server.uptime_seconds], [t.connections, freshSummary.network.active_connections],
    [t.accepted, freshSummary.network.accepted_connections], [t.online, freshSummary.users.online],
    [t.peak, freshSummary.users.peak_online], [t.chat, freshSummary.chat.received],
    [t.rejected, freshSummary.chat.rejected],
    ...(freshSummary.search ? [[t.queries, freshSummary.search.queries], [t.results, freshSummary.search.results]] as [string, number][] : []),
    ...(freshSummary.transfers ? [[t.ctm, freshSummary.transfers.ctm], [t.rcm, freshSummary.transfers.rcm]] as [string, number][] : []),
  ] : [];
  const eventName = (kind: "hub_started" | "hub_stopped" | "transport_refusals" | "login_refusals" | "rate_limit_refusals") =>
    kind === "hub_started" ? t.hubStarted : kind === "hub_stopped" ? t.hubStopped :
      kind === "login_refusals" ? t.loginRefused : kind === "rate_limit_refusals" ? t.rateLimitRefused : t.transportRefused;
  return (
    <main className="remote-shell" lang={language === "pl" ? "pl" : "en"}>
      <header className="remote-head">
        <div><strong>dc24h.eu High Starter</strong><h1>{t.title}</h1><p>{t.mode}</p></div>
        <div className="remote-actions">
          <button type="button" aria-label="English" onClick={() => setLanguage("us")}>EN</button>
          <button type="button" aria-label="Polski" onClick={() => setLanguage("pl")}>PL</button>
          {state === "authorized" && <button type="button" onClick={() => void logout()}>{t.signOut}</button>}
        </div>
      </header>
      {/* Announce discrete status messages, not the entire metric dashboard:
          its counters and incident ages re-render every second. */}
      <section className="remote-panel">
        {state !== "authorized" && <p role="status">{state === "loading" ? t.loading : state === "unauthorized" ? t.unauthorized :
          state === "forbidden" ? t.forbidden : state === "expired" ? t.expired :
          state === "rate-limited" ? t.limited : t.unavailable}</p>}
        {state !== "authorized" && actionMessage && <p role="status">{actionMessage}</p>}
        {state !== "authorized" && state !== "loading" && <form onSubmit={(event) => { void login(event); }}>
          <label>{t.id}<input name="actor" inputMode="numeric" autoComplete="username"
            autoFocus={state === "expired" || state === "forbidden"}
            required pattern="[1-9][0-9]*" value={actor} onChange={(event) => setActor(event.target.value)} /></label>
          <label>{t.password}<input name="password" type="password" autoComplete="current-password"
            required value={password} onChange={(event) => setPassword(event.target.value)} /></label>
          <label>{t.otp}<input name="otp" type="text" inputMode="numeric" autoComplete="one-time-code"
            required pattern="[0-9]{6}" maxLength={6} value={otp} onChange={(event) => setOtp(event.target.value)} /></label>
          <p className="remote-hint">{t.loginHint}</p>
          <button type="submit">{t.signIn}</button>
          <button type="button" onClick={() => void passkeyLogin()} disabled={!actor || !password}>{t.passkeySignIn}</button>
        </form>}
        {state === "authorized" && <>
          <div className="remote-toolbar"><span>{t.mode}</span><button type="button" onClick={() => void load()}>{t.refresh}</button></div>
          <p className="remote-hint" role="status">{sessionExpiresAt === null ? t.sessionUnverified :
            `${t.sessionRemaining}: ${Math.max(0, Math.ceil((sessionExpiresAt - clock) / 60_000))} ${t.minutes}`}</p>
          {freshSummary && <section className="remote-incidents" aria-label={t.incidentTitle}>
            <h2>{t.incidentTitle}</h2>
            <dl className="remote-grid">
              <div><dt>{t.gatewayReady}</dt><dd>{t.incidentReady}</dd></div>
              <div><dt>{t.serviceReady}</dt><dd>{!freshSummary.observation ? t.sourceUnknown :
                freshSummary.server.running ? t.incidentReady : t.incidentDegraded}</dd></div>
              <div><dt>{t.lastUpdated}</dt><dd>{freshSummary.observation ?
                `${Math.max(0, Math.floor((freshSummary.observation.source_age_ms + clock - summaryFetchedAt!) / 1000))} s` :
                t.sourceUnknown}</dd></div>
              <div><dt>{t.chatDenied}</dt><dd>{freshSummary.chat.rejected}</dd></div>
              <div><dt>{t.maintenance}</dt><dd className="remote-unknown">{t.maintenanceUnknown}</dd></div>
            </dl>
            <button type="button" aria-pressed={incidentAcknowledged}
              onClick={() => setIncidentAcknowledged(true)} disabled={incidentAcknowledged}>{t.acknowledge}</button>
            {incidentAcknowledged && <p role="status">{t.acknowledged}</p>}
            {freshSummary.incidents?.state !== "observed" &&
              <p className="remote-unknown" role="status">{t.incidentUnknown}</p>}
            {freshSummary.incidents?.state === "observed" && <div className="remote-timeline">
              <h3>{t.timeline}</h3>
              {freshSummary.incidents.events.length ? <ul>{freshSummary.incidents.events.map((event, i) =>
                <li key={`${i}-${event.kind}`}>{eventName(event.kind)} · {event.age_seconds +
                  Math.floor((clock - summaryFetchedAt!) / 1000)} {t.secondsAgo}</li>)}</ul> :
                <p className="remote-hint">{t.noEvents}</p>}
              {freshSummary.incidents.alerts.length > 0 && <p role="status">{t.alertTitle}: {freshSummary.incidents.alerts.length}</p>}
            </div>}
            {freshSummary.incidents?.retained && <section className="remote-retained"
              aria-labelledby="retained-incident-title">
              <h3 id="retained-incident-title">{t.retainedTitle}</h3>
              <p className="remote-hint">{t.retainedLimit}</p>
              {freshSummary.incidents.retained.state === "unknown" ?
                <p role="status" className="remote-unknown">{t.retainedUnknown}</p> :
                freshSummary.incidents.retained.events.length ?
                  <ul>{freshSummary.incidents.retained.events.map((event, i) =>
                    <li key={`${i}-${event.kind}`}>{eventName(event.kind)} · {event.age_seconds +
                      Math.floor((clock - summaryFetchedAt!) / 1000)} {t.secondsAgo}</li>)}</ul> :
                  <p className="remote-hint">{t.retainedEmpty}</p>}
            </section>}
          </section>}
          {freshSummary ? <dl className="remote-grid">{rows.map(([key, value]) =>
            <div key={key}><dt>{key}</dt><dd>{value}</dd></div>)}</dl> : <p>{t.loading}</p>}
          {summary && !freshSummary && <p role="status">{t.stale}</p>}
          {freshSummary && !freshSummary.search && !freshSummary.transfers && <p className="remote-hint">{t.noData}</p>}
          <section className="remote-devices">
            <h2>{t.devices}</h2>
            <p className="remote-hint">{t.passkeyHelp}</p>
            <div className="remote-actions">
              <button type="button" onClick={() => void loadDevices()}>{t.loadDevices}</button>
              <button type="button" onClick={() => void enroll()}>{t.addPasskey}</button>
            </div>
            {devices && (devices.length ? <ul>{devices.map(device => <li key={device.id}>
              <strong>{device.name}</strong> ({device.device_type}, {device.backed_up ? "backup" : "local"})
              <span className="remote-hint">{t.created}: {displayTime(device.created_at)} · {t.lastUsed}: {displayTime(device.last_used_at)}</span>
              <label>{t.deviceName}<input value={deviceLabels[device.id] ?? ""} maxLength={64}
                onChange={event => setDeviceLabels(previous => ({ ...previous, [device.id]: event.target.value }))} /></label>
              <div className="remote-actions">
                <button type="button" onClick={() => void changeDevice(device.id, false)}
                  disabled={!!deviceMutationBusy || !!pendingRevoke || !(deviceLabels[device.id] ?? "").trim()}>{t.rename}</button>
                {pendingRevoke !== device.id && <button type="button" onClick={() => setPendingRevoke(device.id)}
                  disabled={!!deviceMutationBusy}>{t.remove}</button>}
              </div>
              {pendingRevoke === device.id && <div className="remote-confirm" role="group" aria-label={t.confirmRevoke}>
                <p role="alert">{t.revokeWarning}</p>
                <div className="remote-actions">
                  <button type="button" disabled={!!deviceMutationBusy}
                    onClick={() => { setPendingRevoke(null); void changeDevice(device.id, true); }}>{t.confirmRevoke}</button>
                  <button type="button" disabled={!!deviceMutationBusy} onClick={() => setPendingRevoke(null)}>{t.cancel}</button>
                </div>
              </div>}
            </li>)}</ul> : <p>{t.noDevices}</p>)}
            {actionMessage && <p role="status">{actionMessage}</p>}
          </section>
        </>}
      </section>
    </main>
  );
}
