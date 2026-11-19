import {
  Activity,
  ArrowRight,
  Cable,
  Clock3,
  Database,
  LockKeyhole,
  MessageSquare,
  Network,
  Search,
  Server,
  ShieldCheck,
  Users,
} from "lucide-react";
import { Metric } from "./Metric";
import { duration } from "../lib/status";
import type { HubSnapshot } from "../lib/status";
import type { Copy } from "../languages/us";

const safeInteger = BigInt(Number.MAX_SAFE_INTEGER);

function formatCounter(value: string, locale: string): string {
  return new Intl.NumberFormat(locale).format(BigInt(value));
}

function formatBytes(value: string, locale: string): string {
  let amount = BigInt(value);
  const units = ["B", "KiB", "MiB", "GiB", "TiB"];
  let unit = 0;
  let remainder = 0n;
  while (amount >= 1024n && unit < units.length - 1) {
    remainder = amount % 1024n;
    amount /= 1024n;
    unit += 1;
  }
  if (unit === 0) return `${new Intl.NumberFormat(locale).format(amount)} B`;
  const shown = Number(amount) + Number((remainder * 10n) / 1024n) / 10;
  return `${new Intl.NumberFormat(locale, { minimumFractionDigits: 1, maximumFractionDigits: 1 }).format(shown)} ${units[unit]}`;
}

function formatLatency(samples: string, milliseconds: string, locale: string, empty: string): string {
  if (samples === "0") return empty;
  return `${formatCounter(milliseconds, locale)} ms`;
}

