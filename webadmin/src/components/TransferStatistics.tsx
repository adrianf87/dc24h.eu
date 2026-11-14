import { Cable } from "lucide-react";
import type { Copy } from "../languages/us";
import type { HubSnapshot } from "../lib/status";
import { TRANSFER_COUNTERS } from "../lib/transfer-status";
import { Metric } from "./Metric";
const labels: readonly (keyof Copy)[] = [
  "transferReceived",
  "transferRouted",
  "transferRejected",
  "transferRateLimited",
  "transferRecipientLimited",
  "transferErrors",
  "transferQueued",
  "transferRefused",
  "transferCanceled",
  "transferQueueErrors",
];
export function TransferStatistics({ snapshot, t, number }: { snapshot: HubSnapshot | null; t: Copy; number: (value: number | undefined) => string }) {
  const statistics = snapshot?.transfers?.statistics;
  return <section className="chat-statistics" data-testid="transfer-details">
    <div className="section-heading"><h2>{t.transferTitle}</h2></div>
    <p className="chat-scope">{t.transferScope}</p>
    {!statistics && <p role="status">{t.transferUnavailable}</p>}
    {([0, 10] as const).map((start, group) => <section key={start} data-testid={group === 0 ? "transfer-ctm" : "transfer-rcm"}>
      <div className="section-heading"><h3>{group === 0 ? t.transferCtm : t.transferRcm}</h3></div>
      <div className="metrics-grid">{TRANSFER_COUNTERS.slice(start, start + 10).map((key, index) =>
        <Metric key={key} icon={Cable} label={t[labels[index]]} value={number(statistics?.[key])} hint={index >= 6 ? t.transferQueueHint : t.transferCounterHint} />
      )}</div>
    </section>)}
    <section data-testid="transfer-peer-reports">
      <div className="section-heading"><h3>{t.transferPeerReports}</h3></div>
      <div className="metrics-grid">
        <Metric icon={Cable} label={t.transferProtocolUnsupported} value={number(statistics?.peer_protocol_unsupported)} hint={t.transferPeerHint} />
        <Metric icon={Cable} label={t.transferConnectionFailed} value={number(statistics?.peer_connection_failed)} hint={t.transferPeerHint} />
      </div>
    </section>
  </section>;
}
