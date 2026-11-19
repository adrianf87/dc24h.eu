# WebAdmin Users

alpha-0.8.12 adds a read-only directory of ready hub sessions and a bilingual WebAdmin page for moderation decisions. Create the branch from main. Do not merge it automatically.

`GET /api/v1/users` is bearer-only, like status and health. It does not require `X-DC24H-Admin-Grant`. The maintenance process does not publish this route. Schema v9 is unchanged.

The reactor copies at most 64 ready sessions into a plain directory before the API thread reads that copy. Session models stay on the reactor thread. The sample follows the reactor loop, so a login can appear on the next loop rather than in the same instant as the welcome. A later ready session, a SID that is not four ADC characters, or a nickname outside 1..64 bytes without controls sets `truncated` and is omitted.

Each row has `session`, `sid`, `nickname`, `transport` (`adc` or `adcs`), `role`, `public_muted` and `private_muted`. `session` is the process-local lifetime id as a decimal string. The role is `guest` until the session is authenticated. The directory does not contain a CID, IP address, account id, email, description, shared size, password, proof or kick reason. Those fields also stay out of `GET /api/v1/status`.

The Users page lists the rows and evaluates kick, mute and unmute with the same lower-role rule as Moderation Rules, for a role selected in the browser. The decision does not know whether that role owns the row. The page shows the client command text and does not send it. `+kick`, `+mute` and `+unmute` still run from an authenticated Direct Connect session. Account, CID and IP bans stay on the Bans page. An older API that omits the route shows the directory as unavailable instead of an empty list.

No production deployment, schema migration or account mutation is part of this increment.
