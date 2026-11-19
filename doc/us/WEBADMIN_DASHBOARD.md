# WebAdmin Dashboard — alpha-0.8.11

This increment adds the main WebAdmin panel and a fixed statistics summary. It starts from main `9f72152`. There is no automatic merge. The branch name is `alpha-0.8.11`.

Schema v9 is unchanged. The summary is not stored in MariaDB. It does not append an audit row, create a ban, kick, mute or change an account. `GET /api/v1/status` and `GET /api/v1/health` stay bearer-only. A proved administrative grant is not required and is not read by this object. Configuration writes and live hub administration remain pending.

## Status object

`dashboard` is added after `database` and before the optional `configuration` object. `schema` is the number `1`. `core_running` and `database_enabled` repeat the booleans already on the snapshot. Every other field is a decimal string of one existing `uint64` counter, with no leading zeros. The strings keep values above the JavaScript safe integer range. The object does not sum counters, compute rates or read the clock.

| Field | Source |
| --- | --- |
| `uptime_seconds` | steady-clock uptime already on `server` |
| `users_online`, `users_peak`, `users_logins` | online, peak online, successful logins |
| `connections_active`, `connections_peak`, `connections_accepted`, `connections_rejected`, `connections_closed` | the same network counters |
| `security_rejected`, `security_closed` | admission-limit counters |
| `adc_frames`, `adcs_handshakes` | accepted ADC frames and completed ADCS handshakes |
| `bytes_received`, `bytes_sent` | transport byte counters |
| `chat_received`, `chat_public`, `chat_private`, `chat_rejected`, `chat_flood_limited` | chat attempts and those four outcomes |
| `search_queries`, `search_routed`, `search_rejected`, `search_results` | queries received, routed and rejected, and results received |
| `search_tracked` | stored search-token windows. This is a gauge, not a cumulative event count |
| `transfers_ctm`, `transfers_rcm` | CTM and RCM requests received |
| `transfers_peer_unsupported`, `transfers_peer_failed` | peer protocol and connection reports |
| `kicks_succeeded`, `kicks_refused` | kick outcomes |
| `mutes_applied`, `mutes_public_blocked`, `mutes_private_blocked` | mute applications and blocked public or private messages |

Chat queue copies, search delivery copies, transfer queue copies, kick attempts and mute attempts stay on their existing objects. They are not copied again here. No nickname, CID, IP address, message, reason, password or path is included.

Counters are independent samples, as on the rest of the status snapshot. They reset when the process restarts. A replacement hub does not inherit them.

## WebAdmin

`#dashboard` is the first monitoring page. The default route remains `#overview`. The page is read-only and uses the same manual refresh and five-second poll as the other monitoring pages. EN/US and Polish labels use `en-US` and `pl-PL` grouping. A full `uint64` kick or mute counter is formatted from the decimal string.

Each group links to the existing detail page: users to Overview, network and transfers to Connections, chat to Chat, search to Search, and moderation to Kick. Those pages keep their own counters.

If `dashboard` is absent, this page says the summary is unavailable and does not invent zeros. The other pages still render the fields they already understand. If `dashboard` is present but malformed, or if any copied field disagrees with its source field, the whole snapshot is rejected. A counter that is already too large for a safe JavaScript number in `network`, `users`, `chat`, `search` or `transfers` still makes the snapshot unavailable; the decimal string does not bypass that existing check. Unknown fields are dropped and are not exported.

The panel does not list accounts, bans or audit events, and it does not send a maintenance request. Browser-local search and transfer observations stay on their own pages and are not part of this summary.
