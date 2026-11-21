# User presence and chat

[Polski (pl-PL)](../../pl/alpha-0.1/PRESENCE-CHAT.md)

E3 shares one roster between the ADC and ADCS listeners. The server control strand owns the roster and orders admissions, updates, chat routing, and departures. The roster holds each admitted client's SID, public profile, nickname comparison key, advertised features, and rate limit state. The connection registry associates each SID with its socket session. Each socket strand handles framing, TLS, ADC state, deadlines, and writes.

## Admission and presence

1. The hub negotiates `BASE` and `TIGR`, assigns an SID, and sends its `IINF`. The client sends its initial `BINF` in `IDENTIFY`. The E1 session checks the SID and the CID against the TIGR hash of the PID.
2. The control strand validates the public fields and checks that the nickname and CID are unique among online users. A failed admission produces a fatal `ISTA` for that connection.
3. The hub queues the current members' `BINF` frames followed by the joining client's own `BINF`. The client's session enters `NORMAL`, and the hub announces the join to established members. Socket queues preserve the order of later joins, updates, and quits.

The decimal CLI option `--max-nick-codepoints` defaults to 32 and accepts values from 1 to 256. E1 requires valid UTF-8 in NFC. A nickname must be nonempty and contain code points above U+0020, excluding Unicode control, format, and separator categories. The hub preserves its spelling and compares a full Unicode case-folded NFC key, so `Straße` and `STRASSE` conflict. Comparison is locale-independent and does not identify visually similar letters across scripts.

In `NORMAL`, a client's `BINF` updates its own profile. A field with no value clears an optional attribute. The CID stays fixed and a replacement nickname must pass the same checks as at login. The hub publishes only changed public fields, and a rejected update leaves the previous profile in place. The stored public profile and the update each fit within `--input-bytes`.

The public fields are `ID`, `NI`, `DE`, `VE`, `AP`, `AW`, `SU`, `I4`, `I6`, `U4`, `U6`, `SS`, `SF`, `SL`, `HN`, `HR`, `HO`, `US`, `DS`, `AS`, and `AM`. The socket strand removes the PID and other private fields before passing the profile to the roster. Unknown fields and client-supplied role flags are omitted. An advertised `I4` or `I6` value comes from the TCP peer address of the matching family.

The hub sends `IQUI <sid>` to the other online clients once the session closes and removes the member and SID slot once. TCP loss, TLS close, timeouts, and fatal protocol or transport errors use the same cleanup path. A client can leave by closing its connection or sending a fatal `HSTA`. On graceful server shutdown, each established client receives its own `IQUI` as the final frame when the socket can drain. A fresh connection negotiates an SID again and can reuse the released nickname and CID. [ADC 1.0.4, §§5.1 and 5.3](https://adc.sourceforge.io/ADC.html)

## Chat and routing

Clients send `MSG` in `NORMAL`. The sender must supply the SID assigned to its socket. The hub validates the decoded text and flags and escapes the text for forwarding. `ME1` requests third-person display.

| Frame | Recipients and requirements |
| --- | --- |
| `BMSG <from> <text>` | All online users, including the sender. |
| `FMSG <from> <selectors> <text>` | Online users whose `SU` contains every required (`+`) feature and none of the excluded (`-`) features. |
| `DMSG <from> <to> <text> PM<from>` | The online recipient only. |
| `EMSG <from> <to> <text> PM<from>` | The online recipient and the sender. A message to oneself is sent once. |

One-to-one private chat requires `PM<from>` on a directed frame. The hub rejects any `PM` flag on public or feature broadcasts. A missing or unavailable recipient, a spoofed SID, or invalid flags produce a recoverable status for the sender. The status contains neither message text nor the destination SID. Server logs omit wire payloads. [ADC 1.0.4, §§3.3 and 5.3.5](https://adc.sourceforge.io/ADC.html)

## Limits

- `--max-message-bytes` defaults to 1,024 decoded UTF-8 bytes and accepts values from 1 to 4,096. E1 also limits the escaped parameter and the full ADC frame.
- Each session has eight tokens for chat and post-login `INF` updates. It regains one token every 250 ms, up to eight. Excess commands receive a recoverable status. The socket strand keeps at most one input awaiting the control strand.
- The `--pending-bytes` budget includes posted frames, the socket queue, and the active write. Recipients share immutable bytes for each broadcast. An overflowing recipient is disconnected. The initial roster must fit the new connection's available output budget for admission to succeed.
- The stage supports guest sessions and one-to-one private chat. ADCS encrypts the connection to the hub. Group chat, persistent accounts, search, and peer-to-peer signaling are outside this stage.

## Test with ten clients

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug -R '^AdcPresence\.TenAdcsClientsTrackPresenceAndRouteChat$'
ctest --preset debug -R '^(UserPresence|AdcPresence)\.'
```

CTest generates temporary TLS credentials and runs ten independent clients on an ephemeral loopback port. The integration test checks the roster at every client, public chat, private delivery with and without a sender echo, `INF` changes, a TCP reset, one `IQUI`, a reconnect with a fresh SID, and an empty roster at the end. The other E3 tests cover invalid input, concurrent nickname conflicts, and output limits. [TCP and ADCS setup](TCP-ADCS.md)
