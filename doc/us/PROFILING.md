# Profiling — alpha-0.9.13

Bottleneck detection for alpha-0.8. The branch starts from main `ac87fa2`. The branch name is `alpha-0.9.13`.

The profile runs in process. It times three fixed stages and names the slowest one. There is no new route, INI key, schema migration, Apache setting, CLI flag, WebAdmin page or status field. The hub executable does not link this library and does not run it at startup. This is not a sampling profiler, a flame graph, or a trace of a live hub.

## Stages

`permissions` calls `allows` 256 times. The role and the permission walk the existing catalogs. The policy is the default empty VIP mask.

`moderation` calls `moderate` 512 times. The operation walks its catalog. The actor is five steps ahead in the role catalog and the target role walks that catalog from the start. The target subject is the subject that operation already requires. Account and CID subjects are marked account-bound. The actor is not the target and is not treated as the creator of a ban record.

`decisions` calls 1024 existing role predicates, cycling `can_mute`, `can_revoke`, `can_manage` and `can_assign`. `can_revoke` is the kick rank check. None of these calls dispatches a kick, a mute, a ban or a role change.

`accepted` is the number of decisions the current functions allow. It is not a new permission and it does not choose the bottleneck. `configuration.manage` and `hub.manage` stay unimplemented. Browser kick and mute are still not dispatched.

## Ranking

The bottleneck is the stage with the greatest `elapsed_ms`. Equal elapsed time uses the greater operation count, so a sample that finishes inside one millisecond still has a stable rank. A remaining tie keeps the earlier stage. Zero means the stage finished inside one millisecond. The rank is not host CPU, not a percentage, and not a stored history.

## Limits

`profile.unit` checks the ranking rules directly, including a case where a higher elapsed time beats a larger operation count and a case where the accepted count does not change the rank. It then runs the fixed stages, requires each accepted count to match a second pass, requires at least one acceptance, and requires each stage to finish within five seconds. The five-second ceiling is a hang guard. It is not a capacity claim, a service target, or a load test.

The profile does not open a socket, read a configuration file, write a result file, or talk to MariaDB. Schema v9 is unchanged. `Require local` stays. Status and health stay bearer-only. This does not close a residual threat.
