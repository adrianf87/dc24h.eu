import { Activity, ArrowRight, LockKeyhole, MessageSquare, ShieldCheck } from "lucide-react";
import type { Copy } from "../languages/us";
import type { HubSnapshot } from "../lib/status";
import { Metric } from "./Metric";

export function ChatStatistics({ snapshot, t, number }: {
  snapshot: HubSnapshot | null;
  t: Copy;
  number: (value: number | undefined) => string;
}) {
  const chat = snapshot?.chat;
  return (
    <div className="chat-statistics" data-testid="chat-details">
      <p className="chat-scope">{t.chatScope}</p>
      <section aria-labelledby="chat-messages-title">
        <div className="section-heading"><h2 id="chat-messages-title">{t.chatMessagesTitle}</h2></div>
        <div className="metrics-grid" data-testid="chat-message-statistics">
          <Metric icon={MessageSquare} label={t.chatReceived} value={number(chat?.received)} hint={t.chatReceivedHint} />
          <Metric icon={Activity} label={t.publicChat} value={number(chat?.public_dispatches)} hint={t.chatPublicHint} />
          <Metric icon={LockKeyhole} label={t.privateChat} value={number(chat?.private_dispatches)} hint={t.chatPrivateHint} color="violet" />
          <Metric icon={ShieldCheck} label={t.chatRejected} value={number(chat?.rejected)} hint={t.chatRejectedHint} color="amber" />
          <Metric icon={ShieldCheck} label={t.chatFlood} value={number(chat?.flood_limited)} hint={t.chatFloodHint} color="amber" />
          <Metric icon={Activity} label={t.chatErrors} value={number(chat?.errors)} hint={t.chatErrorsHint} color="amber" />
        </div>
      </section>
      <section aria-labelledby="chat-queues-title">
        <div className="section-heading"><h2 id="chat-queues-title">{t.chatQueuesTitle}</h2></div>
        <p className="chat-scope">{t.chatQueuesNote}</p>
        <div className="metrics-grid" data-testid="chat-queue-statistics">
          <Metric icon={ArrowRight} label={t.chatQueued} value={number(chat?.queued)} hint={t.chatQueuedHint} color="teal" />
          <Metric icon={Activity} label={t.chatUnavailable} value={number(chat?.unavailable)} hint={t.chatUnavailableHint} color="violet" />
          <Metric icon={ShieldCheck} label={t.chatRefused} value={number(chat?.refused)} hint={t.chatRefusedHint} color="amber" />
        </div>
      </section>
    </div>
  );
}
