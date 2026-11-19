let grant = "";

export function setAdminGrant(value: string) {
  grant = /^[0-9a-f]{64}$/.test(value) ? value : "";
}

export function clearAdminGrant() {
  grant = "";
}

export function adminHeaders(): Record<string, string> {
  return grant ? {"X-DC24H-Admin-Grant": grant} : {};
}
