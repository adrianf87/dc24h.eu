import {useEffect,useRef,useState} from "react";
import {parseBan,parseBans} from "../lib/bans";
import type {Ban} from "../lib/bans";
import {readAccountResponse} from "../lib/accounts";
import type {Copy} from "../languages/us";
export function Bans({t}:{t:Copy}){
 const [items,setItems]=useState<Ban[]>([]),[next,setNext]=useState<string|null>(null),[cursor,setCursor]=useState("0"),[selected,setSelected]=useState<Ban|null>(null);
 const [busy,setBusy]=useState(false),[available,setAvailable]=useState(false),[message,setMessage]=useState("");
 const pending=useRef<AbortController|null>(null);const form=useRef<HTMLFormElement>(null);
 async function load(after="0"){
  pending.current?.abort();const controller=new AbortController();pending.current=controller;setBusy(true);setMessage("");const timer=setTimeout(()=>controller.abort(),5000);
  try{const response=await fetch(`./admin-api/v1/bans?after=${after}`,{cache:"no-store",credentials:"same-origin",redirect:"error",signal:controller.signal});if(!response.ok)throw new Error();
   const result=parseBans(await readAccountResponse(response));if(pending.current!==controller||controller.signal.aborted)return;
   setItems(result.bans);setNext(result.next_after);setCursor(after);setSelected(null);setAvailable(true);
  }catch{if(pending.current===controller){setItems([]);setSelected(null);setNext(null);setAvailable(false);setMessage(t.accountsUnavailable);}}
  finally{clearTimeout(timer);if(pending.current===controller)setBusy(false);}
 }
 useEffect(()=>{void load();return()=>{pending.current?.abort();pending.current=null;};},[]);
 async function submit(event:React.FormEvent<HTMLFormElement>){
  event.preventDefault();if(busy||!available)return;const element=event.currentTarget,data=new FormData(element),body=new URLSearchParams();
  body.set("actor_id",String(data.get("actor_id")??""));body.set("proof",String(data.get("proof")??""));
  if(selected)body.set("revision",selected.revision);else{for(const k of ["kind","subject","reason"])body.set(k,String(data.get(k)??""));const expiry=String(data.get("expires_at")??"");body.set("expires_at",expiry?String(Math.floor(new Date(expiry).getTime()/1000)):"");}
  const path=selected?`/${selected.id}/revoke`:"";setBusy(true);setMessage("");element.reset();
  const controller=new AbortController();pending.current=controller;const timer=setTimeout(()=>controller.abort(),20000);
  try{const response=await fetch(`./admin-api/v1/bans${path}`,{method:"POST",cache:"no-store",credentials:"same-origin",redirect:"error",signal:controller.signal,headers:{"Content-Type":"application/x-www-form-urlencoded","X-DC24H-Admin":"1"},body});
   const result=await readAccountResponse(response) as Record<string,unknown>;if(pending.current!==controller)return;
   if(!response.ok){setMessage(result.error==="outcome_unknown"?t.accountsUnknown:response.status===409?t.accountsStale:t.accountsRefused);return;}
   if(result.maintenance!==true||result.restart_required!==true)throw new Error();const ban=parseBan(result.ban);setSelected(ban);setItems(old=>old.some(b=>b.id===ban.id)?old.map(b=>b.id===ban.id?ban:b):old.length<16?[...old,ban]:old);setMessage(t.bansSaved);
  }catch{if(pending.current===controller)setMessage(t.accountsUnknown);}
  finally{body.forEach((_,key)=>body.set(key,""));clearTimeout(timer);if(pending.current===controller)setBusy(false);}
 }
 return <section className="card accounts-card" data-testid="bans">
  <div className="card-heading"><div><h2>{t.bansTitle}</h2><p>{t.bansNote}</p></div><button className="button secondary" disabled={busy} onClick={()=>void load(cursor)}>{t.accountsReload}</button></div>
  <p className="accounts-maintenance">{t.bansMaintenance}</p>{message&&<p role="status">{message}</p>}
  <div className="accounts-table-wrap"><table><caption>{t.bansTable}</caption><thead><tr><th>ID</th><th>{t.bansKind}</th><th>{t.bansSubject}</th><th>{t.bansReason}</th><th>{t.accountsStatus}</th></tr></thead>
   <tbody>{items.map(b=><tr key={b.id}><td><button disabled={busy} onClick={()=>{setSelected(b);form.current?.reset();setMessage("");}}>{b.id}</button></td><td>{t[`ban_${b.kind}`]}</td><td>{b.subject}</td><td>{b.reason}</td><td>{b.revoked_at!==null?t.bansRevoked:b.expires_at!==null&&b.expires_at<=Date.now()/1000?t.bansExpired:t.enabled}</td></tr>)}</tbody></table></div>
  <div className="accounts-buttons"><button disabled={busy||cursor==="0"} onClick={()=>void load()}>{t.accountsFirst}</button><button disabled={busy||!next} onClick={()=>void load(next!)}>{t.accountsNext}</button><button disabled={busy||!available} onClick={()=>{setSelected(null);form.current?.reset();setMessage("");}}>{t.bansCreate}</button></div>
  {selected&&<p>{t.accountsSelected}: {selected.id} · {t.accountsRevision}: {selected.revision} · {t.bansExpires}: {selected.expires_at===null?t.bansPermanent:new Date(selected.expires_at*1000).toLocaleString()}</p>}
  <form ref={form} onSubmit={e=>void submit(e)}><fieldset disabled={busy||!available||selected?.revoked_at!=null}>
   <legend>{selected?t.bansRevoke:t.bansCreate}</legend>
   {!selected&&<><label>{t.bansKind}<select name="kind"><option value="account">{t.ban_account}</option><option value="cid">{t.ban_cid}</option><option value="ip">{t.ban_ip}</option></select></label><label>{t.bansSubject}<input name="subject" required maxLength={64} autoComplete="off"/></label><label>{t.bansReason}<input name="reason" required maxLength={256} autoComplete="off"/></label><label>{t.bansExpiry}<input type="datetime-local" name="expires_at"/></label></>}
   <label>{t.bansActor}<input name="actor_id" required pattern="[1-9][0-9]{0,18}" inputMode="numeric" autoComplete="off"/></label><label>{t.bansProof}<input name="proof" required type="password" maxLength={1024} autoComplete="off"/></label>
   <label className="accounts-confirm"><input type="checkbox" required/>{t.accountsConfirm}</label><button className="button primary" type="submit">{busy?t.accountsWorking:selected?t.bansRevoke:t.bansCreate}</button>
  </fieldset></form>
 </section>;
}
