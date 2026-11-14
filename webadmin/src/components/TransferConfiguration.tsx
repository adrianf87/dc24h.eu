import type { Copy } from "../languages/us";
import type { HubSnapshot } from "../lib/status";
import { TRANSFER_FLAGS, TRANSFER_LIMITS } from "../lib/transfer-status";
const labels: readonly (keyof Copy)[] = ["transferCtmEnabled", "transferRcmEnabled", "transferGuestInitiate", "transferGuestReceive",
  "transferRequestRate", "transferRequestBurst", "transferRequestByteRate", "transferRequestByteBurst",
  "transferIncomingRate", "transferIncomingBurst", "transferIncomingByteRate", "transferIncomingByteBurst"];
export function TransferConfiguration({ snapshot, t, number }: { snapshot: HubSnapshot | null; t: Copy; number: (value: number | undefined) => string }) {
  const settings = snapshot?.transfers?.configuration;
  const protocols = settings?.allowed_protocols;
  return <section className="card search-configuration" data-testid="transfer-configuration">
    <h2>{t.transferSettingsTitle}</h2><p className="chat-scope">{t.transferSettingsNote}</p>
    {!settings && <p role="status">{t.transferSettingsUnavailable}</p>}
    <dl>{[...TRANSFER_FLAGS, ...TRANSFER_LIMITS].map((key, index) => {
      const value = settings?.[key];
      return <div className="search-setting" key={key}><dt>{t[labels[index]]}<code>{key}</code></dt>
        <dd data-transfer-setting={key}>{typeof value === "boolean" ? (value ? t.enabled : t.disabled) : number(value)}</dd></div>;
    })}
      <div className="search-setting transfer-protocol-setting"><dt>{t.transferAllowedProtocols}<code>allowed_protocols</code></dt>
        <dd data-transfer-setting="allowed_protocols">{protocols === undefined ? "—" : protocols === "any" ? t.transferProtocolsAny : protocols.length ? protocols.join(", ") : t.transferProtocolsNone}</dd>
      </div>
    </dl>
  </section>;
}
