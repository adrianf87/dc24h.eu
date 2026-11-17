import {useEffect,useRef,useState} from "react";
import {parseAdmin,ADMIN_COUNTERS} from "../lib/admin";
import type {AdminSnapshot} from "../lib/admin";
import {readAccountResponse} from "../lib/accounts";
import type {Copy} from "../languages/us";
export function AdminCore({t}:{t:Copy}) {
 const [snapshot,setSnapshot]=useState<AdminSnapshot|null>(null),[busy,setBusy]=useState(false);
 const pending=useRef<AbortController|null>(null);
 async function load(){
  pending.current?.abort();const controller=new AbortController();pending.current=controller;setBusy(true);
  const timer=setTimeout(()=>controller.abort(),5000);
  try {
   const response=await fetch("./admin-api/v1/admin",{signal:controller.signal,cache:"no-store",credentials:"same-origin",redirect:"error"});
   if(!response.ok)throw new Error();const value=parseAdmin(await readAccountResponse(response));
   if(pending.current===controller&&!controller.signal.aborted)setSnapshot(value);
  }catch{if(pending.current===controller)setSnapshot(null);}
  finally{clearTimeout(timer);if(pending.current===controller)setBusy(false);}
 }
 useEffect(()=>{void load();return()=>{pending.current?.abort();pending.current=null;};},[]);
 return <section className="card accounts-card" data-testid="admin-core">
  <div className="card-heading"><div><h2>{t.adminCoreTitle}</h2><p>{t.adminCoreNote}</p></div><button className="button secondary" disabled={busy} onClick={()=>void load()}>{t.accountsReload}</button></div>
  {!snapshot?<p role="status">{busy?t.accountsWorking:t.adminCoreUnavailable}</p>:<>
   <p>{t.adminCoreCountersNote}</p>
   <dl className="admin-counters">{ADMIN_COUNTERS.map(key=><div key={key}><dt>{t[`admin_${key}`]}</dt><dd>{BigInt(snapshot.statistics[key]).toLocaleString()}</dd></div>)}</dl>
   <div className="accounts-table-wrap"><table><caption>{t.adminCoreCatalog}</caption><thead><tr><th>{t.accountsAction}</th><th>{t.adminCoreEntry}</th></tr></thead>
   <tbody>{snapshot.actions.map(action=><tr key={action.key}><td><code>{action.key}</code></td><td>{action.available?t.adminCoreProven:t.adminCoreLocal}</td></tr>)}</tbody></table></div>
  </>}
 </section>;
}
