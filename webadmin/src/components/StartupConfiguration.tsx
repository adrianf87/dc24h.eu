import { DATABASE_LIMITS, AUTHENTICATION_LIMITS } from "../lib/configuration";
import type { HubSnapshot } from "../lib/status";
import type { Copy } from "../languages/us";

export function StartupConfiguration({snapshot,t,number}:{snapshot:HubSnapshot|null;t:Copy;number:(value:number|undefined)=>string}) {
  const config=snapshot?.configuration;
  return <section className="card startup-configuration" data-testid="startup-configuration">
    <div className="card-heading"><div><h2>{t.configPolicyTitle}</h2><p>{t.configPolicyNote}</p></div><span className="outline-label">{t.readOnly}</span></div>
    {!config?<p role="status">{t.configUnavailable}</p>:<>
      <p className="configuration-mode">{t.configMode}: <strong>{config.mode==="maintenance"?t.configMaintenance:t.configHub}</strong></p>
      <div className="configuration-sections"><section><h3>{t.configDatabase}</h3><p>{t.configDatabaseNote}</p>
        <dl><div><dt>{t.configDatabaseEnabled}</dt><dd>{config.database.enabled?t.enabled:t.disabled}</dd></div>
          <div><dt>{t.configTransport}</dt><dd>{t.configUnix}</dd></div>
          {DATABASE_LIMITS.map(key=><div key={key} data-configuration-setting={key}><dt>{t[`dbSetting_${key}`]}</dt><dd>{number(config.database[key])}</dd></div>)}
        </dl>
      </section><section><h3>{t.configAuthentication}</h3><p>{t.configAuthenticationNote}</p>
        <dl><div><dt>{t.configAuthenticationEnabled}</dt><dd>{config.authentication.enabled?t.enabled:t.disabled}</dd></div>
          <div><dt>{t.configRequireTls}</dt><dd>{config.authentication.require_tls?t.enabled:t.disabled}</dd></div>
          {AUTHENTICATION_LIMITS.map(key=><div key={key} data-configuration-setting={key}><dt>{t[`authSetting_${key}`]}</dt><dd>{number(config.authentication[key])}</dd></div>)}
        </dl>
        {config.authentication.enabled&&!config.authentication.require_tls&&<p className="configuration-alert" role="note">{t.configPlainAuthentication}</p>}
      </section></div>
      <p className="configuration-footnote">{t.configRestart}</p>
    </>}
  </section>;
}
