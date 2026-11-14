# Connection Security — alpha-0.6.8

**Current increment — alpha-0.6.9:** [Error Handling](ERROR_HANDLING.md) consolidates connection diagnostics and immutable shared STA routing with fixed ready-session audiences. Status preparation, recipient queues, terminal diagnostics and completed STA validation isolate memory faults; routed peer fatal status retains hub sessions. Native, mixed ADC/ADCS and ncdc reset/error/recovery cases cover the behavior.

**alpha-0.6.8 local validation (2026-11-14):** All 159/159 CTest tests passed in Debug, Release and ASan/UBSan, including real ncdc passive/active policy refusals and earlier successful plaintext/TLS peer handshakes. Final builds were warning-free after the statistics allocator helper repair; its native test passed again in all three modes. WebAdmin build/typecheck, 12 Node tests, 10 Chromium tests and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. GitHub Actions completion is not awaited; the full supported OS matrix and production load remain unverified.

Based directly on main `236b58877a317c19bcb1574c2b4545d3091c44fe` after Transfer Tokens. This increment adds explicit connection permissions, trusted lifetime grants and authorization rechecks, and centralizes/tests endpoint and port validation. The numbered branch is published separately from main with code/history preserved.

## IP, port and SID boundaries

The existing ADC INF adapter binds numeric I4/I6 declarations to the kernel peer address at login and on updates. I40.0.0.0 / I6:: become that peer's normalized address. A foreign address or different family is refused; transfer permission grants do not bypass this check. No DNS, address override, NAT probing or hub-side peer socket is introduced. The [ADC specification](https://adc.sourceforge.io/ADC.html), sections 5.3.4 and 5.3.8–5.3.9, describes IP ownership checks and active/passive coordination.

`users::usable_address(AddressBytes, AddressFamily)` now holds the allocation-free binary listener rule previously embedded in `public_address`. Unspecified, multicast, limited IPv4 broadcast, IPv4-mapped IPv6 and scope-dependent IPv6 link-local addresses have no listener. IPv4 uses four address bytes and twelve zero padding bytes. Private/LAN and loopback addresses remain valid. Unknown families are refused. This is a structural listener check, not a universal global-routability/egress policy or reachability proof; directed subnet broadcasts cannot be classified without a network prefix.

`public_address` reuses that rule without changing accepted managed profiles or Search behavior. `ClientCapabilities::listener` also applies it, so even a hand-built native snapshot cannot introduce an unspecified or noncanonical padded listener.

`transfers::valid_connection_port(int64_t)` is shared by native Request and ADC CTM validation. Only 1..65535 is a peer listener port. The parser rejects malformed or signed-64-bit-overflow syntax; the semantic codec rejects zero, negative and oversized values. Valid numeric spelling such as 00001 remains unchanged on forwarding. This rule does not change hub listener port 0 (ephemeral) or UDP port 0 (disabled). RCM still carries no port.

The ADC adapter requires syntactically valid four-character base32 SIDs, verifies source ownership/state and resolves the exact ready target lifetime. Missing/closing/welcome-pending targets, self-connections and stale prepared lifetimes remain rejected. A token, public CT/SU/AP field or claimed operator label never chooses a target or supplies authority. The protocol-neutral Core retains its existing bounded generic session-key contract; ADC-specific SID syntax stays in the adapter.

## Permission model

`transfers/security.hpp` defines `Permissions { initiate, receive }`, `SecuritySettings { ctm_enabled, rcm_enabled, guests }`, typed `PermissionError` and allocation-free `authorize`.

| Gate | Meaning |
| --- | --- |
| `ctm_enabled` | Global admission of CTM, including CTM responses to RCM |
| `rcm_enabled` | Global admission of RCM |
| Sender `initiate` | May send this connection-coordination request |
| Target `receive` | May receive this connection-coordination request |

All switches default to true, preserving earlier guest behavior. Disabled commands cannot be enabled by a per-session grant. A forbidden sender or recipient prevents both original delivery and initial E echo; only the source receives a generic recoverable ISTA 125 with the offending command in FC. No protocol, port, token, SID or IP is included in that denial text. Existing source/state/wire/endpoint validation keeps precedence, so malformed or forged requests are not authorized as a side effect of policy checks.

A grant applies to one request direction, not to socket client/server roles: a passive RCM sender needs initiate; the active target needs receive. Its subsequent CTM response is a new request and needs its own sender initiation and target receipt permissions. The sender's receive bit does not restrict an echo of its own accepted request.

