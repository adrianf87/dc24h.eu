# STA / QUI — alpha-0.2.9

This increment adds typed status/quit validation, protocol diagnostics, status routing and bounded terminal output on the actual C++ ADC/ADCS reactor. It starts from main `da1521502c4a8936b3427a7d38fea6b904686b39`, after the alpha-0.2.8 login merge. Earlier branches/history are preserved. This numbered branch is not merged automatically.

## Status values and validation

`adc/status.hpp` defines StatusCode with independent severity and error number. Severity 0 is success and requires error 00 (`000`); severity 1 is recoverable; severity 2 is fatal. Unknown error numbers in the latter two classes are retained, including 199 and 299. Values such as 001, 300 and 999 are syntactically parseable three-digit tokens but fail semantic validation. The syntax parser remains a syntax boundary; the state machine applies these semantic checks before any change.

`validate_status` and `validate_quit` return borrowed views or fixed StatusError categories. They validate the shared serializer/parser grammar, text/NFC and encoded frame limit, command/kind and parameter shape, duplicate field names and known flag domains. Status descriptions are limited to 2048 decoded UTF-8 bytes, individual STA flag values to 1024 and QUI flag values to 2048; the encoded body still cannot exceed 16384 bytes or 128 parameters. These are explicit local alpha limits, not wire specification limits.

STA flags FC use a four-character uppercase feature/command name, FM/FB a two-character field name, TL a nonnegative signed 64-bit integer, TO/PR nonempty visible ASCII, and I4/I6 a numeric address of the matching family. Flags are validated when present; the module does not infer unavailable transfer/account details or require every command-specific flag for an otherwise understandable remote status. Unknown fields remain bounded metadata and cannot authorize operations.

QUI is hub-originated IQUI only. Its positional SID and optional ID initiator use the common SID codec. TL permits -1 or nonnegative signed 64-bit seconds. MS is user-visible text; DI is a presence flag. RD is parsed as bounded nonempty metadata, without following or interpreting its URL. Builders produce owned Messages and reject invalid local options without truncation. The server currently emits no RD, ID, DI or TL advice: redirects, kicks/bans and reconnect restrictions are not enabled by this stage.

## Hub and routed statuses

HSTA 000 and HSTA 1xx are accepted in every live state without an automatic reply, identity approval or timer renewal beyond the existing transport idle policy. HSTA 2xx closes that peer without echoing its description or generating a reply loop; remaining admitted users receive a bare IQUI. Client descriptions and optional values never enter operator logs.

NORMAL BSTA/DSTA/ESTA/FSTA pass source-SID binding and semantic validation before routing. D sends only to the ready target; E also echoes to the sender, once if both are the same peer; B includes every ready peer including sender. F matches every required/excluded selector against the recipient's INF SU list, not its HSUP capabilities. Pending welcomes and closing/aborted sessions are excluded. A missing/not-ready direct recipient yields recoverable ISTA 140. A routed fatal status concerns the client-client interaction and does not close either hub session. It is not a transfer implementation or permission to execute commands.

Forwarded statuses retain only recognized FC/FM/FB/TL/TO/PR/I4/I6 fields. Unknown fields, including a named PD/PAS-like payload, are discarded. The description and recognized values are client-supplied public metadata; do not interpret them as trusted hub instructions. The original Message is unchanged. Client IQUI/HQUI/BQUI attempts cannot disconnect a victim; the hub rejects the sender. Clients may disconnect normally by closing their socket or issuing a fatal HSTA.

## Fixed local diagnostics

Local fatal responses use fixed English descriptions and the existing INF/SUP/SID status mapping: 221 invalid nickname, 222 nickname in use, 224 CID in use, 227 invalid identity, 243 invalid/missing client information, 245 missing feature, 246 invalid address, 247 no hash overlap, 211 session capacity and 210 internal failure. State/sequence failures now use 244 with the offending FOURCC in FC; other protocol/source/semantic errors use 240. Feature errors identify BASE or the removed TIGR via FC. Peer parameter contents are never interpolated into descriptions.

State-valid unimplemented NORMAL commands, including unknown extensions, return recoverable 125 with FC instead of pretending to execute them. Existing chat/search/transfer handlers remain planned. Invalid syntactic frames continue to be ignored under the existing invalid-frame budget, without one error reply per bad frame. Reaching frame/invalid quotas triggers one fixed fatal protocol resource-limit notice, subject to output availability.

