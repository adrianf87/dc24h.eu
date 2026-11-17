import {MUTE_COUNTERS} from "../lib/mute";
import type {HubSnapshot} from "../lib/status";
import type {Copy} from "../languages/us";
export function MuteStatistics({snapshot,t}:{snapshot:HubSnapshot|null;t:Copy}){
 const counters=snapshot?.mute;
 return <section className="card accounts-card" data-testid="mute-statistics">
  <div className="card-heading"><div><h2>{t.muteTitle}</h2><p>{t.muteNote}</p></div></div>
  <p><code>+mute &lt;nickname&gt; [public|private|all]</code></p><p><code>+unmute &lt;nickname&gt; [public|private|all]</code></p><p>{t.muteCommandNote}</p>
  {counters?<dl className="admin-counters">{MUTE_COUNTERS.map(key=><div key={key}><dt>{t[`mute_${key}`]}</dt><dd>{BigInt(counters[key]).toLocaleString()}</dd></div>)}</dl>:<p>{t.unavailable}</p>}
 </section>;
}
