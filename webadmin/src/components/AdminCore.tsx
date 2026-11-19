import {useEffect, useRef, useState} from "react";
import {parseAdmin, ADMIN_COUNTERS} from "../lib/admin";
import type {AdminSnapshot} from "../lib/admin";
import {parseAccess} from "../lib/access";
import {adminHeaders, clearAdminGrant, setAdminGrant} from "../lib/adminAccess";
import {readAccountResponse} from "../lib/accounts";
import type {Copy} from "../languages/us";

export function AdminCore({t}:{t:Copy}) {
 const [snapshot,setSnapshot]=useState<AdminSnapshot|null>(null),[busy,setBusy]=useState(false),[proving,setProving]=useState(false);
 const [accessMessage,setAccessMessage]=useState(""),[pending,setPending]=useState<string[]>([]);
 const proof=useRef<HTMLInputElement>(null);
 const catalog=useRef<AbortController|null>(null);
 const access=useRef<AbortController|null>(null);
 async function load(){
  catalog.current?.abort();const controller=new AbortController();catalog.current=controller;setBusy(true);
  const timer=setTimeout(()=>controller.abort(),5000);
  try {
   const response=await fetch("./admin-api/v1/admin",{signal:controller.signal,cache:"no-store",credentials:"same-origin",redirect:"error",headers:adminHeaders()});
   if(!response.ok)throw new Error();const value=parseAdmin(await readAccountResponse(response));
   if(catalog.current===controller&&!controller.signal.aborted)setSnapshot(value);
  }catch{if(catalog.current===controller)setSnapshot(null);}
  finally{clearTimeout(timer);if(catalog.current===controller)setBusy(false);}
 }
 useEffect(()=>{void load();return()=>{catalog.current?.abort();access.current?.abort();catalog.current=null;access.current=null;if(proof.current)proof.current.value="";};},[]);
 async function prove(event:React.FormEvent<HTMLFormElement>){
  event.preventDefault();if(proving)return;
  const data=new FormData(event.currentTarget);
  const body=new URLSearchParams();
  body.set("actor_id",String(data.get("actor_id")??""));
  body.set("proof",String(data.get("proof")??""));
  if(proof.current)proof.current.value="";
  access.current?.abort();const controller=new AbortController();access.current=controller;
  setProving(true);setAccessMessage("");
  const timer=setTimeout(()=>controller.abort(),15000);
  try{
   const response=await fetch("./admin-api/v1/admin/access",{method:"POST",signal:controller.signal,cache:"no-store",credentials:"same-origin",redirect:"error",
    headers:{"Content-Type":"application/x-www-form-urlencoded","X-DC24H-Admin":"1"},body});
   const value=parseAccess(await readAccountResponse(response));
   if(!response.ok||access.current!==controller)throw new Error();
   setAdminGrant(value.grant);
   setPending(value.permissions.filter(item=>!item.implemented).map(item=>item.key));
   setAccessMessage(t.accessGranted);
   void load();
  }catch{
   if(access.current===controller){clearAdminGrant();setPending([]);setAccessMessage(t.accessUnavailable);}
  }finally{body.forEach((_,key)=>body.set(key,""));clearTimeout(timer);if(access.current===controller)setProving(false);}
 }
 function forget(){clearAdminGrant();setPending([]);setAccessMessage("");if(proof.current)proof.current.value="";}
 return <section className="card accounts-card" data-testid="admin-core">
  <div className="card-heading"><div><h2>{t.adminCoreTitle}</h2><p>{t.adminCoreNote}</p></div><button className="button secondary" type="button" disabled={busy||proving} onClick={()=>void load()}>{t.accountsReload}</button></div>
  <form onSubmit={event=>void prove(event)}>
   <fieldset disabled={proving}>
    <legend>{t.accessTitle}</legend>
    <p>{t.accessSub}</p>
    <label>{t.accessActor}<input name="actor_id" required pattern="[1-9][0-9]{0,18}" inputMode="numeric" autoComplete="off" /></label>
    <label>{t.accessProof}<input ref={proof} name="proof" required type="password" maxLength={1024} autoComplete="off" /></label>
    <button className="button primary" type="submit">{t.accessSubmit}</button>
    <button className="button secondary" type="button" onClick={forget}>{t.accessClear}</button>
   </fieldset>
  </form>
  {accessMessage?<p data-testid="admin-access-result">{accessMessage}</p>:null}
  {pending.length>0?<ul data-testid="admin-access-pending">{pending.map(key=><li key={key}><code>{key}</code> {t.permissionPending}</li>)}</ul>:null}
  {!snapshot?<p role="status">{busy?t.accountsWorking:t.adminCoreUnavailable}</p>:<>
   <p>{t.adminCoreCountersNote}</p>
   <dl className="admin-counters">{ADMIN_COUNTERS.map(key=><div key={key}><dt>{t[`admin_${key}`]}</dt><dd>{BigInt(snapshot.statistics[key]).toLocaleString()}</dd></div>)}</dl>
   <div className="accounts-table-wrap"><table><caption>{t.adminCoreCatalog}</caption><thead><tr><th>{t.accountsAction}</th><th>{t.adminCoreEntry}</th></tr></thead>
   <tbody>{snapshot.actions.map(action=><tr key={action.key}><td><code>{action.key}</code></td><td>{action.available?t.adminCoreProven:t.adminCoreLocal}</td></tr>)}</tbody></table></div>
  </>}
 </section>;
}
