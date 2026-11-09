# Versions

| Version | Branch | Scope |
| --- | --- | --- |
| alpha-0.1.1 | alpha-0.1.1 | Core |
| alpha-0.1.2 | alpha-0.1.2 | Network |
| alpha-0.1.3 | alpha-0.1.3 | ADC parser |
| alpha-0.1.4 | alpha-0.1.4 | ADCS/TLS; reviewed chatgpt-alpha-0.1.4 history retained |

CMake PROJECT_VERSION is 0.1.4 and the generated runtime version is alpha-0.1.4. Identify an individual build with the version plus its Git commit SHA; compiling does not modify tracked source. Each significant change updates CHANGELOG.md. Alpha denotes development status, not complete hub functionality. Tags and production deployment are not created automatically.

The first three stage branches and their merges already exist on main. The new alpha-0.1.4 branch starts from that main, retains the reference TLS branch through a merge, and contains the follow-up fixes. Main and historical stage branches are preserved.
