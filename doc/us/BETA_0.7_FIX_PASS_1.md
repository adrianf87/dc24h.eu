# Beta 0.7.x — Fix Pass 1: external ADC/ADCS interoperability (0.7.1–0.7.4)

**Disposition: code review and targeted regressions completed locally; new exact-head GitHub CI pending. NO-GO for merge/deployment.** This pass applies only to Stage I on the existing `beta-0.7.x-stage-full` branch and draft [PR #239](https://github.com/adrianf87/dc24h.eu-high-starter/pull/239). It is Fix Pass **1 of 5** under [Issue #238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238). No other fix pass is started here.

## Confirmed defect and repair

`scripts/beta07_client_evidence.py` previously accepted any syntactically valid 40-hex source SHA. The report generator compared neither that SHA to the actual checked-out commit nor its declared image to the hosted runner identity. Consequently, a locally supplied or stale JUnit file and arbitrary SHA could have produced superficially valid independent-client provenance, despite an honest CTest pass requirement.

The repair requires the **actual `git rev-parse HEAD`** to match the input SHA *before* the client package or JUnit are read. The pure report evaluator also rejects mismatched/fake checkout SHA. Before generating evidence, the executable collector requires GitHub Actions on a `github-hosted` Linux runner with the expected Ubuntu 22.04, 24.04 or 26.04 `VERSION_ID`. Unit tests reject wrong head, privately managed runner, mismatched OS, missing/incorrect package digest and incomplete/malformed JUnit cases. This is locally verified consistency; environment variables and local files cannot independently authenticate a GitHub API record. The Owner must still correlate completed run/job/artifact provenance.

## Stage I review ledger

| Patch | Reviewed surfaces and negative coverage | Fix Pass 1 result / residual boundary |
| --- | --- | --- |
| **0.7.1** Independent client identity | `beta07_client_evidence.py`, `beta07_client_evidence_tests.py`, pinned ncdc Ubuntu package SHA-256 and installed executable SHA, `dpkg --verify`, 25 CTest cases | **Fixed source/runner provenance mismatch**. `ncdc` is the sole executed independent client. DC++ and AirDC++ remain explicitly `untested`, not certified. JUnit is generated locally by CTest; external Actions authenticity requires Owner correlation |
| **0.7.2** Identity/negotiation | `ncdc_login_test.py`, `ncdc_negotiation_test.py`, `adc_login_process_tests.py`, `adc.ncdc`/`adcs.ncdc` and `accounts.adc_authentication.process`; SUP/SID/INF, PID/CID, Unicode, MFA/password challenge, spoof, duplicate and pre-auth refusal | Existing positive and refusal cases retained; no new protocol defect confirmed. Real synthetic client evidence is limited to the ncdc fixture and the C++ hub, not all third-party implementations |
| **0.7.3** Search/transfer/fairness | `ncdc_login_test.py`, `ncdc_ports.py`, `ncdc_ports_tests.py`; TTH/FSCH/SCH/RES, CTM/RCM, active/passive, token scope, port collision, guest search/transfer denials, reconnect and rate limits | Existing real ncdc and C++ regressions retained. Hosted-client active/UDP bind tests use isolated loopback IPv4; separate process cases cover applicable IPv6 paths. No equivalence or unsupported capability claim |
| **0.7.4** ADCS trust/downgrade | `tls_security_process_tests.py`, `tls_support.py`, ncdc ADCS CTest, TLS 1.2/1.3 hostname trust, expired/wrong host, mismatched key, stale trust, legacy cipher and rotation refusals | Existing validating-client TLS checks retained; no policy relaxation. Synthetic ncdc peer-to-peer certificates are explicitly **not** proof of a public CA/hostname-verified cross-client deployment |

## Required acceptance and stop condition

- The pre-pass source head `abdd0d27665523a69bfd134e2cdc7998a942c092` had completed-success Actions runs: [C++/WebAdmin](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45449048235), [isolated Ubuntu VM](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45449048397) and [CodeQL](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45449048232). These **do not certify the new Fix Pass 1 commit**.
- For this new head, require successful GitHub-hosted Ubuntu 22.04/24.04/26.04 C++ Debug/Release, ncdc/ADC/ADCS/TLS, WebAdmin/Chromium, VM/systemd, ASan/UBSan, CodeQL, matched recovery, disposable dpkg and three-transport benchmark gates. Every required job and approved de-identified artifact must match the exact post-fix SHA. Pending/skipped/failing or mismatched jobs block acceptance.
- Performance windows are short synthetic regression envelopes, not capacity or production SLA. Keep MariaDB v11/five-store consistency, C++ password/grant authority and private-only WebAdmin boundaries. Do not merge, mark PR ready, deploy or claim the fix pass accepted on a CI trigger alone.

After committing/pushing this pass, record the **actual remote SHA** in the PR handoff and **stop without polling** for CI results. Fix Pass 2 requires a separate Owner request.
