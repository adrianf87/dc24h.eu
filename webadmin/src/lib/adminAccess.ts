let grant = "";
let session = "";
let csrf = "";

export function setAdminAccess(value: {grant: string; session: string; csrf: string}) {
  const nextGrant = /^[0-9a-f]{64}$/.test(value.grant) ? value.grant : "";
  const nextSession = /^[0-9a-f]{32}$/.test(value.session) ? value.session : "";
  const nextCsrf = /^[0-9a-f]{64}$/.test(value.csrf) && value.csrf !== nextGrant ? value.csrf : "";
  if (!nextGrant || !nextSession || !nextCsrf) {
    grant = "";
    session = "";
    csrf = "";
    return;
  }
  grant = nextGrant;
  session = nextSession;
  csrf = nextCsrf;
}

export function clearAdminGrant() {
  grant = "";
  session = "";
  csrf = "";
}

export function adminHeaders(write = false): Record<string, string> {
  if (!grant) return {};
  const headers: Record<string, string> = {"X-DC24H-Admin-Grant": grant};
  if (write) {
    headers["X-DC24H-Session"] = session;
    headers["X-DC24H-Csrf"] = csrf;
  }
  return headers;
}
