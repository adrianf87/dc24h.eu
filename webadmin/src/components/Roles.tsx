import {useState} from "react";
import type {HubSnapshot} from "../lib/status";
import type {Copy} from "../languages/us";
import type {RoleKey} from "../lib/roles";
export function Roles({snapshot,t}:{snapshot:HubSnapshot|null;t:Copy}) {
 const [role,setRole]=useState<RoleKey>("registered"),policy=snapshot?.configuration?.roles;
 const grants=policy?.catalog.find(r=>r.key===role)?.permissions;
 return <section className="card roles-card" data-testid="role-policy">
  <div className="card-heading"><div><h2>{t.rolesPolicy}</h2><p>{t.rolesNote}</p></div><span className="outline-label">{t.readOnly}</span></div>
  {!policy?<p role="status">{t.rolesUnavailable}</p>:<>
   <p>{t.rolesHierarchy}: {policy.catalog.map(r=>t[`role_${r.key}`]).join(" → ")}</p>
   <p>{t.rolesHierarchyNote}</p><p>{t.rolesVip}: <code>{policy.vip_permissions.join(", ")||t.rolesNone}</code></p>
   <label className="roles-selector">{t.rolesSelect}<select value={role} onChange={event=>setRole(event.target.value as RoleKey)}>
    {policy.catalog.map(r=><option key={r.key} value={r.key}>{t[`role_${r.key}`]}</option>)}
   </select></label>
   <div className="roles-table"><table><caption>{t.rolesPermissionsFor} {t[`role_${role}`]}</caption><thead><tr><th>{t.rolesPermission}</th><th>{t.rolesGranted}</th><th>{t.rolesSupport}</th></tr></thead>
   <tbody>{policy.operations.map(p=><tr key={p.key} data-permission={p.key}><td><code>{p.key}</code></td><td>{grants?.includes(p.key)?t.rolesYes:t.rolesNo}</td><td>{p.implemented?t.rolesImplemented:t.rolesPlanned}</td></tr>)}</tbody></table></div>
   <p>{t.rolesSupportNote}</p>
  </>}
 </section>;
}
