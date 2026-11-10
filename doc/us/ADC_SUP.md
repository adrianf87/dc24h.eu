# SUP — alpha-0.2.4

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

Since alpha-0.2.7, [CID/PID verification](ADC_IDENTITY.md) is a dedicated shared module with TigerCid/VerifiedIdentity values, direct cleansed output buffers and a constant-time final digest comparison. INF snapshots retain only a public verified result. Basic verification introduced in alpha-0.2.6 remains compatible; live transport attachment and account/registry policy remain separate.

Since alpha-0.2.6, [ADC INF](ADC_INF.md) supplies ClientSession, combining this lower-level stack with intrinsic INF validation and Tiger PID/CID verification. It requires validated metadata before external identity admission and never retains/exports PD. Registry uniqueness and actual Network attachment remain planned; older APIs/fixtures retain their original scope.

Since alpha-0.2.5, [ADC SID](ADC_SID.md) supplies a shared hub pool and SidSession composition with checked ISID ownership and automatic reservation cleanup. Parser/state SID syntax uses the same codec; AAAA remains valid syntax but is reserved by the pool as local policy. The original lower-level APIs and historical fixtures remain available. Network still uses only the parser.

## Delivered scope

`dc24h/adc/sup.hpp` adds `SupportNegotiation` and `ProtocolConnection` to `dc24h_adc`. The first maintains feature advertisements and selects a session hash. The second composes it with [ADC State](ADC_STATE.md), replacing manual protocol approval with a checked SUP exchange. No socket, logging, database, configuration-file or cryptographic dependency is added.

