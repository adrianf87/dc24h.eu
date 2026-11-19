# Dependencies — alpha-0.9.17

Library control and automatic security tests. The branch starts from main `6940992`. The branch name is `alpha-0.9.17`.

Configuration requires OpenSSL 3. The hub binaries must directly link the same five product libraries and no other product library. WebAdmin dependencies stay on exact versions from the npm registry. The existing security tests stay registered. There is no new route, INI key, schema migration, Apache setting, CLI flag, WebAdmin page or status field. This is not a vulnerability database and it does not run `npm audit`.

## Hub binaries

`dependencies.audit` reads the dynamic section of `dc24h`, `dc24h-owner` and `dc24h-account-admin`. Each one must need all of these libraries:

- `libssl.so.3`
- `libcrypto.so.3`
- `libgcrypt.so.20`
- `libmariadb.so.3`
- `libutf8proc.so.3`

The C++ runtime may also need `libc.so.6`, `libstdc++.so.6`, `libgcc_s.so.1` and `libm.so.6`. Any other shared library fails the test. The binaries must not have `RPATH` or `RUNPATH`.

An AddressSanitizer build may also need `libasan.so.8`. An UndefinedBehaviorSanitizer build may also need `libubsan.so.1`. A ThreadSanitizer build may also need `libtsan.so.2`. A normal build fails if one of those runtimes is present. The selected sanitizer runtime must be present in that build. The normal Debug and Release builds leave `DC24H_SANITIZER` empty.

## WebAdmin

`package.json` dependencies and devDependencies are exact `major.minor.patch` versions. They must match the root package in `package-lock.json`. Every locked package that has a download URL must use `https://registry.npmjs.org/` and a `sha512` integrity. The audit does not install packages and does not ask the registry whether a later version exists.

## Security tests

`tests/security_tests.manifest` names the security tests that CMake always registers. `dependencies.audit` fails when one of those names is absent from the build. `systemd.sandbox` stays conditional on the systemd tools, as it already was. The manifest is not a claim that those tests cover every threat.

## Limits

GitHub Check status still builds Debug and Release on Ubuntu 24.04. The audit runs inside that existing test step. It does not add a job, a sanitizer matrix, or a network scan. Schema v9 is unchanged. `configuration.manage` and `hub.manage` stay unimplemented. Browser kick and mute stay undispatched. Status and health stay bearer-only. `Require local` stays. No residual threat is closed.
