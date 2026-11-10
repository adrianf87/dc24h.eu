# CID/PID — alpha-0.2.7

**Current increment — alpha-0.3.7:** [User Registry](USER_REGISTRY.md) owns bounded SID/CID indexes and a reverse session-owner index privately within SessionManager. Admission is transactional across this registry, NickManager and the Session model. Const User lookups resolve both keys to the same current model; reservations remain until session removal. Earlier module descriptions retain their historical scope.

**Increment delivered in alpha-0.3.5:** [User Core](USER_CORE.md) adds checked UTF-8/NFC Nickname values, shared ADC/User text validation, immutable rename candidates and explicit projection from validated ClientInfo/VerifiedIdentity. Users now links the neutral dc24h_text library using the existing utf8proc package. Manager ownership/uniqueness remains unchanged. Earlier descriptions retain their historical scope.

**Current increment — alpha-0.3.3:** [User Core](USER_CORE.md) adds an owning public guest identity/profile to Session, with validated full-INF projection and lifecycle-guarded updates. It was developed directly from main with Session Core; main now combines it with the previously merged alpha-0.3.2 Session Manager. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.1:** [Session Core](SESSION_CORE.md) provides the independent Users model and stable lifetime IDs, with live readiness/closure/timeline integration. Earlier stage descriptions below retain their original delivery scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

## Scope

The basic Tiger relationship check was introduced with [INF](ADC_INF.md) in alpha-0.2.6. This increment extracts it into `adc/tiger_identity.hpp` and adds typed public values, direct-buffer handling, final binary comparison and dedicated failure tests. INF now uses the shared verifier and retains its public result. Accepted canonical pair semantics remain compatible.

