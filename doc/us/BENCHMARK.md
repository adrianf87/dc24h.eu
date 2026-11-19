# Benchmark — alpha-0.9.12

Performance measurements of alpha-0.8. The branch starts from main `76a093f`. The branch name is `alpha-0.9.12`.

The measurement runs in process. It calls the existing permission and moderation functions on a fixed input sequence and records how many decisions those functions accept and how many whole milliseconds the sequence took. There is no new route, INI key, schema migration, Apache setting, CLI flag, WebAdmin page or status field. The hub executable does not link this library and does not run it at startup.

## Work

`permissions` calls `allows` 1024 times. The role and the permission come from the existing catalogs, walking both in order and wrapping. The policy is the default empty VIP mask.

`moderation` calls `moderate` 1024 times. The operation walks its catalog. The actor is five steps ahead in the role catalog and the target role walks that catalog from the start. The target subject is the subject that operation already requires. Account and CID subjects are marked account-bound. The actor is not the target and is not treated as the creator of a ban record. Refusals stay in the sample. They are not turned into client messages or audit rows.

`accepted` is the number of decisions the current functions allow. It is not a new permission. `configuration.manage` and `hub.manage` stay unimplemented. Browser kick and mute are still not dispatched.

`elapsed_ms` is steady-clock time around that sequence only, in whole milliseconds. Zero means the sequence finished inside one millisecond. It is not host CPU, not a rate, and not a stored history.

## Limits

`benchmark.unit` checks that both sequences keep their operation counts, that the accepted counts match a second pass of the same inputs, that at least one decision is accepted, and that each sequence finishes within five seconds. The five-second ceiling is a hang guard. It is not a capacity claim, a service target, or a load test.

The sample does not open a socket, read a configuration file, write a result file, or talk to MariaDB. Schema v9 is unchanged. `Require local` stays. Status and health stay bearer-only. This does not close a residual threat.
