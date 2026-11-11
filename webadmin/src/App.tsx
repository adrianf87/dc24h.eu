import { useEffect, useState } from "react";
import {
  Activity,
  ArrowDownToLine,
  ArrowRight,
  ArrowUpRight,
  Cable,
  ChevronDown,
  ChevronRight,
  Clock3,
  Database,
  FileText,
  Globe2,
  LayoutDashboard,
  LockKeyhole,
  Menu,
  Network,
  Radio,
  RefreshCw,
  ScrollText,
  Server,
  Settings2,
  ShieldCheck,
  X,
  Users,
} from "lucide-react";
import type { LucideIcon } from "lucide-react";
import { us } from "./languages/us";
import type { Copy } from "./languages/us";
import { pl } from "./languages/pl";
import { duration, WEBADMIN_VERSION } from "./lib/status";
import type { HubSnapshot } from "./lib/status";
import { useHub } from "./lib/useHub";
import type { Sample } from "./lib/useHub";

type Page = "overview" | "connections" | "security" | "database";
const pages: Page[] = ["overview", "connections", "security", "database"];
const currentPage = (): Page =>
  pages.includes(location.hash.slice(1) as Page)
    ? (location.hash.slice(1) as Page)
    : "overview";
function Badge({
  children,
  tone = "neutral",
}: {
  children: React.ReactNode;
  tone?: string;
}) {
  return (
    <span className={`badge ${tone}`}>
      <i />
      {children}
    </span>
  );
}
function Metric({
  icon: Icon,
  label,
  value,
  hint,
  color = "blue",
}: {
  icon: LucideIcon;
  label: string;
  value: string;
  hint: string;
  color?: string;
}) {
  return (
    <article className="metric card">
      <div className="metric-top">
        <span>{label}</span>
        <span className={`metric-icon ${color}`}>
          <Icon size={18} />
        </span>
      </div>
      <strong className="metric-value">{value}</strong>
      <p>{hint}</p>
    </article>
  );
}
function Chart({ samples, t }: { samples: Sample[]; t: Copy }) {
  const max = Math.max(1, ...samples.map((value) => value.active));
  const points = samples.map(
    (sample, index) =>
      `${30 + (index * 740) / Math.max(1, samples.length - 1)},${150 - (sample.active / max) * 120}`,
  );
  const last = points.at(-1)?.split(",");
  return (
    <section className="card activity-card">
      <div className="card-heading">
        <div>
          <h2>{t.activityTitle}</h2>
          <p>{t.activitySub}</p>
        </div>
        <span className="mini-label">
          <span className="tiny-dot" />
          {t.liveSession}
        </span>
      </div>
      <div className={`plot ${samples.length ? "has-data" : ""}`}>
        <svg
          viewBox="0 0 800 180"
          aria-hidden="true"
          preserveAspectRatio="none"
        >
          <path
            d="M0 30H800M0 70H800M0 110H800M0 150H800"
            className="chart-grid"
          />
          {samples.length > 0 && (
            <>
              <polygon
                points={`30,150 ${points.join(" ")} ${last?.[0]},150`}
                fill="#edf2ff"
              />
              <polyline
                points={points.join(" ")}
                fill="none"
                stroke="#5574eb"
                strokeWidth="2.5"
                vectorEffect="non-scaling-stroke"
              />
              {last && (
                <circle
                  cx={last[0]}
                  cy={last[1]}
                  r="4"
                  fill="#5574eb"
                  stroke="white"
                  strokeWidth="2"
                />
              )}
            </>
          )}
        </svg>
        {!samples.length && (
          <div className="empty-plot">
            <span className="empty-icon">
              <Radio size={25} />
            </span>
            <strong>{t.noObservations}</strong>
            <p>{t.noObservationsSub}</p>
          </div>
        )}
      </div>
      <div className="plot-footer">
        <span>
          {samples.length ? `${samples.length} ${t.observations}` : "—"}
        </span>
        <span>
          <span className="legend-dot" />
          {t.activeConnections}
        </span>
        <span>{samples.length === 1 ? t.firstSample : t.current}</span>
      </div>
    </section>
  );
}
function Details({ snapshot, t }: { snapshot: HubSnapshot | null; t: Copy }) {
  return (
    <section className="card details-card">
      <div className="card-heading">
        <div>
          <h2>{t.hubDetails}</h2>
          <p>{t.hubDetailsSub}</p>
        </div>
        <Server size={18} />
      </div>
      <dl>
        <div>
          <dt>{t.project}</dt>
          <dd>
            <span className="project-mark" />
            dc24h.eu
          </dd>
        </div>
        <div>
          <dt>{t.serverVersion}</dt>
          <dd>{snapshot ? <code>{snapshot.server.version}</code> : "—"}</dd>
        </div>
        <div>
          <dt>{t.coreState}</dt>
          <dd>
            {snapshot ? (
              <Badge tone={snapshot.server.core_running ? "green" : "neutral"}>
                {snapshot.server.core_running ? t.running : t.notRunning}
              </Badge>
            ) : (
              <span className="muted">{t.unavailable}</span>
            )}
          </dd>
        </div>
        <div>
          <dt>{t.uptime}</dt>
          <dd>
            <Clock3 size={14} />
            {snapshot ? duration(snapshot.server.uptime_seconds) : "—"}
          </dd>
        </div>
        <div>
          <dt>{t.apiVersion}</dt>
          <dd>{snapshot ? snapshot.api_version : "—"}</dd>
        </div>
      </dl>
    </section>
  );
}
function Components({
  snapshot,
  t,
  number,
}: {
  snapshot: HubSnapshot | null;
  t: Copy;
  number: (value: number | undefined) => string;
}) {
  const rows = [
    {
      icon: Server,
      title: t.core,
      purpose: t.corePurpose,
      value: snapshot
        ? snapshot.server.core_running
          ? t.running
          : t.notRunning
        : t.unavailable,
      green: snapshot?.server.core_running,
    },
    {
      icon: Network,
      title: t.network,
      purpose: t.networkPurpose,
      value: snapshot
        ? `${number(snapshot.network.accepted_connections)} ${t.accepted}`
        : t.unavailable,
    },
    {
      icon: LockKeyhole,
      title: t.adcs,
      purpose: t.adcsPurpose,
      value: snapshot
        ? `${number(snapshot.network.adcs_handshakes)} ${t.completed}`
        : t.unavailable,
    },
    {
      icon: Database,
      title: "MariaDB",
      purpose: t.dbPurpose,
      value: snapshot
        ? snapshot.database.enabled
          ? t.enabled
          : t.disabled
        : t.unavailable,
    },
  ];
  return (
    <section className="card components-card">
      <div className="card-heading">
        <div>
          <h2>{t.components}</h2>
          <p>{t.componentsSub}</p>
        </div>
        <span className="outline-label">{t.componentCount}</span>
      </div>
      <div className="table-scroll">
        <table>
          <thead>
            <tr>
              <th>{t.component}</th>
              <th>{t.purpose}</th>
              <th>{t.reportedValue}</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((row) => (
              <tr key={row.title}>
                <td>
                  <span className="service-icon">
                    <row.icon size={17} />
                  </span>
                  <strong>{row.title}</strong>
                </td>
                <td>{row.purpose}</td>
                <td>
                  <Badge tone={row.green ? "green" : "neutral"}>
                    {row.value}
                  </Badge>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}
export default function App() {
  const [language, setLanguage] = useState<"us" | "pl">(() => {
    try {
      return localStorage.getItem("dc24h-language") === "pl" ? "pl" : "us";
    } catch {
      return "us";
    }
  });
  const [page, setPage] = useState<Page>(currentPage);
  const [menu, setMenu] = useState(false);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const t: Copy = language === "pl" ? pl : us;
  const hub = useHub(autoRefresh),
    snapshot = hub.snapshot;
  const formatter = new Intl.NumberFormat(
    language === "pl" ? "pl-PL" : "en-US",
  );
  const number = (value: number | undefined) =>
    value === undefined ? "—" : formatter.format(value);
  const documentation = `https://github.com/adrianf87/dc24h.eu/blob/${WEBADMIN_VERSION}/doc/${language}/WEBADMIN.md`;
  useEffect(() => {
    document.documentElement.lang = language === "pl" ? "pl" : "en-US";
    try {
      localStorage.setItem("dc24h-language", language);
    } catch {
      /* Preferences are optional. */
    }
  }, [language]);
  useEffect(() => {
    const update = () => {
      setPage(currentPage());
      setMenu(false);
    };
    addEventListener("hashchange", update);
    return () => removeEventListener("hashchange", update);
  }, []);
  useEffect(() => {
    if (!menu) return;
    const escape = (event: KeyboardEvent) => {
      if (event.key === "Escape") setMenu(false);
    };
    addEventListener("keydown", escape);
    return () => removeEventListener("keydown", escape);
  }, [menu]);
  const navigation = [
    { page: "overview", icon: LayoutDashboard },
    { page: "connections", icon: Network },
    { page: "security", icon: ShieldCheck },
    { page: "database", icon: Database },
  ] as const;
  const titles = {
    overview: [t.overviewTitle, t.overviewSub],
    connections: [t.connectionsTitle, t.connectionsSub],
    security: [t.securityTitle, t.securitySub],
    database: [t.databaseTitle, t.databaseSub],
  };
  const exportSnapshot = () => {
    if (!snapshot) return;
    const blob = new Blob(
      [
        JSON.stringify(
          { observed_at: new Date(hub.checked!).toISOString(), ...snapshot },
          null,
          2,
        ),
      ],
      { type: "application/json" },
    );
    const url = URL.createObjectURL(blob),
      link = document.createElement("a");
    link.href = url;
    link.download = "dc24h-snapshot.json";
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  };
  return (
    <div className="app-shell">
      <a
        className="skip-link"
        href="#main-content"
        onClick={(event) => {
          event.preventDefault();
          document.getElementById("main-content")?.focus();
        }}
      >
        {t.skip}
      </a>
      {menu && (
        <button
          className="sidebar-overlay"
          aria-label={t.closeMenu}
          onClick={() => setMenu(false)}
        />
      )}
      <aside
        className={`sidebar ${menu ? "is-open" : ""}`}
        aria-label="WebAdmin"
      >
        <a href="#overview" className="brand">
          <span className="brand-logo">
            <Cable size={25} />
          </span>
          <span>
            <strong>
              dc24h<span>.eu</span>
            </strong>
            <small>{t.brandCaption}</small>
          </span>
        </a>
        <div className="workspace">
          <span className="workspace-icon">
            <Server size={17} />
          </span>
          <span>
            <small>{t.workspace}</small>
            <strong>dc24h.eu</strong>
          </span>
          <span className="workspace-lock">
            <LockKeyhole size={14} />
          </span>
        </div>
        <nav aria-label={t.monitoring}>
          <p className="nav-label">{t.monitoring}</p>
          {navigation.map((item) => (
            <a
              key={item.page}
              href={`#${item.page}`}
              onClick={() => setMenu(false)}
              className={`nav-item ${page === item.page ? "active" : ""}`}
              aria-current={page === item.page ? "page" : undefined}
            >
              <item.icon size={18} />
              {t[item.page]}
              {page === item.page && <span className="nav-active-dot" />}
            </a>
          ))}
          <p className="nav-label admin-label">{t.administration}</p>
          {[
            { icon: Settings2, label: t.configuration },
            { icon: ScrollText, label: t.logs },
          ].map((item) => (
            <button
              key={item.label}
              className="nav-item future"
              disabled
              title={t.noEdits}
            >
              <item.icon size={18} />
              {item.label}
              <small>{t.soon}</small>
            </button>
          ))}
        </nav>
        <div className="sidebar-bottom">
          <a
            href={documentation}
            target="_blank"
            rel="noreferrer"
            className="documentation"
          >
            <FileText size={17} />
            {t.documentation}
            <ArrowUpRight size={15} />
          </a>
          <div className="release-note">
            <span className="release-dot" />
            <div>
              <strong>{WEBADMIN_VERSION}</strong>
              <small>{t.release}</small>
            </div>
            <span className="release-letter">α</span>
          </div>
        </div>
      </aside>
      <div className="main-shell">
        <header className="topbar">
          <button
            className="mobile-menu icon-button"
            onClick={() => setMenu(!menu)}
            aria-label={menu ? t.closeMenu : t.openMenu}
            aria-expanded={menu}
          >
            {menu ? <X size={20} /> : <Menu size={20} />}
          </button>
          <div className="breadcrumb">
            <span>WebAdmin</span>
            <ChevronRight size={14} />
            <strong>{t[page]}</strong>
          </div>
          <div className="topbar-right">
            <span className="environment">
              <span className="tiny-dot" />
              {t.localEnvironment}
            </span>
            <span className="topbar-divider" />
            <label className="language-selector">
              <Globe2 size={15} />
              <select
                aria-label={t.language}
                value={language}
                onChange={(event) =>
                  setLanguage(event.target.value as "us" | "pl")
                }
              >
                <option value="us">EN / US</option>
                <option value="pl">PL / Polski</option>
              </select>
              <ChevronDown size={12} />
            </label>
          </div>
        </header>
        <main id="main-content" tabIndex={-1}>
          <div className="page-heading">
            <div>
              <div className="eyebrow">
                DC24H.EU <span>/</span> {t.monitoring}
              </div>
              <h1>{titles[page][0]}</h1>
              <p>{titles[page][1]}</p>
            </div>
            <div className="heading-actions">
              <button
                className="button secondary export-button"
                disabled={!snapshot}
                title={!snapshot ? t.exportHint : undefined}
                onClick={exportSnapshot}
              >
                <ArrowDownToLine size={15} />
                {t.export}
              </button>
              <button
                className="button primary"
                onClick={() => void hub.refresh()}
                disabled={hub.refreshing}
              >
                <RefreshCw
                  size={15}
                  className={hub.refreshing ? "spinning" : ""}
                />
                {hub.refreshing ? t.refreshing : t.refresh}
              </button>
            </div>
          </div>
          <div className="status-toolbar">
            <div role="status" className="connection-status">
              <Badge
                tone={
                  hub.connection === "live"
                    ? "green"
                    : hub.connection === "offline"
                      ? "amber"
                      : "neutral"
                }
              >
                {t[hub.connection === "live" ? "connected" : hub.connection]}
              </Badge>
              <span className="last-checked">
                {hub.checked
                  ? `${t.lastCheck} ${new Intl.DateTimeFormat(language === "pl" ? "pl-PL" : "en-US", { hour: "2-digit", minute: "2-digit", second: "2-digit" }).format(hub.checked)}`
                  : t.never}
              </span>
            </div>
            <div className="auto-refresh">
              <span>{t.autoRefresh}</span>
              <span className="refresh-interval">{t.everyFive}</span>
              <button
                className={`switch ${autoRefresh ? "on" : ""}`}
                role="switch"
                aria-checked={autoRefresh}
                aria-label={t.toggleRefresh}
                onClick={() => setAutoRefresh(!autoRefresh)}
              >
                <span />
              </button>
            </div>
          </div>
          {hub.connection !== "live" && (
            <section
              className="connection-banner"
              aria-label={
                t[
                  hub.connection === "offline"
                    ? "offlineTitle"
                    : hub.connection === "connecting"
                      ? "connectingTitle"
                      : "waitingTitle"
                ]
              }
            >
              <div className="banner-icon">
                <Radio size={23} />
              </div>
              <div>
                <h2>
                  {
                    t[
                      hub.connection === "offline"
                        ? "offlineTitle"
                        : hub.connection === "connecting"
                          ? "connectingTitle"
                          : "waitingTitle"
                    ]
                  }
                </h2>
                <p>
                  {
                    t[
                      hub.connection === "offline"
                        ? "offlineSub"
                        : hub.connection === "connecting"
                          ? "connectingSub"
                          : "waitingSub"
                    ]
                  }
                </p>
              </div>
              <a href={documentation} target="_blank" rel="noreferrer">
                {t.waitingLink}
                <ArrowRight size={15} />
              </a>
            </section>
          )}
          {page !== "database" && (
            <div className={`metrics-grid ${page === "security" ? "two" : ""}`}>
              <Metric
                icon={page === "security" ? ShieldCheck : Network}
                label={
                  page === "security"
                    ? t.rejectedConnections
                    : t.activeConnections
                }
                value={number(
                  page === "security"
                    ? snapshot?.network.security_rejected
                    : snapshot?.network.active_connections,
                )}
                hint={page === "security" ? t.rejectsHint : t.activeHint}
              />
              {page !== "security" && (
                <>
                  <Metric
                    icon={page === "connections" ? Cable : Activity}
                    label={
                      page === "connections"
                        ? t.acceptedConnections
                        : t.adcFrames
                    }
                    value={number(
                      page === "connections"
                        ? snapshot?.network.accepted_connections
                        : snapshot?.network.adc_frames,
                    )}
                    hint={page === "connections" ? t.acceptsHint : t.adcHint}
                    color="violet"
                  />
                  <Metric
                    icon={LockKeyhole}
                    label={t.tlsHandshakes}
                    value={number(snapshot?.network.adcs_handshakes)}
                    hint={t.tlsHint}
                    color="teal"
                  />
                </>
              )}
              <Metric
                icon={ShieldCheck}
                label={t.securityClosed}
                value={number(snapshot?.network.security_closed)}
                hint={t.securityHint}
                color="amber"
              />
            </div>
          )}
          {page === "overview" && (
            <div className="metrics-grid" data-testid="user-statistics">
              <Metric icon={Users} label={t.onlineUsers}
                value={number(snapshot?.users.online)} hint={t.onlineHint} />
              <Metric icon={Users} label={t.peakOnline}
                value={number(snapshot?.users.peak_online)} hint={t.peakOnlineHint} color="violet" />
              <Metric icon={Activity} label={t.successfulLogins}
                value={number(snapshot?.users.successful_logins)} hint={t.loginsHint} color="teal" />
              <Metric icon={Cable} label={t.closedConnections}
                value={number(snapshot?.network.closed_connections)} hint={t.closedHint} color="amber" />
            </div>
          )}
          {page === "overview" && (
            <div className="metrics-grid" data-testid="chat-statistics">
              <Metric icon={Activity} label={t.publicChat}
                value={number(snapshot?.chat.public_dispatches)} hint={t.chatDispatchHint} />
              <Metric icon={LockKeyhole} label={t.privateChat}
                value={number(snapshot?.chat.private_dispatches)} hint={t.chatDispatchHint} color="violet" />
              <Metric icon={ShieldCheck} label={t.chatRejected}
                value={number(snapshot?.chat.rejected)} hint={t.chatRejectedHint} color="amber" />
              <Metric icon={ShieldCheck} label={t.chatFlood}
                value={number(snapshot?.chat.flood_limited)} hint={t.chatFloodHint} color="teal" />
            </div>
          )}
          {page === "connections" && (
            <div className="metrics-grid" data-testid="connection-statistics">
              <Metric icon={Network} label={t.peakConnections}
                value={number(snapshot?.network.peak_connections)} hint={t.peakConnectionsHint} />
              <Metric icon={ShieldCheck} label={t.allRejected}
                value={number(snapshot?.network.rejected_connections)} hint={t.allRejectedHint} color="amber" />
              <Metric icon={ArrowDownToLine} label={t.receivedBytes}
                value={number(snapshot?.network.received_bytes)} hint={t.bytesHint} color="teal" />
              <Metric icon={ArrowUpRight} label={t.sentBytes}
                value={number(snapshot?.network.sent_bytes)} hint={t.bytesHint} color="violet" />
            </div>
          )}
          {(page === "overview" || page === "connections") && (
            <div className="overview-grid">
              <Chart samples={hub.samples} t={t} />
              <Details snapshot={snapshot} t={t} />
            </div>
          )}
          {page === "security" && (
            <div className="section-note">
              <ShieldCheck size={23} />
              <p>{t.securityNote}</p>
            </div>
          )}
          {page === "database" && (
            <section className="card database-card">
              <div className="database-emblem">
                <Database size={31} />
              </div>
              <span className="eyebrow">MARIADB</span>
              <h2>{t.dbTitle}</h2>
              <Badge tone={snapshot?.database.enabled ? "green" : "neutral"}>
                {snapshot
                  ? snapshot.database.enabled
                    ? t.enabled
                    : t.disabled
                  : t.unavailable}
              </Badge>
              <p>{snapshot ? t.dbNote : t.dbUnavailable}</p>
              <span className="database-footnote">
                <Clock3 size={14} />
                {t.dbLabel}
              </span>
            </section>
          )}
          {page === "overview" && (
            <Components snapshot={snapshot} t={t} number={number} />
          )}
          <div className="bottom-note">
            <span className="note-icon">
              <LockKeyhole size={17} />
            </span>
            <div>
              <strong>{t.localNote}</strong>
              <p>{t.localNoteSub}</p>
            </div>
            <span className="outline-label">
              <LockKeyhole size={11} />
              {t.readOnly}
            </span>
          </div>
          <footer>
            <span>
              {t.footer}
              <span className="footer-dot">·</span>
              {t.buildLabel} {WEBADMIN_VERSION}
            </span>
            <a href={documentation} target="_blank" rel="noreferrer">
              {t.documentation}
              <ArrowUpRight size={13} />
            </a>
          </footer>
        </main>
      </div>
    </div>
  );
}