References: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), sections 3.2, 3.4, 5.1, 5.3.2 and 6.1; [ADC Extensions, TIGR](https://adc.sourceforge.io/ADC-EXT.html). The local supported hash registry currently contains only TIGR. The selected algorithm is Tiger with a 24-byte digest; this module selects and describes it but does not compute digests, trees, CIDs or password responses. TIGR selection is unrelated to OpenSSL's TLS ciphers, certificate hashes and keyprints.

The default local profile is BASE/TIGR. Additional features in `SupportPolicy::features` are caller declarations and require corresponding implementations before real deployment. The standalone module can be tested with other profiles without claiming those extension implementations. Production Network still only parses input; it does not yet attach ProtocolConnection or send these replies. Full hub login remains planned.

## Feature semantics and limits

An initial HSUP starts from an empty peer set. AD adds a feature, RM removes it. Tokens are applied in wire order; adding an existing feature/removing an absent feature is an idempotent operation. The final operation for a name determines membership. Repeated initial SUP is rejected after successful negotiation; updates belong to NORMAL only through ProtocolConnection.

After applying the initial frame, BASE must be present and at least one locally supported hash must overlap. Currently that means TIGR. An unknown peer feature remains stored but never becomes a hash candidate or local capability automatically. Local and peer advertisements are kept separately; the response is not their intersection and does not echo the peer list. SUP capabilities do not replace the INF SU field used for feature routing.

Local application bounds, independent of protocol maxima:

- At most 128 declared local policy tokens, before duplicate collapse; BASE and TIGR are mandatory. Invalid local names/missing requirements/oversize throw fixed-text `invalid_argument` during construction.
- At most 128 SUP tokens per input, as in the parser; every token is rechecked for AD/RM and four-character uppercase/alphanumeric feature grammar, including hand-built Message inputs.
- At most 128 distinct peer features across the whole session, not per frame. Names are stored as an owning sorted vector with no unbounded extension map. Lookup never allocates.
- The cumulative bound is checked before each insertion. At capacity, remove an old optional feature before adding another. Add-then-remove exceeding the bound is rejected even if the final set would fit. Duplicates do not consume capacity.

Updates use a bounded candidate copy. Failed syntax, capacity, missing BASE or removal of the selected hash leaves all support and connection state unchanged. A frame that removes then restores BASE/TIGR is accepted if its final set contains both and every intermediate insertion fits. Unknown optional features may be added/removed. No update renegotiates the session hash. Local policy is immutable; outbound SUP updates in NORMAL return local_update_unsupported rather than silently changing advertised capabilities.

## Reply and errors

Before success, `selected_hash()`, `initial_reply()` and `initial_reply_line()` return empty optionals. After success the response is an owning Message (I/SUP, no routing), with positional_count set. Its order is ADBASE, the chosen hash ADTIGR, then other explicitly local features in lexical order. This places the chosen hash before any other registered hash; adding algorithms requires extending the explicit registry and selection/reply tests. No order or naming heuristic treats unknown features as hashes.

The minimal wire reply is `ISUP ADBASE ADTIGR` followed by LF. `initial_reply_line()` is a bounded SUP-only encoder of controlled tokens, not a general serializer or network write. With 128 policy entries the body is at most 900 bytes. Reply values own storage; callers may destroy or modify returned copies without affecting negotiation.

`SupportError` has fixed categories: invalid_message, feature_limit, missing_base, no_common_hash, session_hash_removed, invalid_sequence, reply_mismatch and local_update_unsupported. No peer tokens or secret fields are included in an error. `support_status_code()` supplies suggested fatal codes for future dispatch: 245 for missing BASE/removed session hash, 247 for no initial hash overlap, 240 otherwise. It does not send STA, choose an error description or close a socket. The owner must apply a bounded error/close policy; unchanged-state rejection is not permission for endless retries.

`SupportNegotiation::accept_initial` and `update_peer` return optional SupportError. This lower-level metadata class does not know connection phase; callers using it directly own sequencing. Constructor errors are exceptions, not SupportError values. Allocation exceptions propagate without a partial support commit. Views/references from local_features/peer_features are borrowed and must not survive mutation or destruction. Separate instances share no state.

## Composed state contract

Use one noncopyable/nonmovable ProtocolConnection per future connection on its owning reactor thread. `observe(Direction, Message)` returns an optional `variant<StateError, SupportError>`; no error means accepted. It exposes state/SID and const support metadata, plus keepalive, explicit identity/password approval and terminal disconnect. It provides no manual protocol approval or mutable state/support access.

1. On the first inbound HSUP, prepare a candidate negotiation. Only after success does the owned machine accept HSUP and approve protocol. Commit the candidate without throwing. The phase remains PROTOCOL.
2. The initial outbound ISUP must exactly match the generated advertisement/type/routing. A truncated, reordered, peer-echoed or unsupported reply is rejected. Observe it only after the complete ordered send.
3. A completed ISID moves the state to IDENTIFY. Parsing BINF alone does not approve identity. Registered-user VERIFY/password gates remain unchanged.
4. In NORMAL, incoming HSUP updates are prepared and checked before state/support commit. IDENTIFY/VERIFY reject them. Source/type/command gates remain active for other commands. DISCONNECTED rejects everything and cannot be reused.

The adapter guarantees unchanged state/support on returned errors. Potentially allocating candidate/reply construction occurs before mutating the owned machine. It has no general outbound preflight interface, output queue, completion callback, deadlines, unique SID allocator, Tiger computation, identity validation, password service or dispatcher. These remain required before attaching it to Network. Keepalive never authorizes login. No API HTTP schema or counters change; current counters still measure parsed syntax.

## Tests and ncdc boundary

`adc.sup` checks required BASE/common hash, unknown/duplicate/ordered features, policy and peer limits, rollback, fixed hash, deterministic replies, ownership, wrong type/hand-built input, state approval, exact ISUP matching, both guest/password continuations, source binding and terminal events. Stream tests cover every two-chunk split of a pipelined SUP/replay transcript.

`adc.sup.ncdc` launches real ncdc with disposable PTY/configuration and no shares. Python owns loopback TCP/TLS sockets and feeds the client's HSUP into the C++ module driver. After sending module-generated ISUP and fixed fixture SID, it reports send completion and passes the actual ncdc BINF back to the adapter. The C++ fixture checks Tiger-sized ID/PD field shapes and remains in unapproved IDENTIFY; it neither validates those identities nor logs them. The fixture is deliberately not the server reactor or a production SID allocator. Its TLS is Python-owned, not C++ ADCS.

Existing `adc.ncdc`/`adcs.ncdc` separately exercise the actual C++ receive-only reactor and OpenSSL TLS. Together these checks establish module and transport boundaries, not complete live hub login. See [TESTING.md](TESTING.md).
