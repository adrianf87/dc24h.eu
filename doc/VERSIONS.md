# Versions

| Version | Branch | Status |
| --- | --- | --- |
| alpha-0.1.1 | alpha-0.1.1 | Core implemented |
| alpha-0.1.2 | alpha-0.1.2 | TCP implemented |
| alpha-0.1.3 | alpha-0.1.3 | ADC parser implemented |
| alpha-0.1.4 | chatgpt-alpha-0.1.4 | ADCS inbound TLS development |

`PROJECT_VERSION` in CMake is `0.1.4`; `DC24H_VERSION` prepends `alpha-`. Builds print the version using `dc24h --version`. Each significant code change must update `CHANGELOG.md`. A build number is a source version plus its Git commit SHA; building does not mutate source files. No release tag is created automatically.

Existing Git history and the main branch are preserved. The TLS development branch starts from the main branch after merging alpha-0.1.3.
