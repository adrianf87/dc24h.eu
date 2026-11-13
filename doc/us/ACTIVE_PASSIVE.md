# Active/Passive — alpha-0.5.10

**Current increment — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) gives Transfer Core owning protocol values and explicit ADC/1.0 plaintext / ADCS/1.0 TLS intent. Pure preference/capability selection never downgrades required TLS. Live CTM/RCM preserve the selected name and extension compatibility; peer failure replies retain opaque tokens. Real ncdc tests cover certificate-checked TLS peer handshakes over both hub transports, alongside the terminal-backpressure regression.

**Increment delivered in alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) consolidates D/E CTM and RCM admission and delivery into one SID router, one immutable packet type and one bounded queue path. Recipients are fixed ready-session lifetimes: D targets only, E targets then independently echoes to the source. Tokens never select recipients; existing roles, wire validation, errors and privacy remain. Mixed-pair tests use the same token without crossing audiences.

**Increment delivered in alpha-0.6.3:** [ADC RCM](ADC_RCM.md) adds owning D/E reverse-request validation and live target/echo routing through Transfer Core, with per-family passive-source/active-target checks, stable lifetime/address admission and shared bounded queues. CTM/RCM share wire-field validation. Real ncdc covers RCM→CTM and peer handshakes in both roles; no pending-token registry or file-data handling is claimed.

**Increment delivered in alpha-0.6.2:** [ADC CTM](ADC_CTM.md) adds owning D/E CTM validation and live target/echo routing over Transfer Core, with checked source TCP activity, lifetime/endpoint rechecks, shared bounded queues and real ncdc client-handshake interoperability. Protocol/port/token/extensions retain their checked spelling and privacy. RCM and file-data handling remain later stages.

**Increment delivered in alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) adds an independent C++20 connection-coordination engine: bounded owning intents, active/passive IPv4/IPv6 roles and immutable session-bound one-attempt plans with cancellation and dispatch rechecks. Shared Users address classification preserves Search behavior. Live CTM/RCM routing and file data transfer remain future work.

**Increment delivered in alpha-0.5.13:** [WebAdmin Search Monitoring](WEBADMIN_SEARCH.md) adds browser-local query/result/refusal/queue rates, a bounded traffic chart and accessible recent-interval table. First samples, gaps, detected resets and failures start fresh baselines; pause/manual refresh and local clear preserve cumulative counters. Existing API/settings and credential isolation remain.

**Increment delivered in alpha-0.5.12:** [Search Configuration and Statistics](SEARCH_CONFIGURATION.md) exposes 19 cumulative hub SCH/RES counters, a lazy stored-token-window gauge and all 16 effective startup search settings through the authenticated read-only API and bilingual WebAdmin Search page. Queue acceptance is distinct from delivery; direct client UDP is excluded. No payload/history or configuration write endpoint is added.

**Increment delivered in alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) adds connection-lifetime query/sent-result/received-result count and byte quotas, bounded denial notices, guest/global permission policy, trusted session grants and configurable active-search limits. Admission precedes routing/token publication. Optional strict result-token policy supplements existing owner/generation checks; direct client UDP remains outside hub enforcement.

Local verification on 2026-11-13: **114/114 CTest suites passed in each of Debug, Release and ASan/UBSan**, with required integration tools and default leak detection. Coverage includes independent reply modes, atomic endpoint projection, IPv4/IPv6 ADC/ADCS scenarios, real ncdc UDP sending/receiving and hub fallback, MariaDB and Apache HTTPS. No compiler warnings. WebAdmin build/typecheck, 4 Node tests, 6 Chromium tests and real C++ API integration passed. Metadata, UTF-8, documentation language parity/local links, changed Python syntax, shipped INI and whitespace checks passed. These are local results; remote CI, the full OS/client matrix and production load remain unverified.

Based directly on main `3655cf9175de72fac1ef2e4e3bfdd5ff023469a5`, after the alpha-0.5.9 merge. Earlier branches and code remain preserved; this increment is published separately from main.

## Search paths

