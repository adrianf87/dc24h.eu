import type { HubSnapshot } from "../lib/status";
import type { Copy } from "../languages/us";
import type { ModerationOperation, ModerationRule, ModerationSubject, ModerationTargetRule } from "../lib/moderation";

function operationLabel(t: Copy, operation: ModerationOperation): string {
  switch (operation) {
    case "kick": return t.op_kick;
    case "mute": return t.op_mute;
    case "unmute": return t.op_unmute;
    case "ban_account": return t.op_ban_account;
    case "ban_cid": return t.op_ban_cid;
    case "ban_address": return t.op_ban_address;
    case "revoke_ban": return t.op_revoke_ban;
  }
}
function subjectLabel(t: Copy, subject: ModerationSubject): string {
  switch (subject) {
    case "session": return t.subject_session;
    case "account": return t.subject_account;
    case "cid": return t.subject_cid;
    case "address": return t.subject_address;
    case "ban_record": return t.subject_ban_record;
  }
}
function targetLabel(t: Copy, target: ModerationTargetRule): string {
  switch (target) {
    case "lower_role": return t.target_lower_role;
    case "lower_account_or_unmapped": return t.target_lower_account_or_unmapped;
    case "permission_only": return t.target_permission_only;
    case "creator_or_lower": return t.target_creator_or_lower;
  }
}

export function ModerationRules({ snapshot, t }: { snapshot: HubSnapshot | null; t: Copy }) {
  const rules = snapshot?.configuration?.moderation;
  return <section className="card roles-card" data-testid="moderation-rules">
    <div className="card-heading"><div><h2>{t.moderationTitle}</h2><p>{t.moderationNote}</p></div><span className="outline-label">{t.readOnly}</span></div>
    {!rules ? <p role="status">{t.moderationUnavailable}</p> : <div className="roles-table"><table>
      <caption>{t.moderationTable}</caption>
      <thead><tr>
        <th>{t.moderationOperation}</th><th>{t.moderationPermission}</th><th>{t.moderationSubject}</th><th>{t.moderationTarget}</th>
        <th>{t.moderationSelf}</th><th>{t.moderationGuest}</th><th>{t.moderationVip}</th><th>{t.moderationBlocks}</th>
      </tr></thead>
      <tbody>{rules.map((rule: ModerationRule) => <tr key={rule.operation} data-operation={rule.operation}>
        <td>{operationLabel(t, rule.operation)}</td>
        <td><code>{rule.permission}</code></td>
        <td>{subjectLabel(t, rule.subject)}</td>
        <td>{targetLabel(t, rule.target)}</td>
        <td>{rule.forbid_self ? t.rolesYes : t.rolesNo}</td>
        <td>{rule.guest_target ? t.rolesYes : t.rolesNo}</td>
        <td>{rule.vip_delegable ? t.rolesYes : t.rolesNo}</td>
        <td>{rule.requires_blocks ? t.rolesYes : t.rolesNo}</td>
      </tr>)}</tbody>
    </table></div>}
  </section>;
}
