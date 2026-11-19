# Stress testing — alpha-0.9.16

Overload and sustained operation. The branch starts from main `cdc509a`. The branch name is `alpha-0.9.16`.

The soak runs in process. It repeats the existing frame limiter, peer limiter, login budget, chat flood guard and session table for 24 rounds. There is no new route, INI key, schema migration, Apache setting, CLI flag, WebAdmin page or status field. The hub executable does not link this library and does not run it at startup. This is not a capacity claim, a load test, or a DDoS test.

## Rounds

Each round uses a synthetic clock that only moves forward. The soak does not sleep and does not open a socket.

- The frame limiter accepts 4 frames in one second and refuses the next. The following round starts one second later, so the window opens again.
- The peer limiter accepts 2 connections for one fixed key, refuses the next, releases those two, accepts the one admission left in the minute budget, and refuses the next. It then moves one minute forward and prunes the idle key. An empty key is refused and is only used to run that prune. The key is not a connection and is not logged.
- A fresh login budget accepts 4 frames and then refuses twice. The second refusal stays refused, because a failed negotiation stays failed.
- The chat guard accepts a burst of 2 one-byte messages and refuses the next. Two seconds later the same guard can accept the burst again.
- The session table accepts 2 sessions, refuses the next with the existing capacity error, closes both, and removes them.

One frame limiter, one peer limiter, one chat guard and one session table live for all 24 rounds. After the last round the peer table is empty, no login is pending, and no session is open. Created sessions equal completed sessions.

## Limits

`stress.unit` checks those counts, checks that a second run records the same counts, checks that every limiter refused work, and checks that the soak finishes within five seconds. The five-second ceiling is a hang guard. It is not a capacity claim, a service target, or a load test. Zero milliseconds means the soak finished inside one millisecond.

The sample does not read a configuration file, write a result file, or talk to MariaDB. Schema v9 is unchanged. `Require local` stays. Status and health stay bearer-only. `configuration.manage` and `hub.manage` stay unimplemented. Browser kick and mute stay undispatched. This does not close a residual threat. GitHub Check status still runs Debug and Release only.