In [ADC](https://adc.sourceforge.io/ADC.html), incoming TCP and incoming UDP capabilities are independent, separately advertised for IPv4 and IPv6. Search requests still travel through the hub using the existing B/F/D/E SCH rules. Clients determine the response transport from the searcher's current public INF: DRES through the hub or URES directly to a client UDP endpoint. A passive responder can send UDP without advertising its own incoming UDP capability. TCP activity does not imply UDP activity, and vice versa.

This release makes those declarations available as typed Search Core metadata and verifies actual client traffic. It adds no UDP listener, UDP proxy, multicast discovery or automatic packet emission to the hub. URES is a client-to-client message with a CID header, not a DRES with SID/target fields. URES received on the hub TCP/TLS stream remains outside the supported session profile. The hub cannot validate or count packets that bypass it.

## Public model and reply metadata

`users::ProfileData` now owns numeric `ipv4` / `ipv6` strings and optional signed-64-bit `udp4_port` / `udp6_port` values. Profile construction bounds and validates numeric address syntax, family, NUL/zone/port suffix rejection and ports 0..65535 before any narrowing. Address bytes contribute to the public profile size budget. Missing or zero ports do not advertise a usable UDP endpoint.

The ADC `user_from_info` projection copies I4/I6/U4/U6 only from validated ClientInfo. Existing kernel-peer address ownership remains authoritative: a wildcard I4/I6 becomes the actual canonical peer address, while a different IP/family is refused. There is no trusted alternate-family override in this increment. Existing INF transactions commit ClientInfo, User/profile and public delta together. Empty-field cancellation, zero ports and SU changes update the current mode without reconnect; invalid or failed allocations publish no partial change.

`search/connectivity.hpp` adds AddressFamily, ReplyRoute, a fixed-size owning UdpEndpoint (network-order address bytes plus port) and ReplyPaths. `reply_paths(profile)` allocates nothing and performs no DNS, socket I/O, timers or reachability checks:

| Declaration | Model |
| --- | --- |
| Usable I4 + exact SU TCP4 | `tcp4=true`; does not imply a UDP endpoint. |
| Usable I6 + exact SU TCP6 | `tcp6=true`; does not imply a UDP endpoint. |
| Usable I4 + exact SU UDP4 + U4 1..65535 | Owning `udp4` endpoint; IPv4 route is UDP4. |
| Usable I6 + exact SU UDP6 + U6 1..65535 | Owning `udp6` endpoint; IPv6 route is UDP6. |
| Missing feature/address/port, or zero port | That family's reply route is the hub. |

Unknown feature revisions/substrings do not grant support. Unspecified, multicast and limited IPv4 broadcast addresses do not become endpoints; scoped/link-local or IPv4-mapped IPv6 destinations are not treated as usable IPv6 endpoints. Loopback/private unicast remains valid for local hubs. A numeric endpoint is an advertisement, not proof of reachability or a credential.

`Core::reply_paths(owner)` returns an allocation-free owning snapshot for a current ready User, or no value for missing/unready/closing sessions. `Plan::reply_paths()` captures the source's paths at query admission; copies/moves keep that snapshot while later INF changes affect new lookups/plans. Existing SCH destination selection, feature matching and original wire fields are unchanged. Admission metadata does not override a client's current public INF, choose a socket for the hub, or cancel queued searches when a mode changes. Clients choose their actual transport; DRES remains supported even for UDP-active recipients.

## Passive and UDP behavior

The existing ResultRouter continues to validate and direct DRES to the ready target, including active-owner TO checks when present and tokenless compatibility when absent. Public UDP capability does not disable hub results. A missing or zero UDP port allows client fallback through the hub without a new server setting or rewritten SCH.

For active search, the hub shares only the already validated public address/port/capabilities and forwards SCH. A responder sends URES to the advertised client endpoint, with its CID, filename/size/root and copied TO when supplied. Delivery loss, duplicate datagrams, sender spoofing and client-side correlation are client concerns outside the hub's TCP admission path. Changing an endpoint does not revoke a datagram already sent to an older address/port.

ADCS protects the hub connection; it does **not** encrypt a separate UDP datagram. The new interoperability tests explicitly use ordinary UDP, including with an ADCS hub. [SUDP](https://adc.sourceforge.io/ADC-EXT.html), NAT traversal, hole punching, UDP reachability probing and client transfer negotiation are not implemented by this hub release. No UDP confidentiality or hub-side validation of direct results is claimed.

## Verification and boundaries

`search.connectivity` covers all sixteen independent TCP4/TCP6/UDP4/UDP6 feature combinations, IPv4/IPv6 address bytes, port boundaries and disabled modes, malformed/scoped/wrong-family addresses, unusable endpoints, exact feature revisions, ready-session lookup, admission snapshots, dynamic INF cancellation/updates, spoof/port rejection and allocation-free lookup/atomic projection faults.

`search.connectivity.process` exercises real IPv4/IPv6 ADC and ADCS sessions with a bystander, canonical endpoint publication, direct UDP sockets for active response scenarios, zero/canceled-port hub fallback and spoof rejection. The raw peers emulate clients; those datagrams do not pass through or get parsed by the hub. Existing feature, result, token, lifetime, queue and privacy suites remain enabled.

`adc.activepassive.ncdc` and `adcs.activepassive.ncdc` use a real indexed ncdc share. A passive ncdc sends URES to an active searcher's loopback UDP socket, with the correct CID/token/file/root. Setting the searcher's UDP port to zero restores DRES delivery through the hub. The same ncdc is then switched to active mode: it publishes its UDP endpoint, emits a token-bearing search and displays a received UDP result in its search UI. Earlier passive receiving/sending, reconnect and feature checks remain. These real-client tests use IPv4; raw peers cover IPv6.

No new CLI/INI option, API endpoint, database migration, operator message or WebAdmin search control is added. Network metadata stays in existing public INF and internal Users/Search models, not logs, chat counters or administrative status payloads. EN/US implementation, US/PL documentation and existing `languages/us` / `languages/pl` remain. Production load, NAT/firewall reachability and the complete OS/client matrix are not established by these loopback tests.
