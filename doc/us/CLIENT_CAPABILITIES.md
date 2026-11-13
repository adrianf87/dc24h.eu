# Client Capabilities — alpha-0.6.6

**Current increment — alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) adds startup CTM/RCM and guest permissions, trusted ready-lifetime grants and preparation/dispatch/echo authorization rechecks. Shared binary IP and port validation preserve managed-profile behavior. Native, mixed ADC/ADCS and real ncdc cases cover denials, spoofing and recovery.

**Increment delivered in alpha-0.6.7:** [Transfer Tokens](TRANSFER_TOKENS.md) unifies bounded opaque token validation and owning values across Core, CTM/RCM and peer errors. Token bytes, including Unicode/edge spaces/backslashes, survive immutable routing and real ncdc boundary handshakes. Independent requests may reuse tokens; no token registry or inferred correlation is added.

**alpha-0.6.6 local validation (2026-11-13):** All 147/147 CTest tests passed in Debug, Release and ASan/UBSan, including real ncdc mode changes and plaintext/TLS peer handshakes. WebAdmin build/typecheck, 12 Node tests, 10 Chromium tests and real C++ API integration passed; no compiler warnings. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. GitHub Actions completion is not awaited. Full supported OS coverage and production load remain unverified.

Based directly on main `1aacfbd` after alpha-0.6.5. This increment unifies public TCP capabilities, per-family connection roles and originator path selection. Existing code and history are preserved; the numbered branch is published separately from main.

## Public declarations and snapshots

`transfers/capabilities.hpp` exposes a fixed-size, owning `ClientCapabilities` snapshot with optional IPv4/IPv6 listener address bytes and advisory `Protocols`. `client_capabilities(Profile)` projects the validated public INF profile without allocations, I/O, DNS, probing or retaining a profile reference. `Core::capabilities(SessionId)` returns a snapshot only for a ready session with a public user; missing, closing and welcome-pending sessions have no snapshot.

An active TCP family requires both its exact public SU feature (`TCP4` or `TCP6`) and a usable numeric address. Absent/unspecified, multicast, IPv4 broadcast, mapped IPv6 and scope-dependent link-local IPv6 addresses do not provide a listener. Private and loopback addresses are permitted for local networks/tests. The snapshot is a declaration, not proof of reachability or ownership. The ADC INF adapter separately checks address ownership and substitutes the connected peer address for I40.0.0.0 / I6::.

`mode(family)` returns `active` when a listener exists, otherwise conservative `passive`. A client may be active in one family and passive in the other. Invalid native family values have no listener. UDP features/ports, legacy `ADC0`, SUP and hub TLS do not grant TCP activity. Exact public SU `ADCS` advertises advisory peer TLS support independently of TCP mode; protocol choice and required TLS retain the alpha-0.6.5 rules.

## Coordination rules

For a specific family:

| Origin | Peer | CTM | RCM |
| --- | --- | --- | --- |
| Active | Active | Allowed | Refused |
| Active | Passive | Allowed | Refused |
| Passive | Active | Refused | Allowed |
| Passive | Passive | Refused | Refused |

`connection_address(origin, peer, kind, family)` implements this rule once. The returned fixed-size bytes belong to the CTM origin or RCM target listener. Invalid kind/family yields no path.

`select_connection(origin, peer, kind)` preserves an explicit command and selects its first eligible family, IPv4 then IPv6. `select_connection(origin, peer)` is a separate helper for trusted originators: prefer CTM on either origin-active family, then RCM on an origin-passive/peer-active family; return no choice for passive/passive. It never generates a token, chooses a port, changes a protocol, queues a frame or connects a socket. Callers may separately use `negotiate_protocol` with both snapshots and an explicit preference; required ADCS cannot silently fall back.

The running Connection Router uses the explicit-command helper; Transfer Core uses the same address rule at preparation and dispatch. CTM keeps source activity/error precedence, and RCM resolves the exact ready target before checking roles. Incoming CTM is never rewritten to RCM. CTM/RCM have no address-family field: selected family/address are admission metadata, not a claim that the peer has a matching outbound network route. Clients choose their sockets and report their own failures. No absent target address is treated as a new CTM permission rule.

## Live INF changes and lifetime safety

Accepted INF SU and I4/I6 changes update the managed profile transactionally. New snapshots and requests use the current state; retained snapshots own their bytes. Removing a selected listener, changing its address or changing an RCM origin to active cancels a stale plan before any sink call. Another available family cannot rescue that old plan. A new request may select the remaining family. Unrelated-family, UDP or advisory TLS changes do not invalidate a still-eligible selected path or rewrite explicit peer protocol intent.

Existing one-attempt dispatch, source ownership, ready-session lifetime binding, source echo rechecks and fixed D/E audiences remain. D reaches only the selected target; E also independently echoes to its source. Payloads/tokens never enter SQL, API/WebAdmin or Chat/Search counters, and no capability registry or transfer history is added. Pending token correlation, NAT traversal, passive/passive relay, peer sockets and file bytes remain future work.

## Verification

`transfers.capabilities` checks all 32×32 combinations of TCP4/TCP6/UDP4/UDP6/ADCS declarations, per-family CTM/RCM roles, originator ordering, native invalid enums, unusable addresses, owning snapshots, allocation-free selection/dispatch, readiness and live stale/fresh plans.

`transfers.capabilities.process` covers both address families, mixed ADC/ADCS hub users, both directions/scopes/commands, fragmented headers, all role pairs, address removal/restoration, exact SU cancellation, wrong-family declarations, bystander isolation, recovery and API/log privacy.

Four real ncdc capability scenarios use both hub transports and both plaintext/TLS peer handshakes. A real ncdc changes passive→active→passive→active without reconnecting; passive/passive RCM gets a private recoverable refusal, restored activity permits RCM→CTM with the original token/protocol and a real CSUP/CINF peer handshake. Local TLS tests trust disposable certificates explicitly. This is not production certificate pinning.

See [Testing](TESTING.md), [Protocol Negotiation](PROTOCOL_NEGOTIATION.md), [Connection Routing](CONNECTION_ROUTING.md), [ADC specification](https://adc.sourceforge.io/ADC.html) sections 5.2.2 and 5.3.8–5.3.9, and [ADC extensions](https://adc.sourceforge.io/ADC-EXT.html).
