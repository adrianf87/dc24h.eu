# User Core — alpha-0.3.3

This branch starts from main `ca00266992dd866bdd30ff68e911c981c6665a62`, after the Session Core merge. The requested alpha-0.3.2 Session Manager is a separate sibling branch from the same main, not an implicit dependency or automatic merge. User Core extends dc24h_users with an owning public guest User and a bounded typed Profile, integrated with live LoginHub admission and updates.

## User, session and identity

A Session describes one transport lifetime and its readiness/closure timeline. A User describes the admitted public guest on that session: Identity (protocol SID, client CID, nickname) plus Profile. Session owns an optional User, absent before admission. There is one live copy of public identity in the Users model. Session::identity() is a borrowed const pointer into User; Session::user() is a const optional reference. Neither permits mutation outside lifecycle methods. Copies of User/Profile own their strings and feature list.

Identity moved from session.hpp to user.hpp; existing includes still work through session.hpp. Storage limits remain 64 bytes SID, 128 bytes CID and 256 bytes nickname, nonempty and NUL-free. These are bounded opaque public keys, not additional ADC validation. The existing ADC Sid/TigerCid codecs, Tiger proof, NFC/nickname rules and uniqueness indexes remain authoritative. Creating an Identity/User directly proves only storage invariants, never CID ownership, account status or uniqueness. CID is not a persistent account ID or authentication credential. SID belongs to a connection and may be leased again after cleanup; local SessionId identifies a distinct lifetime.

User is a copyable value with read-only identity/profile accessors. User::create rejects an invalid/moved-from identity. Keys cannot be changed on an admitted session. Session::identify(User,time) binds it once while negotiating. The Identity overload remains available and binds an empty profile. Session::update_user(User,time) replaces nickname and complete profile together only in identified/ready, with unchanged SID/CID and nondecreasing time. Invalid state, backwards time or changed keys leave the entire model unchanged. Legacy rename mutates the same User identity and preserves its profile. Close retains the last public value until the session is removed; it does not create a retained user account.

## Public profile

ProfileData has an explicit allowlist, not arbitrary INF fields:

| Field | ADC projection | Storage / semantics |
| --- | --- | --- |
| description | DE | Up to 2048 bytes |
| email | EM | Up to 254 bytes; a public client declaration, not verified contact data |
| application / version | AP / VE | Up to 128 bytes each |
| locale | LO | Up to 64 bytes; declared text, not locale selection for the server |
| shared_bytes / shared_files / slots | SS / SF / SL | Optional nonnegative int64, including zero and INT64_MAX |
| features | SU | Up to 128 unique four-character uppercase feature tokens |

Profile::create validates those byte/count bounds, ASCII control/NUL exclusion, nonnegative numbers and feature shape/duplicates. Total retained text/token payload is bounded by 4096 bytes; current individual limits also bound it below that value. Empty text/list and absent numbers represent no declaration, distinct from numeric zero. The model does not verify resource claims, email addresses or feature behavior. UTF-8/NFC semantics remain checked by the ADC validator before projection; the storage factory alone is not a Unicode/protocol validator. The library stays independent of ADC, OpenSSL, utf8proc, SQL, sockets and logging.

No PID/PD, PAS/password, peer address, I4/I6, port, arbitrary extension, RF/referrer, privilege CT, account, role or ban is stored in User/Profile. Explicit C++ fields prevent accepting an arbitrary metadata map. Existing protocol ClientInfo still owns the validated protocol metadata needed for wire relays; this stage does not change public ADC INF serialization or hide fields already required by that protocol.

## Admission, updates and snapshots

LoginHub projects the fully validated ClientInfo, never raw tokens. It builds an admitted User only after identity/uniqueness checks and successful bounded welcome construction. Identified remains distinct from ready until full own-INF delivery. Every accepted INF update projects the complete merged ClientInfo, so omitted fields retain their prior value while explicit cancellations remove it. Nickname and profile commit together through update_user. Invalid INF and duplicate nickname rejection retain the previous model while closing according to existing policy. Allocation exceptions and protocol output failures follow existing behavior; no arbitrary-allocation-fault guarantee is added.

Snapshot retains the existing optional identity field and adds optional profile, both copied from the same User. Both are absent before admission and present after it, including an empty profile. This avoids duplicating identity in the live model while keeping existing snapshot identity access. Snapshot values survive updates, closure and removal. Borrowed references remain owner-thread-only and expire when replaced or destroyed. Snapshots are internal C++ data, not an API JSON schema change or permission to expose profiles publicly through HTTP.

The protocol nickname/CID/SID uniqueness indexes, peer registry, transport ownership, quota rules and wire error responses remain unchanged. No SQL migration, account persistence, configuration option, new runtime message or dependency is needed. Existing US/PL runtime catalogs remain applicable. Main and historical branches are preserved.

## Verification and limits

`users.core` covers exact text/feature/numeric bounds, controls/NUL, invalid feature shape/duplicates, negative declarations, owned snapshots, phase/time guards, immutable SID/CID and atomic profile replacement. Its actual LoginHub cases cover validated profile projection, INT64_MAX without truncation, full updates/cancellations, failed INF rollback, duplicate nick rollback, surviving peers, cleanup and reconnect. Existing Session Core, TCP/TLS process tests and real ncdc login/reconnect/STA/security cases remain required.

This is an in-memory public guest model, not registered-user authentication, a global user directory, multiple sessions per account, roles, bans, chat/search/transfers, persistent history or administrative mutation. Client declarations are untrusted metadata. See [testing](TESTING.md), [Session Core](SESSION_CORE.md), [INF](ADC_INF.md) and [identity validation](ADC_IDENTITY.md). Local tests do not imply other OS, remote CI, load, fuzz or security audit results.
