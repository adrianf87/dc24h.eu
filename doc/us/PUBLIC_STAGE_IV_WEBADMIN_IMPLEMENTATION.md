# Stage IV public WebAdmin implementation

The application uses the native C++ account and password-verifier tables.
Versioned Stage IV migrations add email bindings, HMAC-protected challenges,
attempt counters, sessions, registration reservations and audit records.
Registration creates an active `registered` account only after mailbox
verification. The first successful password and email proof activates the
reserved Administrator. Every later login requires a fresh email code.
Role, credential revision and status are rechecked against authoritative
rows on authenticated requests.

See the [deployment and recovery runbook](STAGE_IV_DEDICATED_SERVICE.md)
for private configuration, the separate test and production databases, and
the Debian 13 installation and acceptance procedure.
