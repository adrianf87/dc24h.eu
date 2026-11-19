import { useEffect, useState } from "react";
import type { Copy } from "../languages/us";
import { adminHeaders } from "../lib/adminAccess";
import { parseManagement } from "../lib/management";
import type { ManagementCatalog } from "../lib/management";

const links = {
  accounts: [{ href: "#accounts", label: "managementOpenAccounts" as const }],
  roles: [
    { href: "#accounts", label: "managementOpenAccounts" as const },
    { href: "#roles", label: "managementOpenRoles" as const },
  ],
  bans: [{ href: "#bans", label: "managementOpenBans" as const }],
};

export function Management({ t }: { t: Copy }) {
  const [catalog, setCatalog] = useState<ManagementCatalog | null>(null);
  const [available, setAvailable] = useState(false);

  useEffect(() => {
    let alive = true;
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 5000);
    void (async () => {
      try {
        const response = await fetch("./admin-api/v1/management", {
          signal: controller.signal,
          cache: "no-store",
          credentials: "same-origin",
          redirect: "error",
          headers: adminHeaders(),
        });
        if (!response.ok) throw new Error();
        const parsed = parseManagement(await response.json());
        if (alive) { setCatalog(parsed); setAvailable(true); }
      } catch {
        if (alive && !controller.signal.aborted) { setCatalog(null); setAvailable(false); }
      } finally {
        clearTimeout(timer);
      }
    })();
    return () => { alive = false; controller.abort(); clearTimeout(timer); };
  }, []);

  return <section className="card management-card" data-testid="management" aria-label={t.managementTitle}>
    <div className="card-heading"><div><h2>{t.managementAreas}</h2></div></div>
    <p className="management-note">{t.managementNote}</p>
    {!available && <p>{t.managementUnavailable}</p>}
    {available && catalog && <div className="management-areas">
      {catalog.areas.map((area) => <article key={area.key} data-testid={`management-${area.key}`}>
        <h3>{area.key === "accounts" ? t.managementAccounts : area.key === "roles" ? t.managementRoles : t.managementBans}</h3>
        <p>{area.key === "accounts" ? t.managementAccountsBody : area.key === "roles" ? t.managementRolesBody : t.managementBansBody}</p>
        <p><span>{t.managementPermission}</span> <code>{area.permission}</code></p>
        <p>{area.allowed ? t.managementAllowed : t.managementDenied}</p>
        <p className="management-links">
          {links[area.key].map((link) => <a key={link.href} href={link.href}>{t[link.label]}</a>)}
        </p>
      </article>)}
    </div>}
  </section>;
}
