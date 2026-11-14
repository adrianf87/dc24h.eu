import { Activity, ArrowRight, Cable, ShieldCheck } from "lucide-react";
import type { Copy } from "../languages/us";
import { transferIntervals } from "../lib/transfer-monitoring";
import type { TransferObservation } from "../lib/transfer-monitoring";
import { Metric } from "./Metric";

export function TransferMonitoring({ samples, available, autoRefresh, clear, t, locale }: {
  samples: TransferObservation[];
  available: boolean;
  autoRefresh: boolean;
  clear: () => void;
  t: Copy;
  locale: string;
}) {
  const intervals = transferIntervals(samples), latest = intervals.at(-1);
  const formatter = new Intl.NumberFormat(locale, { maximumFractionDigits: 1 });
  const number = (value: number | undefined) => value === undefined ? "—" : formatter.format(value);
  const max = Math.max(1, ...intervals.flatMap(value => [value.ctm, value.rcm]));
  const first = samples[0]?.at ?? 0, last = samples.at(-1)?.at ?? first;
  const points = (key: "ctm" | "rcm") => intervals.map(value =>
    `${30 + (value.at - first) / Math.max(1, last - first) * 740},${150 - value[key] / max * 120}`).join(" ");
  return <section className="search-monitoring" data-testid="transfer-monitoring" aria-labelledby="transfer-monitor-title">
    <div className="section-heading">
      <h2 id="transfer-monitor-title">{t.transferMonitorTitle}</h2>
      <span className="mini-label">{autoRefresh ? t.searchSamplingLive : t.searchSamplingPaused}</span>
    </div>
    <p className="chat-scope">{t.transferMonitorNote}</p>
    <div className="metrics-grid" data-testid="transfer-rates">
      <Metric icon={Cable} label={t.transferCtmPerSecond} value={number(latest?.ctm)} hint={t.searchRateHint} />
      <Metric icon={Activity} label={t.transferRcmPerSecond} value={number(latest?.rcm)} hint={t.searchRateHint} />
      <Metric icon={ShieldCheck} label={t.searchRefusalsPerSecond} value={number(latest?.refusals)} hint={t.searchRefusalRateHint} color="amber" />
      <Metric icon={ArrowRight} label={t.searchQueuedPerSecond} value={number(latest?.queued)} hint={t.searchQueueRateHint} color="teal" />
    </div>
    <div className="card search-activity">
      <div className="card-heading"><div><h3>{t.transferTrendTitle}</h3><p>{t.searchTrendNote}</p></div>
        <button className="search-clear" onClick={clear} disabled={!samples.length}>{t.searchClearLocal}</button>
      </div>
      <div className="plot">
        <svg viewBox="0 0 800 180" role="img" aria-label={t.transferChartLabel} preserveAspectRatio="none">
          <title>{t.transferChartLabel}</title>
          <path d="M30 30H770M30 70H770M30 110H770M30 150H770" className="chart-grid" />
          {intervals.length > 0 && (["ctm", "rcm"] as const).map(key => <g key={key} className={`search-series-${key === "ctm" ? "queries" : "results"}`}>
            <polyline points={points(key)} fill="none" strokeWidth="2.5" vectorEffect="non-scaling-stroke" />
            {intervals.map(value => <circle key={value.at} cx={30 + (value.at - first) / Math.max(1, last - first) * 740}
              cy={150 - value[key] / max * 120} r="3" />)}
          </g>)}
        </svg>
        {!intervals.length && <div className="empty-plot"><strong>{available ? t.searchWaitingSamples : t.transferUnavailable}</strong><p>{t.searchWaitingNote}</p></div>}
      </div>
      <div className="plot-footer"><span>{intervals.length ? `0 – ${number(max)} ${t.searchRateUnit}` : "—"}</span>
        <span className="search-legend"><i className="legend-dot" />{t.transferCtm}<i className="legend-dot results" />{t.transferRcm}</span>
        <span>{t.searchIntervalsLabel}: {intervals.length}</span></div>
      <details className="search-observations">
        <summary>{t.searchRecentIntervals}</summary>
        <div className="search-table-scroll"><table>
          <caption>{t.searchTableCaption}</caption>
          <thead><tr><th scope="col">{t.searchIntervalSeconds}</th><th scope="col">{t.transferCtmPerSecond}</th><th scope="col">{t.transferRcmPerSecond}</th><th scope="col">{t.searchRefusalsPerSecond}</th><th scope="col">{t.searchQueuedPerSecond}</th></tr></thead>
          <tbody>{intervals.slice(-6).map(value => <tr key={value.at}><td>{number(value.seconds)}</td><td>{number(value.ctm)}</td><td>{number(value.rcm)}</td><td>{number(value.refusals)}</td><td>{number(value.queued)}</td></tr>)}</tbody>
        </table></div>
        {!intervals.length && <p>{available ? t.searchWaitingSamples : t.transferUnavailable}</p>}
      </details>
    </div>
  </section>;
}
