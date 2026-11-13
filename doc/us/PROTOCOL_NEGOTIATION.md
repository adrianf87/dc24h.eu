# Protocol Negotiation — alpha-0.6.5

**Current increment — alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) adds startup CTM/RCM and guest permissions, trusted ready-lifetime grants and preparation/dispatch/echo authorization rechecks. Shared binary IP and port validation preserve managed-profile behavior. Native, mixed ADC/ADCS and real ncdc cases cover denials, spoofing and recovery.

**Increment delivered in alpha-0.6.7:** [Transfer Tokens](TRANSFER_TOKENS.md) unifies bounded opaque token validation and owning values across Core, CTM/RCM and peer errors. Token bytes, including Unicode/edge spaces/backslashes, survive immutable routing and real ncdc boundary handshakes. Independent requests may reuse tokens; no token registry or inferred correlation is added.

**Increment delivered in alpha-0.6.6:** [Client Capabilities](CLIENT_CAPABILITIES.md) unifies owning IPv4/IPv6 TCP modes and advisory peer protocols. Shared explicit CTM/RCM selection preserves wire intent; a separate trusted-originator helper chooses CTM or RCM and refuses passive/passive. Live INF changes, stale-plan rechecks and real ncdc mode transitions are covered.

**alpha-0.6.5 local validation (2026-11-13):** All **141/141 CTest tests** passed in each of Debug, Release and ASan/UBSan with required integration tools and default leak detection. Coverage includes protocol selection/ownership/faults, explicit known/extension routing, fragmented IPv4/IPv6 mixed ADC/ADCS peer failures, four certificate-checked real ncdc TLS peer suites, earlier plaintext and terminal-pressure ncdc, Search/UDP, MariaDB and Apache HTTPS. All 11 active ncdc scenarios also passed three consecutive ASan/UBSan runs (33 passes) with reserved ports. No compiler warnings. WebAdmin build/typecheck, **12 Node tests**, **10 Chromium tests** and real C++ API integration passed. Metadata, UTF-8, language parity/local links, changed Python syntax, example INI and whitespace checks passed. The full Debian/Ubuntu support matrix and production load remain unverified; remote CI results are reported separately by GitHub Actions.

This increment starts from main `9a041df4a65090580f5b009acdcd6758cadbf1dd`, including the alpha-0.6.4 CI repair. The feature branch preserves history and is published separately from main.

## Explicit peer protocol

Transfer Core now owns `transfers::Protocol` values inside Request. The existing `Request::protocol()` string accessor remains; `protocol_value()` exposes the checked immutable value. CTM/RCM adapters and shared connection frames expose the same classification without allocation. Copying or moving a request never leaves borrowed protocol storage in another plan.

| Exact identifier | Kind | Encryption intent |
| --- | --- | --- |
| `ADC/1.0` | `adc_1_0` | Plain TCP between clients |
| `ADCS/1.0` | `adcs_1_0` | TLS required between clients |
| Any other valid identifier | `extension` | Defined by the peers; no inferred plaintext/TLS |

Matching is case-sensitive and exact. Prefixes, changed versions, lowercase names and legacy `ADCS/0.10`/`ADC0/0.10` remain extension identifiers. An extension never silently maps to plaintext, TLS or a canonical known name. `protocol_name()` returns no name for extensions or invalid native enum values; unknown encryption is represented as `peer_defined`.

Core's existing bounded NFC protocol text contract (1..32 bytes) remains. The ADC codecs independently require visible ASCII identifiers and preserve original bytes, positional arguments, token, port spelling and extensions. The explicit protocol in a received CTM/RCM is authoritative for forwarding. The hub does not select a replacement, synthesize a reply, retry with plaintext or reject an unknown valid extension on behalf of a peer. Selected session lifetimes, address-family roles, fixed SID audiences, attempt consumption, queues and error priority remain those of [Connection Routing](CONNECTION_ROUTING.md).

