# beta-0.9.x — Stage I working evidence (0.9.1–0.9.4)

Status: **implementation begun; not accepted or release-qualified**. Parent
tracking: [issue #242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242).
All paths below refer to the development branch `beta-0.9.x-stage-full`, not
protected `main`. Do not merge on the strength of an unreviewed green check.

## 0.9.1 — Additional independent clients: honest feasibility

- The only **executed, package-pinned external** ADC/ADCS client in the hosted
  Ubuntu fixture remains **ncdc**. `beta07_client_evidence.py` checks the exact
  Ubuntu image, authenticated APT `.deb` SHA-256, installed version,
  `dpkg --verify`, real executable SHA-256, Git checkout SHA and CTest JUnit
  outcomes for password, TTH, SUP/INF/identity and transfers.
- **DC++**: no native Linux executable fixture. **AirDC++**: no pinned build or
  executable + process test in this repository. Both are **not executed**.
- **EiskaltDC++ daemon**: a candidate packaged for supported Ubuntu, *not* a
  proven interoperability result. A package listing or a Python peer socket
  is never substituted for a launched independent client. To promote it,
  acquire Owner review for a pinned binary/license and exact Ubuntu package
  versions, authenticate the APT-index `.deb` digests and installed binaries,
  implement real process connect/auth/search/transfer regressions, and require
  completed JUnit cases. Existing ncdc success cannot prove its behavior.
- The new exact-head report explicitly contains `stage1_complete: false` and
  `result: partial_evidence_only`, even when all wired process tests pass.

## 0.9.2 — Revocation consistency, restart and architecture decision

Current model is an immutable C++ account snapshot at startup. Committed SQL
status/revision changes do **not** hot-revoke a running session. The disposable
integration test now checks that a transaction updating `status` and
`revision`, then **ROLLBACK**, leaves the actual seeded account active; a
subsequent *committed* disable requires a controlled hub stop, disconnect of
both TCP and TLS old sessions and a restart. New ADC and ADCS account logins
are refused while disabled; an operator re-enables the account with a new
transaction and another controlled restart before verifying fresh GPA/PAS.

For the Owner's separate architecture decision, a possible future controlled
refresh would have these boundaries:

1. The **sole writer** commits a monotonic account policy generation with
   account statuses, roles and grants in a transaction; incomplete or
   rolled-back transactions never yield a visible generation.
2. A bounded owner-only control operation (not network input and not a DB
   query on each reactor frame) loads and validates a **complete** candidate
   snapshot; schema/version or partial read failure keeps refresh disabled
   and surfaces a private refusal.
3. Under an explicit policy barrier, the reactor first invalidates sessions
   that no longer satisfy status/roles/grants, drains old queued sends with a
   bounded deadline, then atomically publishes the *entire* new snapshot.
4. Restart, failed refresh, concurrent account writes, stale generations and
   rollback need separate deny-by-default tests and an operational rollback
   plan before any code path can enable this feature.

**No live reload is implemented or authorized in Stage I.** Today the safe
operator path remains: stop writers / pause logins → commit and inspect the
specific account revision → stop the hub and drain sessions → restart the hub
on the committed DB snapshot → verify refused/re-enabled authentication from
fresh ADC and ADCS connections. A failed restart is a stop/no-go, never a
fallback to the previous account grant generation.

## 0.9.3 — Bounded transfer/NAT edges

The existing process matrix spans IPv4/IPv6 and ADC/ADCS CTM/RCM, live
active/passive flags, TTH SCH/RES, owner-token fencing, malformed frames,
disconnected SID, and post-fault recovery. The added negative advertises an
opposite-family `TCP4`/`TCP6` capability **without an address of that family**;
RCM must be refused, without leaking a request to the target. Restoring the
original feature then permits the valid existing paths. The test checks
aggregate admission/refusal accounting and redacted log/status evidence.

This is **synthetic loopback**, not a measured NAT traversal guarantee:
CGNAT, symmetric NAT, multi-host firewall/port-forward and public IPv6 peers
remain unexecuted. No compatibility fallback may bypass source/session and
recipient checks, active-state requirements, token scope or rate budgets.

## 0.9.4 — ADCS trust and certificate lifecycle

The new OpenSSL test builds a disposable CA-signed, SAN-limited leaf and
verifies, on **TLS 1.2 and TLS 1.3**, real accepted `localhost` handshakes,
wrong-DNS and IP-as-host rejection, and refusal of a *different* CA with the
same subject name. Existing process tests additionally exercise cert cutover,
old CA refusal after restart, live SSL context not silently reloaded, wrong
key, expired leaf, AEAD policy and lack of plaintext ADC downgrade.

SNI is sent through the client's expected hostname and verified against the
leaf certificate. **No server-side SNI allowlist is asserted**: a private ADC
client must not disable certificate/hostname validation or silently change
trust roots on reconnect. No private key, password, raw peer transcript or
customer data is uploaded with CI evidence.

## Evidence and next authorization boundary

`build.yml` writes `beta09-stage1-interoperability.json` only after complete
CTest and the strict ncdc/beta08 evidence checks. It refuses missing, failed,
skipped, duplicate or wrong-head cases; the output is **aggregate, synthetic**
and **incomplete for second-client qualification**. Only GitHub-hosted Ubuntu
22.04, 24.04 and 26.04 are required. CI triggered after a Stage I push is
**pending**, not a passed result; do not poll it during this work unit.

Stage I Fix Pass 1, additional independent executable client qualification,
live account policy refresh, a main merge and production operations all need
their **own explicit Owner request**.
