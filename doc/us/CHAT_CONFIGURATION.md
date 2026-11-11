# Chat Configuration — alpha-0.4.10

Based directly on main `6d61edfdd2057a77f8c4722a1495cfa37bea7b19`, after the Anti-Flood merge. Code and protocol diagnostics use EN/US UTF-8; operator help/errors and documentation also support Polish. Existing branches/history are preserved.

## Startup settings

`chat::Settings` in `chat/settings.hpp` groups the existing FloodLimits, a checked client `Policy`, and welcome settings. Configuration parsing, merged validation and Network startup use the same model. Network copies the validated values into LoginHub; adapters retain their own immutable policy copies. Settings are startup-only: edit the INI and restart. No new CLI flags, live reload, HTTP mutation or WebAdmin configuration controls are introduced.

| `[chat]` key | Default | Accepted value / meaning |
| --- | --- | --- |
| `max_text_bytes` | 16384 | 1–16384 decoded UTF-8 text bytes |
| `public_enabled` | on | on/off; client BMSG |
| `private_enabled` | on | on/off; client DMSG and EMSG |
| `actions_enabled` | on | on/off; actual ME1 actions in B/D/E |
| `welcome_enabled` | on | on/off; initial per-client IMSG greeting |
| `welcome_message` | Welcome to dc24h.eu. Guest access is enabled. | Nonempty UTF-8/NFC text, at most 4096 bytes when enabled |
| `max_messages_per_second` | 5 | 1–1000000 |
| `message_burst` | 10 | 1–1000000 |
| `max_bytes_per_second` | 32768 | 1–16777216 |
| `byte_burst` | 131072 | 16384–16777216 |

Defaults preserve previous behavior. The four Anti-Flood key names/ranges remain unchanged; see [Anti-Flood](ANTI_FLOOD.md). In C++, Network options now expose `chat::Settings`, with the former flat flood fields under `.chat.flood`, client policy under `.chat.policy`, and explicit welcome fields.

```ini
[chat]
max_text_bytes = 16384
public_enabled = on
private_enabled = on
actions_enabled = on
welcome_enabled = on
welcome_message = Welcome to dc24h.eu. Guest access is enabled.
max_messages_per_second = 5
message_burst = 10
max_bytes_per_second = 32768
byte_burst = 131072
```

Keys are case sensitive, singular and reject duplicates. Booleans accept only lowercase `on` and `off`; `true`, `1` and uppercase variants are invalid. The existing INI parser trims surrounding value whitespace and retains internal spaces. Welcome text is a literal single line in INI: quotes, backslashes, `\n`, variable references and PM/ME-like text have no special expansion. For example, `\n` displays as a literal backslash followed by n. There is no template or command interpreter.

An empty welcome is permitted only with `welcome_enabled=off`. Other nonempty supplied welcome values are checked even while disabled, including length, UTF-8/NFC and control policy. `chat::validate_settings`, `validate_policy` and `validate_welcome` reject invalid configuration before listeners start; invalid input is not silently repaired or truncated. The shipped example and US/PL help include the defaults.

## Client policy and diagnostics

The configured text ceiling counts decoded UTF-8 bytes, not code points, graphemes, headers or flag bytes. An emoji encoded in four bytes fits a four-byte limit; adding an ASCII character exceeds it. Whitespace is preserved. The existing full encoded ADC body limit of 16384 bytes plus final LF remains separate and cannot be raised through configuration; escape expansion and metadata still count toward that hard bound.

Both direct public/private adapters and MessageRouter use the configured policy in shared `validate_chat`. Complete text/SID/flag validation and ready sender/source ownership remain first. The configured length check then precedes channel/action permission. A legal message exceeding the configured text ceiling gets typed `text_limit`; a disabled operation gets `disabled`. The policy applies to all admitted clients, without trusting CT or inventing operator/account exceptions.