References: [ADC identification/session hash](https://adc.sourceforge.io/ADC.html), [TIGR](https://adc.sourceforge.io/ADC-EXT.html) and [Libgcrypt hash lifecycle](https://www.gnupg.org/documentation/manuals/gcrypt/Working-with-hash-algorithms.html). The CID is Tiger/192 of the **decoded 24-byte PID**, not its 39-character spelling. The implementation continues using GCRY_MD_TIGER1/NESSIE output order. It does not use Tiger2, legacy GCRY_MD_TIGER ordering, a TTH leaf prefix or a file Merkle tree.

The module verifies an intrinsic pair, not account ownership, global CID/nickname uniqueness, permissions or password authentication. It neither generates PIDs nor attaches to the actual receive-only Network. Registry/admission, bounded output and complete live login remain separate milestones.

## Types and canonical input

`TigerCid::parse(text)` returns optional TigerCid for exactly 39 uppercase unpadded base32 symbols. Its final three unused bits must be zero: only A/I/Q/Y are valid final characters. Reject lowercase, whitespace, padding, embedded NUL, non-ASCII characters, wrong length and nonzero tail bits. All 24-byte values, including zero, have a valid encoding; shape validity is not proof of a PID relationship. `text()` returns an owning canonical string; `bytes()` borrows the public 24 bytes for that object's lifetime.

`verify_tiger_identity(cid, pid, hash)` returns variant<VerifiedIdentity, IdentityError>. Only the negotiated SessionHash::tiger192 is accepted; unsupported enum values fail before parsing/hashing, without fallback. The CID is checked first, then PID format, provider availability/context, and the digest comparison. Invalid pairs never produce a success object.

VerifiedIdentity has no default/public value constructor and contains only a TigerCid. It can be copied/moved safely as public data after successful verification. It stores no PID, input view, session pointer or role. Its presence means only that this module successfully checked the supplied pair at that time. It is not a transferable authorization token. Mutating the caller's original strings does not change the returned CID.

IdentityError is fixed: invalid_cid, invalid_pid, mismatch, unsupported_hash, crypto_unavailable. `identity_status_code` suggests 243, 227, 227, 247 and 210 respectively for a future dispatcher; it sends no wire response/disconnect and returns no input text. Malformed syntax is rejected before opening a digest context. Length checks bound oversized input before decoding it; the verifier processes only 24 PID bytes.

## Private buffers and provider handling

The verifier borrows encoded strings and uses two noncopyable/nonmovable local 24-byte buffers: decoded PID and calculated digest. `decode_tiger_base32(text, destination)` writes directly into a supplied digest, with no optional-by-value private buffer in this path. Every decode failure clears the whole destination, including partially decoded data. Its destination must not overlap the encoded input.

`tiger192_into(bytes, destination)` hashes into a supplied output buffer. It checks algorithm availability, opens an owned Libgcrypt context, finalizes, copies the digest and closes the context on every successful-open path. Availability/open/read failure clears the destination and returns false. Positive hashing permits input/output aliasing because input is consumed before writing output. No alternate algorithm or global/FIPS mode change is attempted.

The final comparison is exactly 24 bytes via OpenSSL CRYPTO_memcmp. This comparison uses that library's constant-time primitive; syntax validation, initialization, provider operations, error precedence and the whole handshake are **not** claimed constant time. There is no timing benchmark or complete side-channel audit in this stage.

Both private owned buffers are cleansed by destructors on every normal return, including mismatch and provider failures. Libgcrypt context cleanup closes its internal state. This is limited to owned buffers/context lifecycle: caller Messages/decoder strings, registers, crash dumps and arbitrary compiler/library temporaries are not promised erased. Original encoded PID input remains the caller's responsibility and must not be logged or forwarded.

The alpha-0.2.6 optional-return hash/decoder helpers remain available and delegate to direct-output functions. These are general byte utilities; callers own any returned arrays and their lifetime. Prefer the dedicated verifier for PID checks. No dependency is added beyond existing Libgcrypt and OpenSSL.

## INF integration

InfValidator uses TigerCid for ID shape and verify_tiger_identity for the initial relationship. A successful ClientInfo retains optional VerifiedIdentity alongside the existing ID string; `identity()` returns an owning optional copy. Published valid snapshots contain that result. Ordinary field updates preserve it; existing immutable-ID and forbidden-later-PD rules prevent in-place reassignment. A new connection requires its own initial verification.

ClientInfo's 4096-byte name/value budget is unchanged; the fixed public result adds 24 bytes plus optional/container overhead, never private data. `public_message` still exports only recognized validated fields, with no PD or extra identity-result field. A public result does not bypass ClientSession's external admission or password decisions. Failed verification/provider calls leave snapshot, state and SID reservation unchanged.

No HTTP schema/route, SQL migration, TLS policy, INI option or operator catalog changes. The actual C++ Network still uses the syntax parser alone; module fixture success is not live hub login.

## Verification

`adc.identity` checks reference empty/abc digest bytes and direct-output aliasing, all nonalphabet byte substitutions at every encoded position, all 32 final-symbol possibilities, wrong lengths, cleared failed-decode buffers, 1000 deterministic valid PID/CID pairs and one-bit PID mismatch cases. It also checks public result ownership, fixed errors/statuses, unsupported hash handling, INF result preservation and immutable CID updates. These are deterministic functional tests, not a fuzz campaign.

`adc.identity.failure` links a separate executable with GNU-compatible `--wrap` replacements for gcry_md_algo_info, gcry_md_open and gcry_md_read. It deterministically injects algorithm-unavailable, context-open and digest-read failures, checks zeroed output, no successful identity/INF commit, retained pending SID, recovery after removing the fault and cleanup. Wrappers apply only to that executable; production has no injectable verifier/provider interface, no preload or host configuration change. Version-initialization failure and arbitrary allocation fault injection remain untested branches.

`adc.identity.ncdc` uses real ncdc with disposable loopback TCP and Python-owned TLS. The C++ driver completes module SUP/SID steps, first rejects a one-bit altered canonical CID without changing IDENTIFY state, then accepts the actual client's pair, checks the typed result and exports no PD. It deliberately remains in unapproved IDENTIFY and releases its SID on disconnect. Actual C++ TCP/OpenSSL transport tests and existing INF/SID/SUP fixtures remain separate. See [TESTING.md](TESTING.md).
