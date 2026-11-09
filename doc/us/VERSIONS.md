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

Identify an individual build with its version and Git commit SHA; compiling does not modify tracked source. Each significant change updates CHANGELOG.md. Alpha denotes development status, not complete hub functionality. Tags and production deployment are not created automatically.

Historical stage branches and commit history are preserved. alpha-0.1.4 retains the reference TLS branch through a merge and includes follow-up fixes.

## alpha-0.2.1 Project

CMake PROJECT_VERSION is **0.2.1**, runtime and WebAdmin release are **alpha-0.2.1**, and both npm package/lock root versions are **0.2.1**. `project.metadata` enforces consistency. alpha-0.1.11 is merged into main; alpha-0.2.1 starts from that main. alpha-0.2 is a line marker at the same audit commit. Neither new branch changes main automatically. Proposed later alpha-0.2 versions in ROADMAP remain unimplemented. See [audit](AUDIT_ALPHA_0.1.md).
