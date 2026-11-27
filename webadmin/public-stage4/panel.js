// Stage IV public client: C++ password proof, then an eight-digit email code.
// No client-provided recipient, WebAuthn fallback, privileged API or browser storage.
const api = "/stage4/v1/";
const $ = selector => document.querySelector(selector);
const message = text => { $("#message").textContent = text; };
let csrf = "", pendingCsrf = "", registering = false;

async function call(path, data, token = "") {
  const post = data !== undefined;
  const response = await fetch(api + path, {
    method: post ? "POST" : "GET", credentials: "same-origin", cache: "no-store",
    headers: post ? { "Content-Type": "application/json", "X-DC24H-Stage4-Csrf": token } : {},
    ...(post ? { body: JSON.stringify(data) } : {}),
  });
  const result = await response.json();
  if (!response.ok) throw Error(result.error === "rate_limited" ?
    "Zbyt wiele prób. Spróbuj później." : "Operacja odrzucona.");
  return result;
}

async function send(event) {
  event.preventDefault();
  const submit = $("#login-form button");
  submit.disabled = true;
  message("Sprawdzanie hasła i wysyłanie kodu…");
  try {
    if (!window.isSecureContext) throw Error("Wymagane HTTPS.");
    const initial = await call("challenge");
    const username = $("#actor").value;
    const password = $("#password").value;
    $("#password").value = "";
    const result = await call("login/send", { username, password }, initial.csrf);
    registering = false;
    pendingCsrf = result.csrf;
    $("#login").hidden = true;
    $("#verification").hidden = false;
    $("#email-code").focus();
    message("Jeśli konto ma zweryfikowany adres e-mail, kod został wysłany.");
  } catch (error) { message(error instanceof Error ? error.message : "Nie można wysłać kodu."); }
  finally { $("#password").value = ""; submit.disabled = false; }
}

async function verify(event) {
  event.preventDefault();
  const submit = $("#code-form button");
  submit.disabled = true;
  try {
    const code = $("#email-code").value;
    $("#email-code").value = "";
    if (registering) {
      await call("register/verify", { code }, pendingCsrf);
      leave();
      message("Adres e-mail potwierdzony. Zaloguj się, aby otrzymać nowy kod.");
    } else {
      const session = await call("login/verify", { code }, pendingCsrf);
      enter(session);
    }
  } catch (error) { message(error instanceof Error ? error.message : "Kod nieprawidłowy lub wygasł."); }
  finally { $("#email-code").value = ""; submit.disabled = false; }
}

function enter(session) {
  csrf = session.csrf;
  pendingCsrf = "";
  $("#login").hidden = true;
  $("#verification").hidden = true;
  $("#dashboard").hidden = false;
  $("#registration").hidden = true;
  $("#refresh").hidden = !["owner", "administrator"].includes(session.role);
  $("#role").textContent = "Rola: " + session.role;
  message("Zalogowano: hasło i kod e-mail potwierdzone.");
}
function leave() {
  csrf = "";
  pendingCsrf = "";
  registering = false;
  $("#registration").hidden = true;
  $("#dashboard").hidden = true;
  $("#verification").hidden = true;
  $("#login").hidden = false;
  $("#result").textContent = "";
  $("#password").value = "";
  $("#email-code").value = "";
}
async function action(fn) {
  try { await fn(); }
  catch (error) { message(error instanceof Error ? error.message : "Operacja odrzucona."); }
}

$("#login-form").addEventListener("submit", send);
$("#code-form").addEventListener("submit", verify);
$("#restart-login").addEventListener("click", () => { leave(); message("Rozpocznij logowanie ponownie."); });
$("#refresh").addEventListener("click", () => action(async () => {
  $("#result").textContent = JSON.stringify((await call("summary")).summary, null, 2);
  message("Stan usługi kont odświeżony.");
}));
$("#revoke-sessions").addEventListener("click", () => action(async () => {
  await call("sessions/revoke", {}, csrf); leave(); message("Sesje unieważnione.");
}));
$("#logout").addEventListener("click", () => action(async () => {
  await call("logout", {}, csrf); leave(); message("Wylogowano.");
}));

$("#start-registration").addEventListener("click", () => {
  $("#login").hidden = true;
  $("#registration").hidden = false;
  $("#new-username").focus();
  message("");
});
$("#cancel-registration").addEventListener("click", () => { leave(); message(""); });
$("#registration-form").addEventListener("submit", event => {
  event.preventDefault();
  const submit = $("#registration-form button");
  submit.disabled = true;
  const password = $("#new-password").value;
  $("#new-password").value = "";
  void action(async () => {
    try {
      const initial = await call("challenge");
      const result = await call("register/send", {
        username: $("#new-username").value, password, email: $("#email").value,
      }, initial.csrf);
      registering = true;
      pendingCsrf = result.csrf;
      $("#registration").hidden = true;
      $("#verification").hidden = false;
      $("#email-code").focus();
      message("Sprawdź pocztę, jeśli można założyć konto z podanymi danymi.");
    } finally { submit.disabled = false; }
  });
});
$("#password-form").addEventListener("submit", event => {
  event.preventDefault();
  const current = $("#current-password").value, password = $("#replacement-password").value;
  $("#current-password").value = "";
  $("#replacement-password").value = "";
  void action(async () => {
    await call("password/change", { current, password }, csrf);
    leave();
    message("Hasło zmienione. Zaloguj się ponownie.");
  });
});
call("session").then(enter).catch(() => leave());
