# ADC search and peer connection signaling

[Polski (pl-PL)](../../pl/alpha-0.1/SEARCH-P2P.md)

E4 routes file searches, results, and requests to open a peer connection. ADC and ADCS share the same online users. The hub control strand reads capabilities from each current public `INF` profile and routes validated frames through the existing bounded output queues. File contents and the client-to-client handshake travel on the peer connection.

## Search and results

An admitted ADC session in `NORMAL` can send `SCH`. `BSCH` goes to all admitted users, including the sender. `FSCH` reaches users whose `SU` contains every `+` feature and none of the `-` features. `DSCH` goes to its target, and `ESCH` also echoes to its sender.

The hub validates inclusion (`AN`), exclusion (`NO`), extension (`EX`), size bounds (`GE`, `LE`, `EQ`), type (`TY`), and TIGR hash (`TR`). Searches can repeat text terms. An optional `TO` identifies the search and is copied to its results. Unrecognized fields with valid wire syntax can accompany a supported filter.

Clients send hub-routed results as `DRES` or `ERES`. A result includes `FN` and `SI`, plus `TR` for files. An optional `SL` reports free slots. The hub accepts results only for the requester and from an online responder to whom it queued the search. `DRES` goes to the requester. `ERES` also echoes to the sender. A requester that omitted `TO` can receive results without `TO` within the same bounded search window. Clients with a usable advertised UDP address and port can also exchange `URES` directly. [ADC 1.0.4, §§3.3 and 5.3.6–5.3.7](https://adc.sourceforge.io/ADC.html) · [ADC Extensions, TIGR](https://adc.sourceforge.io/ADC-EXT.html)

A search from `AAAB`, filtered to IPv4 TCP-active peers, can receive a directed reply from `AAAC`.

```text
FSCH AAAB +TCP4 ANrelease ANnotes TOquery-1
DRES AAAC AAAB FN/Docs/release\snotes.txt SI37 SL1 TRW6AIUW3CLDF6OGHNVE4JPDDJ2P74IWRCF2O36TA TOquery-1
```

## Active and passive peers

A user is TCP-active for routing when its profile advertises `TCP4` with an `I4` or `TCP6` with an `I6` from its established hub socket. An active user can send `DCTM <from> <to> <protocol> <port> <token>`. The other peer connects to the advertised address and port. A passive user sends `DRCM <from> <to> <protocol> <token>` to an active peer, which answers with `DCTM` using the same protocol and token. The echo types `ECTM` and `ERCM` also return the frame to the sender. The hub accepts reverse requests addressed to an active peer, including fallback requests from active users.

```text
DRCM AAAB AAAC ADC/1.0 connect-1
DCTM AAAC AAAB ADC/1.0 41300 connect-1
```

Here `AAAB` opens a TCP connection to `AAAC` on port `41300` and supplies `TOconnect-1` in its client-to-client `CINF`. Clients own the token and use it to associate the peer connection with the request. `ADC/1.0` signals plaintext TCP. `ADCS/1.0` and the legacy `ADCS/0.10` signal TLS when both peers advertise compatible support. The hub publishes syntactically valid `KP` SHA-256 keyprints so clients can verify peer certificates. The hub listener's TLS mode and the peer connection's TLS mode are independent. [ADC 1.0.4, §§5.2.2 and 5.3.8–5.3.9](https://adc.sourceforge.io/ADC.html) · [ADC Extensions, KEYP and ADCS](https://adc.sourceforge.io/ADC-EXT.html)

## Limits in Alpha 0.1

- Sender SIDs are bound to the socket. The hub rejects malformed ports and tokens, offline targets, and results for searches the responder did not receive. Opaque tokens are limited to 256 decoded bytes and cannot contain ASCII control characters. Routing errors go only to the sender.
- A delivered search authorizes up to five routed results per responder for a UDP-passive requester or ten for a UDP-active requester. Authorization lasts 60 seconds per responder. Searches that reuse a token, or omit it, share a bounded result allowance. Each requester can use up to 16 simultaneous token slots, including the tokenless slot.
- A reverse request is tracked for 60 seconds. While one is pending for the same pair, an answering `CTM` must match its protocol and token. With no pending request, an active user can initiate a standalone `CTM`.
- Search and connection signaling each start with eight requests per client. Searches regain one request per second and signaling regains four per second. Results have their own rate limit and all forwarding uses the existing output limits. Disconnecting removes related state and prevents a reused SID from inheriting it.
- Search matching, direct UDP, connection establishment, and file transfer are client responsibilities. E4 signaling covers the protocols above. NAT traversal extensions and relay service are outside Alpha 0.1 E4. Two passive peers need an active endpoint to establish a direct connection.

## Local verification

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug -R '^(AdcProtocol|PeerSignaling|AdcPeerRouting)\.'
ctest --preset debug
```

The integration tests use ephemeral loopback ports and temporary TLS certificates. They test search and reverse signaling across ADC and ADCS, then verify a client-to-client connection on another socket. The suite also checks isolation after spoofing, incorrect tokens, and disconnects.

To reproduce the client check, start a loopback hub with `--enable-adc --adc-port 0` and open two independent ncdc profiles with distinct nicknames and download directories. Share a small local test file from each profile. In one profile, run `/set active_ip 127.0.0.1`, `/set active_port 0`, and `/set active true`. Leave the other passive. For the ADC test, use `/set tls_policy disabled` in both profiles. Connect both profiles, use `/search <name>` on each, and press `d` on the search result. Compare the downloaded bytes with the file from the other profile. Repeat with ADCS and `/set tls_policy force` on both profiles to test TLS between peers. [Loopback hub setup](TCP-ADCS.md)
