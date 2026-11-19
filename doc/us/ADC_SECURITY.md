# ADC security — alpha-0.9.2

Parser validation, fuzzing and protocol control. This increment names the hub command profile the connection state machine already enforced and calls that one function before state and sequence checks. The branch starts from main `6c26839`. The branch name is `alpha-0.9.2`.

The wire set of accepted frames is unchanged. Unknown extensions still parse. GET, GFI and SND stay unsupported commands, not syntax errors. This is not a new rejection class, not coverage-guided fuzzing, and not a bandwidth cap.

## Command profile

`control_message` in `dc24h_adc` returns `ok`, `unsupported_command` or `invalid_type`. It reads direction, message type and command. It does not read parameters, features or text, and it does not allocate.

`ConnectionStateMachine::observe` calls it after the command name matches the kind and before state, source and sequence checks. A disconnected session still returns `disconnected` first. A hand-built message whose kind does not match its command name still returns `invalid_message` before the profile.

| Command | Profile |
| --- | --- |
| Unknown, GET, GFI, SND | `unsupported_command` for every type and both directions, before the type check |
| SUP | Incoming hub, or outgoing info |
| PAS | Incoming hub only. Every outgoing PAS is `invalid_type` |
| SID, GPA, QUI | Outgoing info only |
| INF | Incoming broadcast. Outgoing info or broadcast |
| STA, MSG | Broadcast, direct, echo, feature, plus incoming hub or outgoing info |
| SCH, RES, CTM, RCM | Broadcast, feature, direct or echo |

Argument and text syntax stay in the [parser](ADC_PROTOCOL.md). The parser still accepts unknown extensions and hub-legal frames, including ISID and GET. The serializer still re-parses hub frames. An unknown command still reaches the existing login path and can produce the existing unsupported status. It is not reclassified as an invalid frame.

Outgoing PAS was already rejected. The profile now reports `invalid_type` before the state check, including when the state would also have been wrong. Accepted frames do not change. Routed STA outside NORMAL remains a state-machine `invalid_type` after the profile. State, sequence and source checks stay in the [state machine](ADC_STATE.md).

## Fuzz campaign

`adc.security` is a deterministic in-process CTest. It does not use libFuzzer and it does not measure coverage.

- Generator: `std::mt19937`, seed `0x0ADC0922`. The same seed drives chunk sizes. The test asserts the seed.
- Exactly 4096 mutations. The test asserts that count, so the campaign cannot shrink without a failing build.
- Each mutation starts from a fixed BASE frame: HSUP, ISUP, ISID, BINF, HMSG, BMSG, DMSG, EMSG, FSCH, ISTA, IQUI, HPAS, IGPA, BSTA, BSCH, DRES, DCTM, DRCM, HZ99 or HGET. The edit flips a byte, inserts a byte, erases a byte, duplicates a prefix or appends LF. The buffer stops at `max_frame_bytes + 64`.
- No exception escapes. After every chunk the stream decoder buffer is at most `max_frame_bytes`.
- A line-split oracle matches the decoder, including an oversize frame without LF. That input already emitted `frame_too_long`, and `finish()` is empty rather than `truncated_frame`. A short frame without LF is `truncated_frame`.
- Every `Message` produced by `parse_line` round-trips through `serialize` to the same fields.
- On a fresh machine, `control_message` and `observe` agree on `unsupported_command` and on profile `invalid_type`.

The 8 message types, 16 commands and both directions are also checked directly. Parameters do not change the profile result.

The campaign does not prove the absence of parser bugs. It does not close the alpha-0.9.1 residual threats T01–T16. T01 remains spoofed identity outside this gate. T13 remains resource exhaustion. Existing `adc.parser` and `adc.syntax` tests stay in place. See [Testing](TESTING.md) and [Security](SECURITY.md).

## Unchanged

Schema v9 is unchanged. No status counter, WebAdmin page, language string or Apache setting is added. Check status stays on GitHub-hosted Ubuntu 24.04. HTTP/2, OCSP, automatic renewal and a public authenticated gateway remain unimplemented. `configuration.manage`, `hub.manage` and browser kick or mute stay unimplemented.
