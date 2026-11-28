import { BadgeCheck, ChartNoAxesColumnIncreasing, Clock3, RefreshCw, ShieldCheck, UserCheck, UsersRound } from "lucide-react";
import type { CSSProperties } from "react";

export type IdentitySummary = {
  schema: 1;
  source: "stage4-account-service";
  accounts: { total: number; active: number; owners: number; administrators: number; registered: number };
  verified_emails: number;
};

const number = new Intl.NumberFormat("pl-PL");
const percent = new Intl.NumberFormat("pl-PL", { style: "percent", maximumFractionDigits: 1 });
const time = new Intl.DateTimeFormat("pl-PL", { hour: "2-digit", minute: "2-digit" });

function AccountMetrics({ data }: { data: IdentitySummary }) {
  const { total, active, owners, administrators, registered } = data.accounts;
  const verified = data.verified_emails;
  const other = total - owners - administrators - registered;
  const metrics = [
    { label: "Łącznie kont", value: total, icon: UsersRound, tone: "violet" },
    { label: "Aktywne konta", value: active, icon: UserCheck, tone: "mint" },
    { label: "Zweryfikowane adresy e-mail", value: verified, icon: BadgeCheck, tone: "sky" },
  ];
  const roles = [
    { label: "Użytkownicy zarejestrowani", count: registered, tone: "violet" },
    { label: "Administratorzy", count: administrators, tone: "sky" },
    { label: "Właściciele", count: owners, tone: "mint" },
    { label: "Pozostałe role", count: other, tone: "neutral" },
  ];
  const rate = total ? verified / total : 0;
  const ringStyle = { "--verification-fill": `${rate * 100}%` } as CSSProperties;

  return <>
    <div className="api-stats">
      {metrics.map(({ label, value, icon: Icon, tone }) => <article className="overview-stat surface" key={label}>
        <span className="stat-label">{label}</span>
        <span className={`stat-icon tone-${tone}`}><Icon size={19} aria-hidden="true"/></span>
        <strong className="stat-value">{number.format(value)}</strong>
      </article>)}
    </div>
    <div className="overview-grid">
      <section className="surface overview-detail" aria-labelledby="role-chart-heading">
        <div className="section-title">
          <span className="stat-icon tone-violet"><ChartNoAxesColumnIncreasing size={17} aria-hidden="true"/></span>
          <h3 id="role-chart-heading">Struktura ról</h3>
        </div>
        <ul className="role-chart">
          {roles.map(({ label, count, tone }) => <li key={label}>
            <span className="role-chart-label">
              <span className={`chart-dot tone-${tone}`} aria-hidden="true"/>{label}
            </span>
            <strong>{number.format(count)}</strong>
            <progress className={`tone-${tone}`} value={count} max={Math.max(1, total)} aria-label={label}/>
          </li>)}
        </ul>
      </section>
      <section className="surface overview-detail" aria-labelledby="email-chart-heading">
        <div className="section-title">
          <span className="stat-icon tone-mint"><ShieldCheck size={17} aria-hidden="true"/></span>
          <h3 id="email-chart-heading">Weryfikacja e-mail</h3>
        </div>
        <div className="verification-chart">
          <div className="verification-ring" style={ringStyle} role="img"
            aria-label={total ? `${percent.format(rate)} kont ze zweryfikowanym adresem e-mail` : "Brak kont do weryfikacji"}>
            <div className="verification-ring-center">
              <strong>{total ? percent.format(rate) : "—"}</strong>
              <span>zweryfikowanych</span>
            </div>
          </div>
          <dl className="verification-legend">
            <div><dt><span className="chart-dot tone-mint" aria-hidden="true"/> Zweryfikowane</dt><dd>{number.format(verified)}</dd></div>
            <div><dt><span className="chart-dot tone-neutral" aria-hidden="true"/> Bez weryfikacji</dt><dd>{number.format(total - verified)}</dd></div>
          </dl>
        </div>
      </section>
    </div>
  </>;
}

export function AccountOverview({ snapshot, error, onRefresh }: {
  snapshot: { data: IdentitySummary; receivedAt: Date } | null;
  error: string;
  onRefresh: () => void;
}) {
  const loading = !snapshot && !error;
  return <section className="overview-card" aria-labelledby="identity-overview" aria-busy={loading}>
    <div className="overview-heading">
      <h2 id="identity-overview">Konta użytkowników</h2>
      <div className="overview-tools">
        {snapshot && <span className="snapshot-time">
          <Clock3 size={14} aria-hidden="true"/> Odczyt o <time dateTime={snapshot.receivedAt.toISOString()}>{time.format(snapshot.receivedAt)}</time>
        </span>}
        <button type="button" disabled={loading} onClick={onRefresh}><RefreshCw size={16} aria-hidden="true"/> Odśwież</button>
      </div>
    </div>
    {error && <p className="surface api-error" role="alert">{error}</p>}
    {loading && <p className="surface api-loading" role="status">Wczytywanie danych kont…</p>}
    {snapshot && <AccountMetrics data={snapshot.data}/>}
  </section>;
}
