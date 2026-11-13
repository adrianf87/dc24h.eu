import { Activity, Search } from "lucide-react";
import type { Copy } from "../languages/us";
import type { HubSnapshot } from "../lib/status";
import type { SearchCounter, SearchConfigKey } from "../lib/search-status";
import { Metric } from "./Metric";
const counters: readonly (readonly [SearchCounter, keyof Copy])[] = [
  ["queries_received", "searchQueriesReceived"],
  ["queries_routed", "searchQueriesRouted"],
  ["queries_rejected", "searchQueriesRejected"],
  ["queries_rate_limited", "searchQueriesRateLimited"],
  ["queries_ignored", "searchQueriesIgnored"],
  ["query_errors", "searchQueryErrors"],
  ["query_queued", "searchQueryQueued"],
  ["query_unavailable", "searchQueryUnavailable"],
  ["query_refused", "searchQueryRefused"],
  ["query_canceled", "searchQueryCanceled"],
  ["results_received", "searchResultsReceived"],
  ["results_routed", "searchResultsRouted"],
  ["results_rejected", "searchResultsRejected"],
  ["results_rate_limited", "searchResultsRateLimited"],
  ["results_recipient_limited", "searchResultsRecipientLimited"],
  ["result_errors", "searchResultErrors"],
  ["result_queued", "searchResultQueued"],
  ["result_unavailable", "searchResultUnavailable"],
  ["result_refused", "searchResultRefused"],
];
const settings: readonly (readonly [SearchConfigKey, keyof Copy])[] = [
  ["queries_enabled", "searchQueriesEnabled"],
  ["results_enabled", "searchResultsEnabled"],
  ["guest_queries", "searchGuestQueries"],
  ["guest_results", "searchGuestResults"],
  ["require_result_token", "searchRequireToken"],
  ["max_active_searches", "searchMaxActive"],
  ["max_active_per_user", "searchMaxOwner"],
  ["search_lifetime_seconds", "searchLifetime"],
  ["query_rate", "searchQueryRate"],
  ["query_burst", "searchQueryBurst"],
  ["query_byte_rate", "searchQueryByteRate"],
  ["query_byte_burst", "searchQueryByteBurst"],
  ["result_rate", "searchResultRate"],
  ["result_burst", "searchResultBurst"],
  ["result_byte_rate", "searchResultByteRate"],
  ["result_byte_burst", "searchResultByteBurst"],
];
export function SearchStatistics({ snapshot, t, number }: { snapshot: HubSnapshot | null; t: Copy; number: (value: number | undefined) => string }) {
  const search = snapshot?.search;
  return <div className="chat-statistics" data-testid="search-details">
    <p className="chat-scope">{t.searchScope}</p>
    {!search && <p role="status">{t.searchUnavailable}</p>}
    <div className="metrics-grid" data-testid="search-token-statistics">
      <Metric icon={Search} label={t.searchTracked} value={number(search?.statistics.tracked_searches)} hint={t.searchTrackedHint} />
    </div>
    {([0, 10] as const).map((start, group) => <section key={start}>
      <div className="section-heading"><h2>{group === 0 ? t.searchQueriesTitle : t.searchResultsTitle}</h2></div>
      <div className="metrics-grid" data-testid={group === 0 ? "search-query-statistics" : "search-result-statistics"}>
        {counters.slice(start, group === 0 ? 10 : 19).map(([key, label]) => <Metric key={key} icon={Activity} label={t[label]} value={number(search?.statistics[key])} hint={t.searchCounterHint} />)}
      </div>
    </section>)}
    <section className="card search-configuration" data-testid="search-configuration">
      <h2>{t.searchSettingsTitle}</h2><p className="chat-scope">{t.searchSettingsNote}</p>
      <dl>{settings.map(([key, label]) => {
        const value = search?.configuration[key];
        return <div className="search-setting" key={key}><dt>{t[label]}<code>{key}</code></dt>
          <dd data-setting={key}>{typeof value === "boolean" ? (value ? t.enabled : t.disabled) : number(value)}</dd></div>;
      })}</dl>
    </section>
  </div>;
}
