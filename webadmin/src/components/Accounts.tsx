import {ACCOUNT_ROLES} from "../lib/roles";
import { useEffect, useRef, useState } from "react";
import { parseAccount, parseAccounts, parseStaff, isStaff, readAccountResponse } from "../lib/accounts";
import type { Account } from "../lib/accounts";
import type { Copy } from "../languages/us";

export function Accounts({t,staff=false}:{t:Copy;staff?:boolean}) {
  const [items,setItems]=useState<Account[]>([]), [next,setNext]=useState<string|null>(null);
  const [cursor,setCursor]=useState("0"),[selected,setSelected]=useState<Account|null>(null);
  const [action,setAction]=useState(staff?"role":"create"),[busy,setBusy]=useState(false),[available,setAvailable]=useState(false);
  const [message,setMessage]=useState("");const form=useRef<HTMLFormElement>(null);
  const alive=useRef(true);const pending=useRef<AbortController|null>(null);
  async function load(after="0") {
    pending.current?.abort();const controller=new AbortController();pending.current=controller;
    setBusy(true);setMessage("");const timer=setTimeout(()=>controller.abort(),5000);
    try {
      const response=await fetch(`./admin-api/v1/accounts?${staff?"staff_after":"after"}=${after}`,{signal:controller.signal,cache:"no-store",credentials:"same-origin",redirect:"error"});
      if(!response.ok)throw new Error();
      const page=(staff?parseStaff:parseAccounts)(await readAccountResponse(response));if(!alive.current||controller.signal.aborted)return;
      setItems(page.accounts);setNext(page.next_after);setCursor(after);setAvailable(true);setSelected(null);
    }catch{if(alive.current&&pending.current===controller){setAvailable(false);setItems([]);setSelected(null);setNext(null);setMessage(t.accountsUnavailable);}}
    finally{clearTimeout(timer);if(alive.current&&pending.current===controller)setBusy(false);}
  }
  useEffect(()=>{alive.current=true;void load();return()=>{alive.current=false;pending.current?.abort();};},[]);
  async function submit(event:React.FormEvent<HTMLFormElement>) {
    event.preventDefault();if(busy||!available||(!selected&&action!=="create"))return;
    const element=event.currentTarget,data=new FormData(element);const body=new URLSearchParams();
    body.set("proof",String(data.get("proof")??""));
    if(action!=="password")body.set("actor_id",String(data.get("actor_id")??""));
    if(action==="role")body.set("role",String(data.get("role")??""));
    if(action==="create")body.set("name",String(data.get("name")??""));else body.set("revision",selected!.revision);
    if(["create","password","reset"].includes(action))body.set("password",String(data.get("password")??""));
    const path=action==="create"?"":`/${selected!.id}/${action}`;
    setBusy(true);setMessage("");const controller=new AbortController();pending.current=controller;
    const timer=setTimeout(()=>controller.abort(),20000);
    // Do not keep proofs/passwords in React state, storage, URLs or exports.
    element.reset();
    try {
      const response=await fetch(`./admin-api/v1/accounts${path}`,{method:"POST",signal:controller.signal,
        credentials:"same-origin",redirect:"error",cache:"no-store",headers:{"Content-Type":"application/x-www-form-urlencoded","X-DC24H-Admin":"1"},body});
      const value=await readAccountResponse(response);
      if(!value||typeof value!=="object")throw new Error();const result=value as Record<string,unknown>;
      if(!alive.current)return;
      if(!response.ok){setMessage(result.error==="outcome_unknown"?t.accountsUnknown:response.status===409?t.accountsStale:response.status===429?t.accountsRate:t.accountsRefused);return;}
      if(result.maintenance!==true)throw new Error();const account=parseAccount(result.account);
      setItems(old=>old.map(a=>a.id===account.id?account:a).filter(a=>!staff||isStaff(a)));setSelected(staff&&!isStaff(account)?null:account);setMessage(t.accountsSaved);
    }catch{if(alive.current)setMessage(t.accountsUnknown);}
    finally{body.forEach((_,key)=>body.set(key,""));clearTimeout(timer);if(alive.current)setBusy(false);}
  }
  return <section className="card accounts-card" aria-label={staff?t.operatorsTitle:t.accountsTitle}>
    <div className="card-heading"><div><h2>{staff?t.operatorsTitle:t.accountsTitle}</h2><p>{staff?t.operatorsNote:t.accountsNote}</p></div>
      <button className="button secondary" disabled={busy} onClick={()=>void load(cursor)}>{t.accountsReload}</button></div>
    <p className="accounts-maintenance">{t.accountsMaintenance} {t.accountsRoleNote} {staff&&t.operatorsOwnership}</p>
    {message&&<p role="status" className="accounts-message">{message}</p>}
    <div className="accounts-table-wrap"><table><caption>{t.accountsTable}</caption><thead><tr>
      <th>ID</th><th>{t.accountsName}</th><th>{t.accountsRole}</th><th>{t.accountsStatus}</th><th>{t.accountsRevision}</th>
    </tr></thead><tbody>{items.map(account=><tr key={account.id} aria-selected={selected?.id===account.id}>
      <td>{account.id}</td><td><button disabled={busy} onClick={()=>{setSelected(account);setAction("disable");form.current?.reset();setMessage("");}}>{account.name}</button></td>
      <td>{t[`role_${account.role}`]}</td>
      <td>{account.status==="active"?t.enabled:t.disabled}</td><td>{account.revision}</td>
    </tr>)}</tbody></table></div>
    {available&&!items.length&&<p>{t.accountsEmpty}</p>}
    <div className="accounts-buttons"><button disabled={busy||cursor==="0"} onClick={()=>void load()}>{t.accountsFirst}</button>
      <button disabled={busy||!next} onClick={()=>void load(next!)}>{t.accountsNext}</button></div>
    {selected&&<p>{t.accountsSelected}: <strong>{selected.name}</strong> · ID {selected.id} · {t.accountsUpdated}: {new Date(selected.updated_at*1000).toISOString()}</p>}
    <form ref={form} onSubmit={event=>void submit(event)}>
      <fieldset disabled={busy||!available}><legend>{t.accountsAction}</legend>
        <label>{t.accountsAction}<select aria-label={t.accountsAction} value={action} onChange={e=>{setAction(e.target.value);form.current?.reset();setMessage("");}}>
          {!staff&&<option value="create">{t.accountsCreate}</option>}<option value="activate">{t.accountsActivate}</option>
          <option value="disable">{t.accountsDisable}</option><option value="reset">{t.accountsReset}</option><option value="password">{t.accountsPassword}</option><option value="role">{t.accountsChangeRole}</option>
        </select></label>
        {action==="create"?<label>{t.accountsName}<input name="name" required maxLength={64} autoComplete="off"/></label>:<p>{selected?t.accountsSelected+": "+selected.name:t.accountsSelect}</p>}
        {action==="role"&&<label>{t.accountsNewRole}<select aria-label={t.accountsNewRole} name="role" required>{ACCOUNT_ROLES.filter(r=>r!=="owner").map(r=><option key={r} value={r}>{t[`role_${r}`]}</option>)}</select></label>}
        {action!=="password"&&<label>{t.accountsActor}<input name="actor_id" required inputMode="numeric" pattern="[1-9][0-9]{0,18}" autoComplete="off"/></label>}
        <label>{action==="password"?t.accountsCurrentProof:t.accountsAdminProof}<input name="proof" type="password" required maxLength={1024} autoComplete="off"/></label>
        {["create","password","reset"].includes(action)&&<label>{t.accountsNewPassword}<input name="password" type="password" required minLength={15} maxLength={1024} autoComplete="new-password"/></label>}
        <label className="accounts-confirm"><input type="checkbox" required/>{t.accountsConfirm}</label>
        <button className="button primary" disabled={action!=="create"&&!selected} type="submit">{busy?t.accountsWorking:t.accountsApply}</button>
      </fieldset>
    </form>
  </section>;
}
