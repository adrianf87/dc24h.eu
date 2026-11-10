# INF — alpha-0.2.6

**Increment delivered in alpha-0.3.12:** [Limits](LIMITS.md) adds configurable session/user ceilings shared by ADC/ADCS, bounded by the connection cap. SessionManager checks capacity transactionally; LoginHub rejects a full user registry before welcome/list preparation with ISTA 211 and own IQUI. Closing reservations count until cleanup. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.5:** [User Core](USER_CORE.md) adds checked UTF-8/NFC Nickname values, shared ADC/User text validation, immutable rename candidates and explicit projection from validated ClientInfo/VerifiedIdentity. Users now links the neutral dc24h_text library using the existing utf8proc package. Manager ownership/uniqueness remains unchanged. Earlier descriptions retain their historical scope.


**Increment delivered in alpha-0.3.1:** [Session Core](SESSION_CORE.md) provides the independent Users model and stable lifetime IDs, with live readiness/closure/timeline integration. Earlier stage descriptions below retain their original delivery scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

Since alpha-0.2.7, [CID/PID verification](ADC_IDENTITY.md) is a dedicated shared module with TigerCid/VerifiedIdentity values, direct cleansed output buffers and a constant-time final digest comparison. INF snapshots retain only a public verified result. Basic verification introduced in alpha-0.2.6 remains compatible; live transport attachment and account/registry policy remain separate.

## Delivered scope

`adc/inf.hpp` adds InfValidator, a read-only ClientInfo snapshot and ClientSession over [SidSession](ADC_SID.md). `adc/identity_hash.hpp` provides Tiger/192 and canonical base32 helpers. These are C++20 modules in dc24h_adc; no socket, logger, SQL or HTTP dependency is added. Production Network still invokes the parser alone, so this stage does not enable live login.