The [ADC specification](https://adc.sourceforge.io/ADC.html) assigns unsupported-protocol decisions to receiving clients and requires an RCM reply to retain protocol and token. The [ADCS extension](https://adc.sourceforge.io/ADC-EXT.html) defines TLS before the peer ADC handshake and the `ADCS/1.0` identifier. These boundaries guide this increment; the hub coordinates requests and does not terminate peer TLS.

## Selection for trusted originators

`advertised_protocols(Profile)` projects baseline ADC and the exact public INF SU `ADCS` declaration. `ADC0` alone is not treated as modern ADCS/1.0. TCP/UDP activity, hub ADC/ADCS transport and SUP features do not imply peer TLS support. Declarations are unverified capabilities, not proof of an authenticated or encrypted peer connection. The pure profile helper does not establish session readiness; live Core admission still does.

`negotiate_protocol(origin, peer, preference)` is an allocation-free, stateless API for a trusted caller originating a new request. It considers both supplied capability sets:

| Preference | Selection |
| --- | --- |
| `prefer_adc` | Common ADC, otherwise common ADCS |
| `prefer_adcs` | Common ADCS, otherwise common ADC |
| `require_adcs` | Common ADCS, otherwise `encryption_unavailable` |

No overlap yields `no_common_protocol`; an invalid native preference yields `invalid_preference`. Capability sets may disable baseline ADC explicitly. This is a reusable originator API, not an automatic hub policy for received CTM/RCM. Live routing preserves explicit requests even if current public claims change; it never substitutes the helper's preference for a client's choice. A TLS handshake or certificate failure belongs to the peers and cannot authorize hub fallback.

## Peer failure replies

The existing STA validator now treats TO in connection errors 41/42 as the same opaque NFC token accepted by CTM/RCM (1..128 bytes). This fixes rejection of legitimate escaped spaces, Unicode and backslashes. PR in those replies uses the same bounded visible-ASCII grammar as connection requests (1..32 bytes). Existing duplicate checks, frame/field ceilings, native serialization boundary and unknown-field removal remain. Other status codes retain their previous token rules; this does not add mandatory fields or a pending-token registry.

Ready, source-bound DSTA/ESTA routing remains separate and follows its explicit target SID. Peer errors are not matched to an inferred request, CID, token owner or stored negotiation. Bystanders receive nothing, and a forged source still receives the existing fatal rejection. Only the selected recipients see the supplied protocol/token; logs, SQL, API/WebAdmin and Chat/Search counters do not acquire connection payloads or history.

## Verification and limits

`transfers.protocol` covers exact classification, ownership, limits/NFC, extension safety, capability/preference combinations, required-TLS refusal, invalid native preferences and allocation-free selection/dispatch. Both D/E CTM and RCM preserve intent and audience after caller input or public capability changes. Native peer-error cases cover opaque tokens, bounds, duplicates, controls and unknown-field stripping.

`transfers.protocol.process` exercises IPv4/IPv6 with mixed ADC/ADCS users, both scopes and commands, known/unknown/legacy identifiers, changing declarations, client CTM replies and peer 141/142 errors. It verifies byte preservation, source ownership, bystander isolation, chat recovery and unchanged API/log privacy.

Four new real ncdc suites (`adc.ctm.adcs.ncdc`, `adcs.ctm.adcs.ncdc`, `adc.rcm.adcs.ncdc`, `adcs.rcm.adcs.ncdc`) use exact ADCS/1.0 with TLS 1.2/1.3 peer sockets and certificate verification against the disposable local ncdc certificate. The raw peer also supplies a disposable certificate. RCM covers ncdc connecting to a TLS listener and ncdc responding as the active TLS listener; CTM reaches CSUP/CINF/CGET over TLS. A real ncdc unsupported-protocol reply preserves a Unicode token. Plain peer suites continue over both hub transports, and the terminal-pressure regression remains enabled.

The ncdc activation helper reserves OS-assigned TCP/UDP port numbers with SO_REUSEADDR until the client advertises its live endpoints. Unlike kernel bind(0), ncdc port 0 chooses a random possibly occupied number. The non-listening TCP reservation and temporary UDP reservation prevent concurrent tests/outgoing sockets from taking the numbers during handover; the helpers release both before peer traffic. CTM replies must use the reserved TCP port.

The tests' explicit local certificate trust is not a production KEYP implementation. Client KP projection/pinning policy, production certificate trust, pending-token correlation/deadlines, transfer security policy and file-data transfer remain later work. There is no new dependency, configuration key, permission, database migration, endpoint, transfer metric or WebAdmin page. See [Build](BUILD.md), [Testing](TESTING.md), [Transfer Core](TRANSFER_CORE.md), [ADC CTM](ADC_CTM.md) and [ADC RCM](ADC_RCM.md).