export function Dashboard({
  snapshot,
  t,
  locale,
}: {
  snapshot: HubSnapshot | null;
  t: Copy;
  locale: string;
}) {
  const board = snapshot?.dashboard;
  if (!board) {
    return (
      <section className="card dashboard-card" data-testid="hub-dashboard">
        <p>{t.dashboardUnavailable}</p>
      </section>
    );
  }
  const number = (value: string) => formatCounter(value, locale);
  const sample = snapshot?.monitoring;
  const uptime =
    BigInt(board.uptime_seconds) <= safeInteger
      ? duration(Number(board.uptime_seconds))
      : number(board.uptime_seconds);
  return (
    <div data-testid="hub-dashboard">
      <div className="section-note dashboard-note">
        <Server size={23} />
        <p>{t.dashboardNote}</p>
      </div>
      <section aria-label={t.dashboardHub}>
        <div className="section-heading">
          <h2>{t.dashboardHub}</h2>
        </div>
        <div className="metrics-grid" data-testid="dashboard-hub">
          <Metric
            icon={Server}
            label={t.dashboardCore}
            value={board.core_running ? t.running : t.notRunning}
            hint={t.dashboardHintState}
          />
          <Metric
            icon={Database}
            label={t.dashboardDatabase}
            value={board.database_enabled ? t.enabled : t.disabled}
            hint={t.dashboardHintState}
            color="teal"
          />
          <Metric
            icon={Clock3}
            label={t.dashboardUptime}
            value={uptime}
            hint={t.dashboardHintState}
            color="violet"
          />
        </div>
      </section>
      <section aria-label={t.dashboardUsers}>
        <div className="section-heading">
          <h2>{t.dashboardUsers}</h2>
          <a href="#overview">
            {t.dashboardOpen}
            <ArrowRight size={14} />
          </a>
        </div>
        <div className="metrics-grid" data-testid="dashboard-users">
          <Metric icon={Users} label={t.dashboardOnline} value={number(board.users_online)} hint={t.dashboardHintCounter} />
          <Metric icon={Users} label={t.dashboardPeakUsers} value={number(board.users_peak)} hint={t.dashboardHintCounter} color="violet" />
          <Metric icon={Activity} label={t.dashboardLogins} value={number(board.users_logins)} hint={t.dashboardHintCounter} color="teal" />
        </div>
      </section>
      <section aria-label={t.dashboardNetwork}>
        <div className="section-heading">
          <h2>{t.dashboardNetwork}</h2>
          <a href="#connections">
            {t.dashboardOpen}
            <ArrowRight size={14} />
          </a>
        </div>
        <div className="metrics-grid" data-testid="dashboard-network">
          <Metric icon={Network} label={t.dashboardActive} value={number(board.connections_active)} hint={t.dashboardHintCounter} />
          <Metric icon={Network} label={t.dashboardPeakConnections} value={number(board.connections_peak)} hint={t.dashboardHintCounter} color="violet" />
          <Metric icon={Cable} label={t.dashboardAccepted} value={number(board.connections_accepted)} hint={t.dashboardHintCounter} color="teal" />
          <Metric icon={ShieldCheck} label={t.dashboardRejected} value={number(board.connections_rejected)} hint={t.dashboardHintCounter} color="amber" />
          <Metric icon={Cable} label={t.dashboardClosed} value={number(board.connections_closed)} hint={t.dashboardHintCounter} />
          <Metric icon={ShieldCheck} label={t.dashboardSecurityRejected} value={number(board.security_rejected)} hint={t.dashboardHintCounter} color="amber" />
          <Metric icon={LockKeyhole} label={t.dashboardSecurityClosed} value={number(board.security_closed)} hint={t.dashboardHintCounter} color="amber" />
          <Metric icon={Activity} label={t.dashboardAdc} value={number(board.adc_frames)} hint={t.dashboardHintCounter} color="violet" />
          <Metric icon={LockKeyhole} label={t.dashboardAdcs} value={number(board.adcs_handshakes)} hint={t.dashboardHintCounter} color="teal" />
          <Metric icon={Network} label={t.dashboardBytesIn} value={number(board.bytes_received)} hint={t.dashboardHintCounter} />
          <Metric icon={Network} label={t.dashboardBytesOut} value={number(board.bytes_sent)} hint={t.dashboardHintCounter} color="violet" />
        </div>
      </section>
      <section aria-label={t.dashboardChat}>
        <div className="section-heading">
          <h2>{t.dashboardChat}</h2>
          <a href="#chat">
            {t.dashboardOpen}
            <ArrowRight size={14} />
          </a>
        </div>
        <div className="metrics-grid" data-testid="dashboard-chat">
          <Metric icon={MessageSquare} label={t.dashboardChatReceived} value={number(board.chat_received)} hint={t.dashboardHintCounter} />
          <Metric icon={MessageSquare} label={t.dashboardChatPublic} value={number(board.chat_public)} hint={t.dashboardHintCounter} color="violet" />
          <Metric icon={LockKeyhole} label={t.dashboardChatPrivate} value={number(board.chat_private)} hint={t.dashboardHintCounter} color="teal" />
          <Metric icon={ShieldCheck} label={t.dashboardChatRejected} value={number(board.chat_rejected)} hint={t.dashboardHintCounter} color="amber" />
          <Metric icon={ShieldCheck} label={t.dashboardChatFlood} value={number(board.chat_flood_limited)} hint={t.dashboardHintCounter} color="amber" />
        </div>
      </section>
      <section aria-label={t.dashboardSearch}>
        <div className="section-heading">
          <h2>{t.dashboardSearch}</h2>
          <a href="#search">
            {t.dashboardOpen}
            <ArrowRight size={14} />
          </a>
        </div>
        <div className="metrics-grid" data-testid="dashboard-search">
          <Metric icon={Search} label={t.dashboardSearchQueries} value={number(board.search_queries)} hint={t.dashboardHintCounter} />
          <Metric icon={Search} label={t.dashboardSearchRouted} value={number(board.search_routed)} hint={t.dashboardHintCounter} color="teal" />
          <Metric icon={ShieldCheck} label={t.dashboardSearchRejected} value={number(board.search_rejected)} hint={t.dashboardHintCounter} color="amber" />
          <Metric icon={Search} label={t.dashboardSearchResults} value={number(board.search_results)} hint={t.dashboardHintCounter} color="violet" />
          <Metric icon={Search} label={t.dashboardSearchTracked} value={number(board.search_tracked)} hint={t.dashboardHintGauge} />
        </div>
      </section>
      <section aria-label={t.dashboardTransfers}>
        <div className="section-heading">
          <h2>{t.dashboardTransfers}</h2>
          <a href="#connections">
            {t.dashboardOpen}
            <ArrowRight size={14} />
          </a>
        </div>
        <div className="metrics-grid" data-testid="dashboard-transfers">
          <Metric icon={Cable} label={t.dashboardCtm} value={number(board.transfers_ctm)} hint={t.dashboardHintCounter} />
          <Metric icon={Cable} label={t.dashboardRcm} value={number(board.transfers_rcm)} hint={t.dashboardHintCounter} color="violet" />
          <Metric icon={ShieldCheck} label={t.dashboardPeerUnsupported} value={number(board.transfers_peer_unsupported)} hint={t.dashboardHintCounter} color="amber" />
          <Metric icon={ShieldCheck} label={t.dashboardPeerFailed} value={number(board.transfers_peer_failed)} hint={t.dashboardHintCounter} color="amber" />
        </div>
      </section>
      <section aria-label={t.dashboardModeration}>
        <div className="section-heading">
          <h2>{t.dashboardModeration}</h2>
          <a href="#kick">
            {t.dashboardOpen}
            <ArrowRight size={14} />
          </a>
        </div>
        <div className="metrics-grid" data-testid="dashboard-moderation">
          <Metric icon={Users} label={t.dashboardKicks} value={number(board.kicks_succeeded)} hint={t.dashboardHintCounter} />
          <Metric icon={ShieldCheck} label={t.dashboardKicksRefused} value={number(board.kicks_refused)} hint={t.dashboardHintCounter} color="amber" />
          <Metric icon={MessageSquare} label={t.dashboardMutes} value={number(board.mutes_applied)} hint={t.dashboardHintCounter} color="violet" />
          <Metric icon={MessageSquare} label={t.dashboardPublicBlocked} value={number(board.mutes_public_blocked)} hint={t.dashboardHintCounter} color="teal" />
          <Metric icon={LockKeyhole} label={t.dashboardPrivateBlocked} value={number(board.mutes_private_blocked)} hint={t.dashboardHintCounter} color="teal" />
        </div>
      </section>
      {sample ? (
        <section aria-label={t.monitoringTitle}>
          <div className="section-heading">
            <h2>{t.monitoringTitle}</h2>
          </div>
          <div className="metrics-grid" data-testid="dashboard-process">
            <Metric icon={Activity} label={t.monitoringCpuUser} value={sample.available ? number(sample.cpu_user_ms) : "—"} hint={t.monitoringHintSample} />
            <Metric icon={Activity} label={t.monitoringCpuSystem} value={sample.available ? number(sample.cpu_system_ms) : "—"} hint={t.monitoringHintSample} color="violet" />
            <Metric icon={Clock3} label={t.monitoringCpuDelta} value={sample.available ? number(sample.cpu_delta_ms) : "—"} hint={t.monitoringHintSample} color="teal" />
            <Metric icon={Clock3} label={t.monitoringCpuWindow} value={sample.available ? number(sample.cpu_window_ms) : "—"} hint={t.monitoringHintSample} />
            <Metric icon={Server} label={t.monitoringRss} value={sample.available ? formatBytes(sample.rss_bytes, locale) : "—"} hint={t.monitoringHintSample} color="violet" />
            <Metric icon={Users} label={t.monitoringAccepted} value={number(sample.sessions_accepted)} hint={t.monitoringHintSample} />
            <Metric icon={Users} label={t.monitoringNegotiating} value={number(sample.sessions_negotiating)} hint={t.monitoringHintSample} color="teal" />
            <Metric icon={Users} label={t.monitoringIdentified} value={number(sample.sessions_identified)} hint={t.monitoringHintSample} />
            <Metric icon={Users} label={t.monitoringReady} value={number(sample.sessions_ready)} hint={t.monitoringHintSample} color="violet" />
            <Metric icon={Users} label={t.monitoringClosing} value={number(sample.sessions_closing)} hint={t.monitoringHintSample} color="amber" />
            <Metric icon={Activity} label={t.monitoringCreated} value={number(sample.sessions_created)} hint={t.monitoringHintSample} />
            <Metric icon={Activity} label={t.monitoringCompleted} value={number(sample.sessions_completed)} hint={t.monitoringHintSample} color="teal" />
            <Metric icon={Clock3} label={t.monitoringLoginLast} value={formatLatency(sample.login_samples, sample.login_last_ms, locale, t.monitoringNoSample)} hint={t.monitoringHintSample} />
            <Metric icon={Clock3} label={t.monitoringLoginMax} value={formatLatency(sample.login_samples, sample.login_max_ms, locale, t.monitoringNoSample)} hint={t.monitoringHintSample} color="amber" />
            <Metric icon={Activity} label={t.monitoringReactorLast} value={formatLatency(sample.reactor_samples, sample.reactor_last_ms, locale, t.monitoringNoSample)} hint={t.monitoringHintSample} color="violet" />
            <Metric icon={Activity} label={t.monitoringReactorMax} value={formatLatency(sample.reactor_samples, sample.reactor_max_ms, locale, t.monitoringNoSample)} hint={t.monitoringHintSample} color="amber" />
          </div>
        </section>
      ) : null}
    </div>
  );
}
