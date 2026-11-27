// The only public browser entrypoint. No private gateway, C++ tokens, dynamic
// HTML injection, persisted browser credentials or third-party auth scripts.
const api = "/stage4/v1/";
const $ = selector => document.querySelector(selector);
const message = text => { $("#message").textContent = text; };
const encode = bytes => btoa(String.fromCharCode(...new Uint8Array(bytes)))
  .replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
const decode = value => Uint8Array.from(atob(value.replace(/-/g, "+").replace(/_/g, "/")
  .padEnd(Math.ceil(value.length / 4) * 4, "=")), char => char.charCodeAt(0));
let csrf = "";

async function call(path, data, token = "") {
  const post = data !== undefined;
  const res = await fetch(api + path, {
    method: post ? "POST" : "GET", credentials: "same-origin", cache: "no-store",
    headers: post ? { "Content-Type": "application/json", "X-DC24H-Stage4-Csrf": token } : {},
    ...(post ? { body: JSON.stringify(data) } : {}),
  });
  const body = await res.json();
  if (!res.ok) throw Error(body.error === "rate_limited" ? "Zbyt wiele prób. Spróbuj później." : "Operacja odrzucona.");
  return body;
}

function getOptions(json) {
  if (typeof PublicKeyCredential.parseRequestOptionsFromJSON === "function")
    return PublicKeyCredential.parseRequestOptionsFromJSON(json);
  return { ...json, challenge: decode(json.challenge),
    allowCredentials: (json.allowCredentials ?? []).map(c => ({ ...c, id: decode(c.id) })) };
}
function createOptions(json) {
  if (typeof PublicKeyCredential.parseCreationOptionsFromJSON === "function")
    return PublicKeyCredential.parseCreationOptionsFromJSON(json);
  return { ...json, challenge: decode(json.challenge),
    user: { ...json.user, id: decode(json.user.id) },
    excludeCredentials: (json.excludeCredentials ?? []).map(c => ({ ...c, id: decode(c.id) })) };
}
function serialize(credential) {
  if (typeof credential.toJSON === "function") return credential.toJSON();
  const response = credential.response;
  const registration = response.attestationObject !== undefined;
  return {
    id: credential.id, rawId: encode(credential.rawId), type: "public-key",
    response: registration ? {
      clientDataJSON: encode(response.clientDataJSON), attestationObject: encode(response.attestationObject),
      transports: response.getTransports?.() ?? [],
    } : {
      clientDataJSON: encode(response.clientDataJSON), authenticatorData: encode(response.authenticatorData),
      signature: encode(response.signature), userHandle: response.userHandle ? encode(response.userHandle) : null,
    },
    clientExtensionResults: credential.getClientExtensionResults(),
    authenticatorAttachment: credential.authenticatorAttachment ?? undefined,
  };
}

async function login(event) {
  event.preventDefault();
  const submit = $("#login-form button");
  submit.disabled = true;
  message("Potwierdź oba składniki logowania…");
  try {
    if (!window.isSecureContext || !navigator.credentials) throw Error("Wymagana bezpieczna przeglądarka z WebAuthn.");
    const initial = await call("challenge");
    const actor_id = $("#actor").value;
    const password = $("#password").value;
    $("#password").value = "";
    const pending = await call("login/options", { actor_id, password }, initial.csrf);
    const credential = await navigator.credentials.get({ publicKey: getOptions(pending.options) });
    if (!credential) throw Error("Nie potwierdzono passkey.");
    const session = await call("login/finish", { response: serialize(credential) }, pending.csrf);
    enter(session);
  } catch (error) { message(error instanceof Error ? error.message : "Logowanie nie powiodło się."); }
  finally { $("#password").value = ""; submit.disabled = false; }
}
function enter(session) {
  csrf = session.csrf;
  $("#login").hidden = true;
  $("#dashboard").hidden = false;
  $("#role").textContent = "Rola: " + session.role;
  message("Zalogowano dwuskładnikowo.");
}
function leave() {
  csrf = "";
  $("#dashboard").hidden = true;
  $("#login").hidden = false;
  $("#result").textContent = "";
}
async function action(fn) {
  try { await fn(); }
  catch (e) { message(e instanceof Error ? e.message : "Operacja odrzucona."); }
}

$("#login-form").addEventListener("submit", login);
$("#refresh").addEventListener("click", () => action(async () => {
  $("#result").textContent = JSON.stringify((await call("summary")).summary, null, 2);
  message("Stan huba odświeżony.");
}));
$("#devices").addEventListener("click", () => action(async () => {
  const { devices } = await call("devices");
  $("#result").textContent = devices.map(d => `${d.name} (${d.id})`).join("\n") || "Brak urządzeń.";
}));
$("#enroll").addEventListener("click", () => action(async () => {
  if (!window.isSecureContext || !navigator.credentials) throw Error("Wymagana przeglądarka z WebAuthn.");
  const { options } = await call("devices/enroll/options", {}, csrf);
  const credential = await navigator.credentials.create({ publicKey: createOptions(options) });
  if (!credential) throw Error("Nie potwierdzono urządzenia.");
  await call("devices/enroll/finish", { response: serialize(credential) }, csrf);
  leave(); message("Passkey zapisany. Zaloguj się ponownie.");
}));
$("#revoke-sessions").addEventListener("click", () => action(async () => {
  await call("sessions/revoke", {}, csrf); leave(); message("Sesje unieważnione.");
}));
$("#logout").addEventListener("click", () => action(async () => {
  await call("logout", {}, csrf); leave(); message("Wylogowano.");
}));

call("session").then(enter).catch(() => leave());
