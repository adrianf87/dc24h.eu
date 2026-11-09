# ADC syntax parser

The reference is [ADC 1.0.4](https://dc-protocols.github.io/ADC.html), section 3. This milestone implements syntax parsing, not the complete client/session protocol.

`parse_line(string_view)` accepts a frame body without LF and returns `KeepAlive`, `Message` or `ParseError`. The result owns its strings. Messages retain type, original command, optional source/target, feature selectors and decoded parameters. `recognize_command` recognizes STA, SUP, SID, INF, MSG, SCH, RES, CTM, RCM, GPA, PAS, QUI, GET, GFI and SND. Valid extension commands remain `unknown` messages.

`StreamDecoder::feed(bytes, sink)` synchronously delivers zero or more results in order. Do not reenter the same decoder from its sink; sink exceptions propagate. `finish()` reports an unfinished frame once and resets the decoder. Oversized frames already counted as invalid are not counted again at EOF. Instances are single-threaded; separate instances can run independently.

Headers B/C/D/E/F/H/I/U, routing fields, base32 SID/CID syntax and feature selectors are validated. CID length is not tied to a particular hash. Parameters use one ASCII space; escapes are `\s`, `\n` and `\\`. Empty LF is keepalive. utf8proc validates UTF-8 and NFC; non-normalized input is rejected rather than rewritten.

Local limits (application policy, not ADC requirements): 16384 body bytes, 128 parameters excluding routing, no raw ASCII controls/DEL including NUL, tab and CR. CRLF is rejected. An invalid frame produces one error and parsing resumes at the next LF. Oversize input is discarded until LF with bounded memory. Every TCP/TLS client has an independent decoder; results are counted and released without dispatch or queues.

Shutdown counters: `frames` counts valid nonempty frames; `unknown_commands` is their unknown-command subset; `invalid_frames` includes malformed, oversized and truncated input; `keepalives` counts empty LF. No parameters, PID or PAS hashes enter logs.

No serializer, semantic field validation, command/type permission matrix, SID allocation, session state machine, authentication, routing, chat, search or transfer coordination exists. U headers can be parsed but there is no UDP listener. ncdc tests cover initial HSUP only: no ISUP/ISID response or NORMAL session. TLS transport is described in [ADCS_TLS.md](ADCS_TLS.md).

## Security integration — alpha-0.1.7

The parser library retains its 16 KiB/128-parameter limits and resynchronization contract. `StreamDecoder::in_progress()` reports both buffered partial input and oversized discard state for absolute timeout tracking. The server adds per-connection result-rate and invalid-lifetime budgets, and may close abusive peers. It feeds one LF-delimited segment at a time and stops parsing the rest of a read when a quota is breached. The crossing rate result is not counted in ADC statistics; received_bytes can include unparsed remainder already read from the socket. Final truncated EOF is counted once during close. See [SECURITY.md](SECURITY.md).
