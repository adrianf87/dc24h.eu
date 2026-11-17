import {KICK_COUNTERS} from "../lib/kick";
import type {HubSnapshot} from "../lib/status";
import type {Copy} from "../languages/us";
export function KickStatistics({snapshot,t}:{snapshot:HubSnapshot|null;t:Copy}){
 const counters=snapshot?.kick;
 return <section className="card accounts-card" data-testid="kick-statistics">
  <div className="card-heading"><div><h2>{t.kickTitle}</h2><p>{t.kickNote}</p></div></div>
  <p><code>+kick &lt;nickname&gt; [reason]</code></p><p>{t.kickCommandNote}</p>
  {counters?<dl className="admin-counters">{KICK_COUNTERS.map(key=><div key={key}><dt>{t[`kick_${key}`]}</dt><dd>{BigInt(counters[key]).toLocaleString()}</dd></div>)}</dl>:<p>{t.unavailable}</p>}
 </section>;
}
