import { useEffect, useState } from "react";
import type { Copy } from "../languages/us";
import type { HubSnapshot } from "../lib/status";
import { decideSession, parseHubUsers } from "../lib/users";
import type { HubUsersDirectory, SessionDecision } from "../lib/users";
import { ROLE_KEYS } from "../lib/roles";
import type { RoleKey } from "../lib/roles";

export function HubUsers({ snapshot, t }: { snapshot: HubSnapshot | null; t: Copy }) {
  const [directory, setDirectory] = useState<HubUsersDirectory | null>(null);
  const [available, setAvailable] = useState(false);
  const [actor, setActor] = useState<RoleKey>("moderator");
  const roles = snapshot?.configuration?.roles;

  useEffect(() => {
    let alive = true;
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 5000);
    void (async () => {
      try {
        const response = await fetch("./admin-api/v1/users", { signal: controller.signal, cache: "no-store", credentials: "same-origin", redirect: "error" });
        if (!response.ok) throw new Error();
        const parsed = parseHubUsers(await response.json());
        if (alive) { setDirectory(parsed); setAvailable(true); }
      } catch {
        if (alive && !controller.signal.aborted) { setDirectory(null); setAvailable(false); }
      } finally {
        clearTimeout(timer);
      }
    })();
    return () => { alive = false; controller.abort(); clearTimeout(timer); };
  }, [snapshot?.server.uptime_seconds, snapshot?.users.online]);

  function decision(target: RoleKey, operation: "kick" | "mute" | "unmute"): SessionDecision | null {
    if (!roles) return null;
    return decideSession(actor, target, operation, roles);
  }

  function label(value: SessionDecision | null): string {
    if (value === "allowed") return t.usersAllowed;
    if (value === "not_lower") return t.usersNotLower;
    if (value === "missing_permission") return t.usersMissingPermission;
    return t.usersDecisionUnavailable;
  }

  return <section className="card" data-testid="hub-users" aria-label={t.usersTitle}>
    <div className="card-heading"><div><h2>{t.usersTable}</h2><p>{t.usersSub}</p></div></div>
    <p className="users-note">{t.usersNote}</p>
    {!available && <p>{t.usersUnavailable}</p>}
    {available && directory && <>
      {directory.truncated && <p>{t.usersTruncated}</p>}
      {roles && <label>{t.usersActor}
        <select aria-label={t.usersActor} value={actor} onChange={(event) => setActor(event.target.value as RoleKey)}>
          {ROLE_KEYS.map((role) => <option key={role} value={role}>{t[`role_${role}`]}</option>)}
        </select>
      </label>}
      {!roles && <p>{t.usersDecisionUnavailable}</p>}
      <p>{t.usersSelfNote} {t.usersBanNote}</p>
      <div className="accounts-table-wrap"><table>
        <caption>{t.usersTable}</caption>
        <thead><tr>
          <th>{t.usersNickname}</th><th>{t.usersSid}</th><th>{t.usersTransport}</th><th>{t.usersRole}</th>
          <th>{t.usersPublicMute}</th><th>{t.usersPrivateMute}</th><th>{t.usersActions}</th>
        </tr></thead>
        <tbody>
          {directory.users.map((user) => <tr key={user.session} data-testid="hub-user-row">
            <td>{user.nickname}</td>
            <td><code>{user.sid}</code></td>
            <td>{user.transport === "adcs" ? "ADCS" : "ADC"}</td>
            <td>{t[`role_${user.role}`]}</td>
            <td>{user.public_muted ? t.usersMuted : t.usersOpen}</td>
            <td>{user.private_muted ? t.usersMuted : t.usersOpen}</td>
            <td className="users-actions">
              {(["kick", "mute", "unmute"] as const).map((operation) => <span key={operation} data-operation={operation}>
                <code className="users-command">{operation === "kick" ? `+kick ${user.nickname}` : operation === "mute" ? `+mute ${user.nickname}` : `+unmute ${user.nickname}`}</code>
                {" "}{label(decision(user.role, operation))}
              </span>)}
            </td>
          </tr>)}
        </tbody>
      </table></div>
      {directory.users.length === 0 && <p>{t.usersEmpty}</p>}
      <p className="users-links">
        <a href="#kick">{t.usersOpenKick}</a>
        <a href="#mute">{t.usersOpenMute}</a>
        <a href="#bans">{t.usersOpenBans}</a>
      </p>
    </>}
  </section>;
}