## Configuration and trusted grants

```ini
[transfers]
ctm_enabled = on
rcm_enabled = on
guest_initiate = on
guest_receive = on
```

Only literal lower-case on/off is accepted. Unknown keys/sections, duplicate keys and invalid values fail configuration checks. The settings flow from Configuration through Network Options and LoginLimits to Transfer Core. They are startup policy: restart to change them. There is no new CLI switch, HTTP endpoint or WebAdmin control. The read-only status schema does not add transfer settings/counters in this stage.

`LoginHub::set_transfer_permissions(lifetime_id, Permissions)` is a trusted owner-thread API, available only for an existing ready connection. It returns false for missing, negotiating, closing or departed IDs. Grants live in that Peer, not in public INF, a CID/SID string or a global map; disconnect destroys them, and a replacement connection receives configured guest defaults. Registered-account roles, persistent grants, browser administration and SQL permission management remain future work.

Native `Core(sessions, settings, authority)` accepts a borrowed `PermissionSource` for trusted integration. It must outlive Core and supply read-only, allocation-free, noexcept lookups for exact ready lifetimes. A missing authority entry fails closed. A null authority explicitly uses configured guest permissions after readiness checks, keeping existing native callers source-compatible. Core and LoginHub remain nonmovable; dependent C++ components must rebuild, with no stable binary ABI promised.

## Dispatch, revocation and scope

Transfer Core checks both source initiation and target receipt at preparation and again immediately before the target sink. If a grant is revoked between those points, dispatch consumes/cancels that prepared attempt and invokes no sink. Restoring a grant permits a new request, not replay of a copied old plan. Listener address/activity and session lifetime checks still apply; the plan cannot silently migrate to another address family or user.

For E, target delivery is attempted first. Source readiness/ownership, endpoint role, global command policy and current source initiation are independently checked before echo. Revocation of source initiation in the target callback cancels echo. A later target permission change does not retract the target's already accepted queue insertion or cancel an otherwise eligible independent source echo. Permission revocation does not recall frames already queued or close established peer sockets.

Authorization adds no allocation to dispatch. Each plan retains its existing domain and one-attempt state; queue backpressure and per-recipient allocation isolation remain. Permissions govern CTM/RCM coordination. Existing peer STA error routing, Search permissions, authentication, firewall/egress policy, IP allowlists, transfer rate quotas, TLS trust/pinning, NAT traversal, token correlation and file bytes are outside this increment. Existing global frame/admission/output limits continue to apply. Payloads and permission grants are not added to logs, SQL, API/WebAdmin or Chat/Search counters.

## Verification

`transfers.security` covers LAN/loopback and invalid binary addresses/padding/families, signed port boundaries, malformed SID syntax, all sender/recipient permission combinations, global disable, missing authority, null-authority guest defaults, revocation before dispatch, one-attempt refusal after grant restoration, callback-time echo revocation, independent echo, trusted managed grants, untrusted public metadata, readiness and disconnect/replacement defaults. Authorization and revoked dispatch are exercised with C++ allocations disabled.

`transfers.security.process` covers IPv4/IPv6 mixed ADC/ADCS users, all four INI switches, both commands/scopes, source-only denials, public metadata claims, valid leading-zero and invalid/overflow ports, unavailable/self targets, foreign INF addresses, forged source SIDs, recovery and API/log privacy. Signed overflow remains a parser-level drop; semantic port failures return a recoverable generic error. Strict on/off and duplicate key rejection are checked against the actual executable.

Four real ncdc scenarios (`adc.connectionsecurity.ncdc`, `adc.connectionsecurity.active.ncdc`, `adcs.connectionsecurity.ncdc`, `adcs.connectionsecurity.active.ncdc`) test passive RCM and active CTM denied by guest policy over both hub transports. The real client displays the recoverable denial, no request reaches the observer, and chat/shutdown remain usable. Earlier successful plaintext and certificate-checked TLS peer handshakes remain enabled.

See [Configuration](CONFIGURATION.md), [Testing](TESTING.md), [Transfer Core](TRANSFER_CORE.md), [Connection Routing](CONNECTION_ROUTING.md), [Client Capabilities](CLIENT_CAPABILITIES.md) and [Transfer Tokens](TRANSFER_TOKENS.md).
