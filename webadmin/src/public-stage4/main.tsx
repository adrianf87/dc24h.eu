import { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { Operations } from "./operations.tsx";
import { AccountOverview } from "./overview.tsx";
import type { IdentitySummary } from "./overview.tsx";
import { ACCOUNT_ROLES } from "../lib/roles.ts";

type SessionRole = "owner" | "administrator" | "member" | "unknown";
function sessionRole(value: unknown): SessionRole {
  if (value === "owner" || value === "administrator") return value;
  if (typeof value === "string" && (ACCOUNT_ROLES as readonly string[]).includes(value)) return "member";
  return "unknown";
}

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
  const [role, setRole] = useState<SessionRole>(() => sessionRole(document.querySelector<HTMLElement>("#dashboard")?.dataset.role));
  const [visible, setVisible] = useState(() => document.querySelector<HTMLElement>("#dashboard")?.hidden === false);
  const [sequence, setSequence] = useState(0);
  const [snapshot, setSnapshot] = useState<{ data: IdentitySummary; receivedAt: Date } | null>(null);
  const [error, setError] = useState("");
  const [csrf, setCsrf] = useState("");

  useEffect(() => {
    const authenticated = (event: Event) => {
      const detail: unknown = (event as CustomEvent).detail;
      const value = detail && typeof detail === "object" && "role" in detail ? detail.role : "unknown";
      setRole(sessionRole(value));
      const token = detail && typeof detail === "object" && "csrf" in detail ? detail.csrf : undefined;
      setCsrf(typeof token === "string" && /^[0-9a-f]{64}$/.test(token) ? token : "");
      setVisible(true); setSequence(value => value + 1);
    };
    const loggedOut = () => { setVisible(false); setRole("unknown"); setSnapshot(null); setError(""); setCsrf(""); };
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
    setError(""); setSnapshot(null);
    readIdentitySummary(controller.signal).then(value => {
      if (!controller.signal.aborted) setSnapshot({ data: value, receivedAt: new Date() });
    }).catch(() => {
      if (!controller.signal.aborted) setError("Odmowa dostępu lub usługa niedostępna.");
    });
    return () => controller.abort();
  }, [visible, role, sequence]);

  if (!visible || role === "unknown") return null;
  if (role === "member") return <div className="member-view"><h1>Moje konto</h1>
    <p>Zmień hasło lub zarządzaj swoimi sesjami w ustawieniach konta.</p>
    <button className="button-primary" type="button" aria-haspopup="dialog"
      onClick={() => document.querySelector<HTMLButtonElement>("#show-settings")?.click()}>Ustawienia konta</button></div>;
  const overview = <AccountOverview snapshot={snapshot} error={error} onRefresh={() => setSequence(value => value + 1)}/>;
  return csrf ? <Operations csrf={csrf} role={role} overview={overview}/> : null;
}

const element = document.querySelector("#stage4-react");
if (element) createRoot(element).render(<ReadOnlyDashboard />);
