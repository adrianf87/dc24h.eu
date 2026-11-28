import { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { Operations } from "./operations.tsx";

type SessionRole = "owner" | "administrator" | "registered" | "unknown";
type IdentitySummary = { schema: 1; source: "stage4-account-service";
  accounts: { total: number; active: number; owners: number; administrators: number; registered: number };
  verified_emails: number };

async function readIdentitySummary(signal: AbortSignal): Promise<IdentitySummary> {
  const response = await fetch("/stage4/v1/identity-summary", {
    method: "GET", credentials: "same-origin", cache: "no-store", redirect: "error", signal,
  });
  if (!response.ok || !response.headers.get("content-type")?.startsWith("application/json"))
    throw Error("API unavailable or access denied");
  const root: unknown = await response.json();
  if (!root || typeof root !== "object" || !("schema" in root) || root.schema !== 1 ||
      !("data" in root)) throw Error("Invalid API envelope");
  const data = root.data;
  if (!data || typeof data !== "object" || !("schema" in data) || data.schema !== 1 ||
      !("source" in data) || data.source !== "stage4-account-service" ||
      !("accounts" in data) || !data.accounts || typeof data.accounts !== "object" ||
      !("verified_emails" in data)) throw Error("Invalid C++ response");
  const accounts = data.accounts as Record<string, unknown>;
  const valid = (value: unknown): value is number =>
    typeof value === "number" && Number.isSafeInteger(value) && value >= 0 && value <= 1_000_000_000;
  if (![accounts.total, accounts.active, accounts.owners, accounts.administrators,
    accounts.registered, data.verified_emails].every(valid)) throw Error("Invalid C++ count");
  if ((accounts.active as number) > (accounts.total as number) ||
    (accounts.owners as number) + (accounts.administrators as number) +
      (accounts.registered as number) > (accounts.total as number) ||
    (data.verified_emails as number) > (accounts.total as number)) throw Error("Invalid C++ counts");
  return data as IdentitySummary;
}

function ReadOnlyDashboard() {
  const [role, setRole] = useState<SessionRole>(() => {
    const value = document.querySelector<HTMLElement>("#dashboard")?.dataset.role ?? "unknown";
    return value === "owner" || value === "administrator" || value === "registered" ? value : "unknown";
  });
  const [visible, setVisible] = useState(() => document.querySelector<HTMLElement>("#dashboard")?.hidden === false);
  const [sequence, setSequence] = useState(0);
  const [summary, setSummary] = useState<IdentitySummary | null>(null);
  const [error, setError] = useState("");
  const [csrf, setCsrf] = useState("");

  useEffect(() => {
    const authenticated = (event: Event) => {
      const detail: unknown = (event as CustomEvent).detail;
      const value = detail && typeof detail === "object" && "role" in detail ? detail.role : "unknown";
      setRole(value === "owner" || value === "administrator" || value === "registered" ? value : "unknown");
      const token = detail && typeof detail === "object" && "csrf" in detail ? detail.csrf : undefined;
      setCsrf(typeof token === "string" && /^[0-9a-f]{64}$/.test(token) ? token : "");
      setVisible(true); setSequence(value => value + 1);
    };
    const loggedOut = () => { setVisible(false); setRole("unknown"); setSummary(null); setError(""); setCsrf(""); };
    document.addEventListener("stage4:authenticated", authenticated);
    document.addEventListener("stage4:logout", loggedOut);
    document.dispatchEvent(new Event("stage4:session-request"));
    return () => {
      document.removeEventListener("stage4:authenticated", authenticated);
      document.removeEventListener("stage4:logout", loggedOut);
    };
  }, []);

  useEffect(() => {
    if (!visible || (role !== "owner" && role !== "administrator")) return;
    const controller = new AbortController();
    setError(""); setSummary(null);
    readIdentitySummary(controller.signal).then(value => {
      if (!controller.signal.aborted) setSummary(value);
    }).catch(() => {
      if (!controller.signal.aborted) { setSummary(null); setError("Odmowa dostępu lub usługa niedostępna."); }
    });
    return () => controller.abort();
  }, [visible, role, sequence]);

  if (!visible || role === "unknown") return null;
  if (role === "registered") return <p className="api-note">Zestawienie kont wymaga roli Owner lub Administrator.</p>;
  return <section className="api-panel" aria-label="Odczyt API WebAdmin">
    <div className="api-heading">
      <h3>WebAdmin — konta (C++ / MariaDB)</h3>
      <button type="button" onClick={() => setSequence(value => value + 1)}>Odśwież API</button>
    </div>
    {error && <p role="alert">{error}</p>}
    {summary && <div className="api-stats">
      <p><strong>Łącznie kont:</strong> {summary.accounts.total}</p>
      <p><strong>Aktywne konta:</strong> {summary.accounts.active}</p>
      <p><strong>Owner:</strong> {summary.accounts.owners}</p>
      <p><strong>Administratorzy:</strong> {summary.accounts.administrators}</p>
      <p><strong>Użytkownicy zarejestrowani:</strong> {summary.accounts.registered}</p>
      <p><strong>Zweryfikowane adresy e-mail:</strong> {summary.verified_emails}</p>
    </div>}
    {csrf && <Operations csrf={csrf} role={role}/>}
  </section>;
}

const element = document.querySelector("#stage4-react");
if (element) createRoot(element).render(<ReadOnlyDashboard />);