## Terminal sequence and ownership

For a local rejection or live login/idle/partial-frame/quota expiry:

1. Admission closes once; subsequent commands cannot change state/metadata or restart closing.
2. The output queue preserves its first frame, which may already be partially sent or pinned by an SSL WANT retry. Later ordinary frames are cancelled with exact byte accounting.
3. The hub appends fatal ISTA and, when the recipient has an assigned SID, a final own IQUI with fixed MS description. If the preserved front is a pending ISID, that ISID completes before STA/QUI. A peer that has not received and will not receive a SID gets ISTA only.
4. A fully sent own QUI is the final application frame. No welcome completion, extra response or relay can enqueue data behind it. The original session stays reserved while terminal bytes drain; closing frames intentionally do not run the ordinary fatal-STA observer, which would release SID too early.
5. Actual transport cleanup announces one bare IQUI to other admitted peers, removes owned nickname/CID/SID indexes, cancels remaining output and releases the SID lease. Repeated cleanup is harmless; per-recipient FIFO orders old INF, removal and a later replacement.

The existing 256 KiB/client and 16 MiB/hub wire-byte caps apply to terminal output too. If even the notification cannot fit, a pinned front is not rewritten and the affected connection closes immediately. EOF/reset, failed writes, stalled output, incomplete/failed TLS and process shutdown may also cancel notices; delivery cannot be promised on an unusable transport. Shutdown continues to skip presence fan-out and does not wait for peers. TLS close_notify/reload is not added.

## Deadlines, logs and administration

INI `[security] close_timeout=2` accepts 1–3600 seconds, without a new CLI switch. It limits the whole terminal drain from the first observed closing transition and never slides after read/write progress. Input login/partial/idle timers stop initiating new notices once closing starts. Existing output limits still constrain memory; a pending TLS read needing write readiness can finish its dependency before terminal writes, discarding any returned plaintext. Stop and periodic expiry remain checked by the nonblocking reactor.

Normal idle, full-login and partial-frame expiry now attempt protocol notification before closure. Output stalls and unavailable TLS still close immediately. Existing security policies count a connection closure once even when quota rejection and output overflow coincide. New fixed opt-in INFO events `peer_status` and `close_timeout` expose peer fatal reports and exhausted terminal drains; no client status/quit text, nicknames or addresses is logged. Existing operator timeout warnings retain US/PL translations; wire descriptions remain English.

The API v1 schema and WebAdmin remain unchanged. Active connections are transport sockets, including terminal drains, not a ready-user count. Parsed-frame counters do not imply authorization. No SQL migrations, bans/accounts, administrative mutations or password policy are added.

## Verification

`adc.status` checks every three-digit code, field/description boundaries, unknown errors, known/unknown/duplicate fields, QUI options, malformed-state rollback, success without admission, partial terminal frames, pinned-front stability, cancelled output, pending ISID ordering, exactly-once cleanup, B/D/E/F routing and fatal peer reports. `adc.status.process` exercises real mixed TCP/OpenSSL connections for terminal sequences/EOF, forged QUI, malformed STA, public routing, reconnect, privacy and timeout/quota notices. Existing slow-reader and SSL backpressure suites remain in the strict regression gate.

`adc.status.ncdc` and `adcs.status.ncdc` use disposable real ncdc PTYs: the client sees the duplicate-nickname fatal reason, the existing user remains intact, and after that owner leaves ncdc reconnects to NORMAL and receives a recoverable unimplemented-command response. TLS terminates in C++. Complete-login/reconnect and prior SUP/SID/INF/Tiger fixtures remain separate regression tests. The complete strict gate has 40 CTest entries. Arbitrary allocation failures, all TLS WANT_READ dependencies, measured load/fuzz/side-channel behavior and the remaining OS matrix are not established by these tests.

`adc.close.process` uses an executable-local `--wrap=accept4` fixture with tiny server send buffers and client receive windows. It forces pending TCP and SSL writes, initiates idle closure, observes exactly two close_timeout events despite blocked output, and verifies a fresh guest can still log in. No production socket tuning or host sysctl is changed; this fixture requires a GNU-compatible linker.

Protocol reference: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), sections 3.3, 5.1, 5.3.1 and 5.3.12.
