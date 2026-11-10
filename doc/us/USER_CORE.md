# User Core — alpha-0.3.5

**Current increment — alpha-0.3.10:** [INF Updates](INF_UPDATES.md) validates and prepares ready-state updates before atomically committing ClientInfo, User/profile and nickname ownership. Fanout carries validated supplied fields and explicit cancellations; initial lists remain full public snapshots. Unknown-only/empty updates produce no public frame. Earlier module descriptions retain their historical scope.

This increment starts from main `ba12aa3a96ea70e2b64ca3b11a519bd862e7f2f4`, after the alpha-0.3.4 Session Manager merge. It develops the public guest model introduced in alpha-0.3.3: checked nickname values, shared Unicode text rules and an explicit ADC projection. The manager remains the owner of registered sessions and uniqueness indexes. Main and historical branches are preserved; this branch is not merged automatically.

## Identity and checked names

Session describes one transport lifetime; User is the admitted public guest's Identity plus Profile. Session owns one optional User, absent before admission. SessionManager owns registered Sessions and SID/CID/nickname indexes. Registered model access is const; mutations use manager gates. User/Identity/Profile remain owning copyable values, not account records.

Identity keeps bounded opaque SID/CID strings and a `users::Nickname` value. SID and CID storage bounds remain 64/128 bytes, nonempty and NUL-free. Their generic factory does not prove ADC syntax, lease ownership, Tiger identity, account status or uniqueness. The live adapter supplies the real four-character Sid and canonical public CID from ClientInfo::VerifiedIdentity. SID is a reusable protocol lease; SessionId distinguishes connection lifetimes. CID is not account authentication.

Nickname has no default or public unchecked constructor. `Nickname::create` requires nonempty, at most 256 UTF-8 bytes, already in NFC, without controls/format characters or line/paragraph separators. ASCII space is forbidden in names, matching the existing ADC text policy. No silent normalization, case folding or locale-dependent comparison is introduced. The generic 256-byte bound is deliberately wider than ADC's unchanged 64-byte nickname limit; passing the model factory alone does not satisfy all ADC policy. Non-ASCII characters follow the existing Unicode-category policy, not an expanded blanket whitespace ban.

`Identity::create(sid,cid,text)` uses this checked type. The overload taking Nickname retains the same SID/CID storage checks without revalidating the name. `nickname_value()` returns const Nickname; `nickname()` preserves the existing const string accessor. There is only one owned nickname string. `Nickname::valid()` cheaply checks the presence/storage invariant of a constructed or moved-from value; it does not rerun Unicode allocation. Read-only access prevents bypassing construction through the supported API.

## Shared text and build dependency

`dc24h_text` provides `text::valid_public_text(value, nickname)` using the project's existing utf8proc dependency. It extracts the previous ADC INF validator's UTF-8/NFC/category checks; ADC and Users now call the same function. It accepts empty fragments so callers can represent optional absent profile fields, while Nickname enforces presence. It rejects malformed/overlong UTF-8, surrogates, non-NFC input, ASCII/C1 controls, Unicode format controls and line/paragraph separators. Input is capped at 4096 bytes before normalization allocation. Existing individual field limits are checked first.

Profile factories now enforce these Unicode rules for every public text field, closing the previous direct-model gap where non-ASCII malformed data could pass byte/control checks. The wire policy remains unchanged because ADC already checked those rules. Invalid values return nullopt; allocation failure propagates as an exception before model mutation, with no termination wrapper or fabricated validation result. Normalization memory is bounded by the input cap and library processing; no exact byte budget or fault-injection audit is claimed.

Users depends on the neutral text library, which privately links utf8proc. ADC uses the same library. There is no Users-to-ADC, TLS, crypto, socket, SQL or logging dependency. This adds internal target linkage, not a new system package; existing `libutf8proc-dev` remains required.

## Profile and immutable value changes

ProfileData retains its explicit public allowlist:

| Field | ADC projection | Bound / meaning |
| --- | --- | --- |
| description | DE | 2048 bytes |
| email | EM | 254 bytes, public declaration rather than verified contact data |
| application / version | AP / VE | 128 bytes each |
| locale | LO | 64 bytes, declared metadata rather than server language selection |
| shared_bytes / shared_files / slots | SS / SF / SL | Optional nonnegative int64, zero and INT64_MAX included |
| features | SU | At most 128 unique uppercase four-character feature tokens |

Text is valid NFC UTF-8 without forbidden categories; ordinary spaces remain allowed in profile text. Empty text/list and absent numbers mean no declaration, distinct from zero. Existing 4096-byte aggregate text/token bound and individual bounds remain unchanged. Resource, email and feature claims are not verified facts.

`User::with_nickname(Nickname)` creates a detached owning candidate preserving SID, CID and the complete profile. It does not mutate the source or reserve a name. Session's standalone rename and SessionManager's managed rename both use this candidate path. Neither needs privileged access to private User/Identity fields. Live changes still require phase/time checks, unchanged SID/CID and manager uniqueness. Invalid text or a name collision leaves both the original User and registry indexes unchanged. Existing identify(User), update_user and Identity admission remain available.

Snapshots copy identity/profile together from the User. They survive source updates, closure and removal. Borrowed model/profile/name references remain owner-thread-only and expire on replacement or destruction. No user revision, cross-thread publication, account persistence or HTTP profile endpoint is introduced.

## Explicit validated ADC projection

`adc::user_from_info(const ClientInfo&, Sid)` is the public adapter in adc/user.hpp/cpp. It accepts the already validated ClientInfo type, takes CID from its VerifiedIdentity and a nonzero typed Sid supplied by the caller, creates the checked nickname, and projects only the listed profile fields. It returns an optional User; absent identity/name, zero SID or incompatible model bounds fail closed. It does not rehash PID, inspect credentials, acquire a SID lease or decide global uniqueness/readiness.

LoginHub uses this adapter after successful bounded welcome construction and after accepted merged INF updates. Omitted fields retain their merged values; explicit cancellations clear them. The adapter borrows ClientInfo and returns owning values. PID/PD, PAS/password, peer IP, I4/I6, ports, arbitrary extension fields, RF/referrer, privilege CT, account/roles and bans are never copied to User/Profile. Protocol ClientInfo still retains validated wire metadata needed for existing relays; serialization and status codes remain unchanged.

No SQL migration, new API JSON field, configuration option or outward runtime message is required. Existing US/PL runtime catalogs remain applicable. Registered-user authentication, multi-session accounts, roles, bans, persistent/distributed identity, chat/search/transfers and administrative mutation remain separate work.

## Verification

`users.values` covers typed construction, invalid/overlong/truncated UTF-8, surrogates, noncanonical normalization, NUL/controls/format/separator rejection across names and every profile text field, UTF-8 byte limits, detached rename ownership, standalone/managed rejection rollback, verified CID/SID projection and merged field cancellation. It also proves that ADC retains its stricter 64-byte nickname bound. Existing manager/core/identity suites remain required.

Real ncdc login/reconnect now uses `dc24hŻółw` on both ADC and ADCS. The plaintext test checks its public INF; both transports reach NORMAL and retain one readiness/cleanup per lifetime without nickname leakage in logs. ADCS wire remains encrypted. See [testing](TESTING.md), [Session Core](SESSION_CORE.md), [Session Manager](SESSION_MANAGER.md), [INF](ADC_INF.md) and [identity proof](ADC_IDENTITY.md). Local tests do not imply other OS, remote CI, production load or a security audit.
