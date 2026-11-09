# Versions

| Version | Branch | Scope |
| --- | --- | --- |
| alpha-0.1.1 | alpha-0.1.1 | Core |
| alpha-0.1.2 | alpha-0.1.2 | Network |
| alpha-0.1.3 | alpha-0.1.3 | ADC parser |
| alpha-0.1.4 | alpha-0.1.4 | ADCS/TLS; reviewed chatgpt-alpha-0.1.4 history retained |
| alpha-0.1.5 | alpha-0.1.5 | Configuration: INI, CLI, paths and file logging |
| alpha-0.1.6 | alpha-0.1.6 | Logging: startup, connections, warnings and errors |
| alpha-0.1.7 | alpha-0.1.7 | Bounded IP admission, frame quotas and absolute deadlines |
| alpha-0.1.8 | alpha-0.1.8 | MariaDB connection and migration metadata baseline |
| alpha-0.1.9 | alpha-0.1.9 | Local read-only administrative HTTP/JSON API |
| alpha-0.1.10 | alpha-0.1.10 | React/TypeScript/Vite WebAdmin layout and local read-only adapter |
| alpha-0.1.11 | alpha-0.1.11 | Apache2 static HTTPS template and isolated TLS/access tests |
| alpha-0.2.1 | alpha-0.2.1 | Project: alpha-0.1 audit and alpha-0.2 preparation |
| alpha-0.2.2 | alpha-0.2.2 | ADC Parser: BASE arguments, named fields and bounded scalar syntax |

Identify an individual build with its version and Git commit SHA; compiling does not modify tracked source. Each significant change updates CHANGELOG.md. Alpha denotes development status, not complete hub functionality. Tags and production deployment are not created automatically.

Historical stage branches and commit history are preserved. alpha-0.1.4 retains the reference TLS branch through a merge and includes follow-up fixes.

## Current version — alpha-0.2.2

CMake PROJECT_VERSION and npm package/lock root versions are **0.2.2**; runtime/WebAdmin identify **alpha-0.2.2**. `project.metadata` enforces consistency. The parser stage starts from main after the audit merge (`ca464e0`); it does not merge itself into main or change the historical alpha-0.2 marker. Alpha-0.2.1 and alpha-0.2 audit branches remain preserved. See [parser contract](ADC_PROTOCOL.md) and [historical audit](AUDIT_ALPHA_0.1.md).