| Attempt | Response / delivery |
| --- | --- |
| Valid B/D/E text above configured ceiling | Recoverable `ISTA 100 Chat\stext\slimit\sexceeded FC<FOURCC>` |
| Disabled public, private or ME1 operation | Recoverable `ISTA 125 Chat\soperation\sis\sdisabled FC<FOURCC>` |
| ME0 with actions disabled | Plain message remains allowed if its channel is enabled |
| ME01/other valid integer spelling for action one | Treated as ME1 and refused when actions are disabled |
| Invalid syntax, source, state or known fields | Existing parser/state/fatal behavior, not a configuration refusal |

Codes 100 and 125 use recoverable severity with generic/access-denied categories from [ADC STA](https://adc.sourceforge.io/ADC.html). Fixed descriptions do not include client text or configured welcome contents. Refusal publishes no public/private packet or EMSG echo and leaves the session ready. Subsequent allowed messages can proceed. Client-mode switches do not disable user-list INF/QUI, keepalives or trusted system IMSG. No PM intent is converted to broadcast, and actions are refused rather than silently downgraded.

Anti-Flood remains enabled on actual Network connections and charges eligible B/D/E attempts before policy/refusal and routing. Repeated policy failures therefore do not bypass the existing count/byte budget or output bounds. Recipient readiness, immutable lifetime/domain plans, FIFO and dispatch-time validation remain authoritative. Low-level trusted Core/system APIs remain separate from client policy.

## Welcome and ownership

An enabled custom greeting is validated once at construction and stored in one shared immutable IMSG packet. It follows own BINF, is included in the aggregate preflighted welcome budget, and contains one escaped text parameter without source SID or PM/ME flags. It is independent of `max_text_bytes` and client channel/action switches. Own-BINF final LF still marks readiness exactly once; consuming IMSG does not repeat it.

With the welcome disabled there is no greeting frame or byte charge, and the own-BINF completion still marks readiness. The initial user-list algorithm and presence updates remain intact. The internal LoginLimits API carries `chat::Policy`, optional transport anti-flood limits and optional owning `welcome_text`; a null welcome means disabled. The legacy `adc::hub_welcome_text` constant remains an alias of the default text.

Configured greetings may be Polish or other valid NFC UTF-8 text; there is no per-client language negotiation. Operator `--language` does not rewrite wire text. Arbitrary welcome text remains absent from logs/API exports, though it is intentionally visible to joining clients. No new persistence, database migration, API field, dependency, moderation, role model or administration endpoint is added.

## Verification

All **88/88 CTest suites** passed locally in Debug, Release and ASan/UBSan with strict integration-tool detection and default leak detection. This includes configured-chat unit/process coverage, all earlier chat/security/queue regressions, actual ncdc ADC/ADCS with custom welcome, MariaDB and Apache HTTPS. WebAdmin typecheck/production build, 4 Node tests, 3 Chromium tests and real C++ API integration passed. Metadata, documentation links, shipped INI and US/PL help checks passed. Results are local; remote CI, the full OS/client matrix and production-load guarantees require separate validation.

`chat.configuration` covers preserved defaults, settings bounds, disabled/empty/invalid welcomes, UTF-8 byte ceilings, direct adapter policy, independent public/private switches, ME0/action alias handling, immutable policy copies, invalid-source precedence, recoverable runtime refusals, optional/custom welcome, system-message independence and queue cleanup.

`chat.configuration.process` tests both source transports for all channel/action switches, no-welcome login, exact allowed recipient/echo matrices, ordered INF fences proving no leaked packets, configured Unicode/whitespace boundaries, custom literal welcome, recovery, fatal spoof rejection and log/API privacy. Configuration unit tests cover all new keys, strict booleans, duplicates, ranges, literal INI text, empty-disabled welcome and PL error catalogs. Actual ncdc ADC/ADCS displays a configured greeting while retaining chat/action/flags/reconnect checks. Existing Anti-Flood and generic validation suites remain enabled.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(chat.configuration|configuration|adc.ncdc|adcs.ncdc)' --output-on-failure
```

See [CHANGELOG.md](../../CHANGELOG.md) for validation results. Remote CI, the full OS/client matrix and production-load guarantees require separate evidence.