References: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), identification/state/INF sections; [TIGR extension](https://adc.sourceforge.io/ADC-EXT.html); [Libgcrypt algorithms](https://www.gnupg.org/documentation/manuals/gcrypt/Available-hash-algorithms.html). The implementation uses Libgcrypt GCRY_MD_TIGER1, standard Tiger/192 in NESSIE byte order, not GCRY_MD_TIGER's historical ordering, Tiger2 or a Tiger tree. Install libgcrypt20-dev to build. Algorithm/context unavailability fails validation; no global/FIPS mode is changed and no alternate hash is substituted.

This module checks a client's own metadata and PID/CID relationship. It does not establish global nickname/CID uniqueness, account ownership, permissions, registered-user passwords or routing. The owner still makes admission decisions before approving identity. Reported share/slot/speed values remain untrusted claims even when syntactically valid.

## Initial identity and public data

The local initial BINF profile requires NI, ID and PD. ID/PD must be 39-character uppercase unpadded base32 for 24 bytes, with the final three unused bits zero. Lowercase, padding, bad lengths and alternate encodings with nonzero unused bits are rejected. The decoded PID is hashed and compared to the CID. Owning a valid PID/CID pair is not registered-user authentication.

PD is borrowed from the caller's decoded Message and never inserted into ClientInfo. Temporary decoded PID bytes are cleansed; the Libgcrypt context is closed. This is not a promise to erase the original Message, decoder buffers or all compiler/library temporaries. The caller owns those inputs and must never log/forward the raw initial INF. No error contains a field value or nickname/address/PID. A public snapshot includes only recognized validated fields and cannot be default-constructed as an unvalidated previous snapshot.

`ClientInfo::field(name)` returns an optional borrowed string_view. `integer(name)` returns a checked int64 only for known numeric fields. `bytes()` counts retained names plus decoded values. `public_message(Sid)` returns an owning BINF Message with positional_count 0, sorted fields and no PD or unknown extensions. The caller supplies the correct owner's SID. This is decoded data, not wire serialization: a future serializer must escape spaces/backslashes before sending. Views do not survive snapshot replacement/destruction; copies and exported Messages own their storage.

## Validated profile

These are explicit local rules; they do not claim every ADC extension or field is implemented.

| Fields | Rules |
| --- | --- |
| NI | Required; 1..64 UTF-8 bytes, NFC, codepoints above ASCII space; no Unicode control/format/line/paragraph separators |
| ID | Required canonical Tiger-sized CID; immutable after initial validation; same-value updates are permitted |
| PD | Required initially; absent from snapshots and forbidden in every later update |
| I4 / I6 | Numeric address in the actual peer's family, matching that peer; zero placeholder becomes the peer's canonical address |
| U4 / U6 | Integers 0..65535; zero conveys no validated listening capability |
| SS, SF, US, DS, SL, AS, AM, HN, HR, HO, FS | Nonnegative signed-64-bit values; no truth/relationship checks; FS is optional metadata |
| AW | Integer 0..2 |
| CT | Client may report 0 or informational bot flag 1; registration/operator/owner/hub/other bits are rejected and no role is granted |
| SU | Up to 128 distinct four-character features, comma-separated; no empty entries, trailing comma, duplicates or lowercase; independent of SUP |
| DE / RF | At most 2048 bytes each; RF is inert text, never fetched |
| AP / VE | At most 128 bytes each |
| EM | At most 254 bytes; bounded text, not an email-address verifier |
| LO | At most 64 bytes |

Known text fields must be valid NFC UTF-8 and reject ASCII controls/DEL, Unicode control/format and line/paragraph separators. Text other than NI permits ordinary ASCII spaces. This is stricter than merely accepting every Unicode codepoint above the protocol minimum. Integer syntax is the existing parse_integer contract: no plus/fraction/overflow, leading zeroes and negative zero accepted. Cancellation is represented by a bare two-character name. Optional fields are removed; NI/ID cannot disappear. PD cancellation is still an illegal post-initial PD occurrence.

InfValidator construction takes a trusted numeric kernel peer address, without port, brackets, hostname or zone. IPv4-mapped IPv6 peer addresses normalize to IPv4. Declared addresses must use the same family; an alternate-family claim is rejected even if it is unspecified. This narrow profile has no trusted-user exception or independently verified dual-stack address service. IP cancellation removes public metadata, not the trusted peer context. Transport security must continue to use the actual socket peer.

Unknown names are ignored and never exported. Duplicate names fail the entire INF, including repeated unknown names, instead of choosing a first/last value. The syntax parser still preserves every token; this is the consumer's local ambiguity policy. Bare optional fields remove previous values; absent fields preserve them. Full snapshots are not rebuilt from only the latest update.

## Bounds and transactional validation

Only parser-produced decoded BINF enters the validator. It additionally checks command/type/source, no target/F selectors, at most 128 parameters, valid named prefixes, duplicate names and per-token bounds for hand-built callers. It does not recreate complete wire framing/escaping validation. Callers must use the source SID actually bound to that connection and the negotiated SessionHash; unsupported hashes fail.

Retained name/value bytes total at most 4096, plus bounded map/container overhead for the fixed recognized field set. SU is at most 639 bytes. Each known field has its own limits; the parser retains its existing 16 KiB frame bound. Unknown values are not copied. Changes are applied to an owning candidate, with the cumulative byte budget checked after each stored update. Remove old optional fields before adding at the limit. A rejected candidate leaves the previous snapshot unchanged, including when the failing field follows earlier valid changes. Allocation exceptions propagate before any session state commit.

ID is fixed after initial validation. A new PID is never accepted as an in-place identity change; reconnection requires a new session and initial validation. A standalone InfValidator knows no connection state. Its previous argument must be that connection's validated snapshot, not another client's metadata. Global rename/CID admission belongs to the future registry and must be checked before committing/announcing changes in a live hub.

## Composed session contract

ClientSession requires the shared SidPool, trusted peer address and optional SupportPolicy. It is noncopyable/nonmovable, belongs to the same owner thread as its pool and exposes no mutable lower-level state. SUP/SID preparation and completed-send sequencing remain unchanged.

Incoming INF is allowed only in IDENTIFY or NORMAL. The adapter validates a candidate before the lower-level state observes it and moves the snapshot only after state acceptance. A malformed/duplicate/mismatched INF therefore cannot set the pending identity flag or overwrite existing metadata. Valid initial INF remains in IDENTIFY. `approve_identity(mode)` additionally requires a validated snapshot, but the caller must first enforce uniqueness, bans, account/guest policy and any other admission rules. The password branch still needs an external authentication service. NORMAL updates are validated atomically; updates in VERIFY are rejected. Successful parsing or validation never silently grants NORMAL.

Outgoing INF containing a named PD is rejected by this adapter as an additional guard; use the public snapshot builder. This is not a complete outbound permission/serialization service. Terminal own-SID QUI, fatal peer/hub status and disconnect clear metadata and release the SID through the underlying session. Other-user events do not. No HTTP schema/counters, SQL migrations, operator catalogs or configuration options change.

`observe` returns optional variant<StateError, SupportError, SidError, InfError>. InfError categories are fixed: invalid_message, wrong_source, duplicate_field, missing_identity, invalid_cid, invalid_pid, cid_mismatch, invalid_nick, invalid_text, invalid_integer, invalid_ip, invalid_features, privilege_claim, field_limit, immutable_identity, unexpected_pid, unsupported_hash and crypto_unavailable. Suggested fatal status codes are 227 for PID/hash mismatch, 221 for invalid nick, 246 for invalid IP, 247 for unsupported hash, 210 for provider failure and 243 for remaining field failures. The helper sends no STA or disconnect and includes no input text. Owners must bound retries and close rejected connections appropriately.

## Verification and remaining work

`adc.inf` checks fixed Tiger empty/abc vectors, byte order, 1000 base32 round trips and noncanonical tail bits; required/duplicate/private/unknown fields; integer/text/feature/byte bounds; IP placeholder, normalization and spoofing; immutable identity, cancellation, public ownership, atomic updates and guest/password sequencing. Stream tests compare every two-chunk split and bytewise UTF-8 input with semantic rejection and parser recovery.

`adc.inf.ncdc` uses actual ncdc in the existing isolated fixture with --inf. The C++ ClientSession negotiates SUP, owns an allocated SID and validates the real client's PID/CID/BINF, exports no PD and remains in unapproved IDENTIFY. TCP and Python-owned TLS variants pass send-completion events separately and clean up the session. It does not demonstrate C++ Network attachment or full guest login. Existing actual C++ TCP/OpenSSL and older state/SUP/SID fixtures remain separate.

Future integration still needs bounded ordered output/preflight, escaping, registry uniqueness/admission/rename transactions, user-list/QUIT lifecycle, registered-user authentication and whole-login deadlines. Tiger identity hashing is implemented here; TTH file trees and a password service are not. See [TESTING.md](TESTING.md), [SECURITY.md](SECURITY.md) and [ROADMAP.md](ROADMAP.md).
