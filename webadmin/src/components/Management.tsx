import { useEffect, useState } from "react";
import type { Copy } from "../languages/us";
import { adminHeaders } from "../lib/adminAccess";
import { parseManagement } from "../lib/management";
import type { ManagementArea, ManagementCatalog } from "../lib/management";

const links: Partial<Record<ManagementArea["key"], { href: string; label: "managementOpenAccounts" | "managementOpenRoles" | "managementOpenBans" | "managementOpenConfiguration" }[]>> = {
  accounts: [{ href: "#accounts", label: "managementOpenAccounts" }],
  roles: [
    { href: "#accounts", label: "managementOpenAccounts" },
    { href: "#roles", label: "managementOpenRoles" },
  ],
  bans: [{ href: "#bans", label: "managementOpenBans" }],
  configuration: [{ href: "#configuration", label: "managementOpenConfiguration" }],
};

function title(area: ManagementArea, t: Copy) {
  if (area.key === "accounts") return t.managementAccounts;
  if (area.key === "roles") return t.managementRoles;
  if (area.key === "bans") return t.managementBans;
  if (area.key === "configuration") return t.managementConfiguration;
  return t.managementHub;
}

function body(area: ManagementArea, t: Copy) {
  if (area.key === "accounts") return t.managementAccountsBody;
  if (area.key === "roles") return t.managementRolesBody;
  if (area.key === "bans") return t.managementBansBody;
  if (area.key === "configuration") return t.managementConfigurationBody;
  return t.managementHubBody;
}

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
      {catalog.areas.map((area) => {
        // The broad management/authorize contract remains 501. Configuration
        // has a narrower, separately authorized editor in beta-0.1.4.
        const areaLinks = area.implemented || (area.key === "configuration" && area.allowed) ? links[area.key] : undefined;
        return <article key={area.key} data-testid={`management-${area.key}`}>
          <h3>{title(area, t)}</h3>
          <p>{body(area, t)}</p>
          <p><span>{t.managementPermission}</span> <code>{area.permission}</code></p>
          <p>{area.allowed ? t.managementAllowed : t.managementDenied}</p>
          {areaLinks ? <p className="management-links">
            {areaLinks.map((link) => <a key={link.href + link.label} href={link.href}>{t[link.label]}</a>)}
          </p> : <p>{t.managementNotImplemented}</p>}
        </article>;
      })}
    </div>}
  </section>;
}
