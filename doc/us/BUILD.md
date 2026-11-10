# Build

**Current increment — alpha-0.3.11:** [User Disconnect](USER_DISCONNECT.md) finalizes lifecycle and releases departing output, protocol/SID and registry resources before sending public QUI. Notification allocation failures cannot abort source cleanup; recipients that cannot receive the notice are closed. Existing own ISTA → IQUI drain and shutdown behavior remain. Earlier module descriptions retain their historical scope.

**Increment delivered in alpha-0.3.5:** [User Core](USER_CORE.md) adds checked UTF-8/NFC Nickname values, shared ADC/User text validation, immutable rename candidates and explicit projection from validated ClientInfo/VerifiedIdentity. Users now links the neutral dc24h_text library using the existing utf8proc package. Manager ownership/uniqueness remains unchanged. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) owns SID/CID/nickname indexes together with live models. Const lookups and managed lifecycle/User mutations preserve uniqueness through admission, rename and cleanup. LoginHub retains protocol and output ownership. Earlier descriptions retain their historical delivery scope.

**Increment delivered in alpha-0.3.3:** [User Core](USER_CORE.md) adds an owning public guest identity/profile to Session, with validated full-INF projection and lifecycle-guarded updates. It was developed directly from main with Session Core; main now combines it with the previously merged alpha-0.3.2 Session Manager. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) centrally owns all registered lifecycle models, with bounded capacity, stable lookup, phase counts and ordered snapshot pages. Protocol peers borrow models; their queues and public routing indexes remain in ADC. Earlier descriptions retain their historical scope.

**Increment delivered in alpha-0.3.1:** [Session Core](SESSION_CORE.md) provides the independent Users model and stable lifetime IDs, with live readiness/closure/timeline integration. Earlier stage descriptions below retain their original delivery scope.

**Increment delivered in alpha-0.2.10:** [Negotiation security](NEGOTIATION_SECURITY.md) adds pending admission caps, cumulative pre-readiness input budgets and shared pre-INF/Tiger attempt limits. Earlier release descriptions below retain their original scope.

**Increment delivered in alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) adds semantic status/quit validation, source-checked B/D/E/F status routing, precise errors and bounded terminal ISTA/own-IQUI output. Historical descriptions below retain the behavior of their original releases.

**Integration delivered in alpha-0.2.8:** [Guest login](ADC_LOGIN.md) attaches the state/SUP/SID/INF/Tiger stack to the actual C++ TCP/TLS reactor with bounded ordered output, nickname/CID admission, public presence and complete ncdc NORMAL/reconnect tests. Historical stage descriptions below retain their original scope; statements that transport attachment/output/login were still planned describe those earlier versions. Registered-user authentication, chat, search and transfer dispatch remain planned.

For the complete integration suite, configure `-DDC24H_REQUIRE_INTEGRATION_TESTS=ON` with BUILD_TESTING enabled. This requires openssl, ncdc, MariaDB server/client tools and Apache2/mod_ssl; CI uses this mode. Missing tools fail configuration. The default OFF mode keeps existing optional-test behavior for developer machines. `project.metadata` uses Python only and checks release/documentation consistency. See [audit](AUDIT_ALPHA_0.1.md).

Requirements: Linux, CMake >=3.22, a C++20 toolchain (GCC >=11), pthreads, utf8proc 2.x development files and OpenSSL >=1.1.1 development files. Python >=3.8 and the `openssl` executable support tests. ncdc adds two real-client smoke tests when available; CI installs it. Missing ncdc is reported explicitly by CMake. Dependencies are located locally; configuration does not download them.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
ctest --test-dir build-release --output-on-failure
```

For a binary without tests/Python requirements:

```sh
cmake -S . -B build-runtime -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-runtime --parallel 2
sudo cmake --install build-runtime --prefix /usr/local
```

Language catalogs are compiled into the binary. CMake generates `dc24h/version.hpp` from project version 0.2.7; `--version` prints alpha-0.2.7. Identify an exact build with that version plus `git rev-parse HEAD`. Do not edit generated files. MariaDB Connector/C development/runtime libraries are required; a running SQL server is optional. Node.js and Apache2 are not required for the C++ build; the optional frontend has separate requirements.

For sanitizers:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-sanitize --parallel 2
ctest --test-dir build-sanitize --output-on-failure
```

GCC/Clang AddressSanitizer/UndefinedBehaviorSanitizer must be available. The optional frontend build is documented in [WEBADMIN.md](WEBADMIN.md).

Installation includes the binary and `etc/dc24h/dc24h.ini.example` relative to the selected prefix; it never installs an active configuration or starts a service.

Connector/C (`libmariadb-dev`) is required. Install `mariadb-server` for the optional `database.process` test; CMake reports missing tools. Installation also includes the immutable migration SQL under `share/dc24h/migrations`.

## WebAdmin build

Use a supported Node/npm toolchain, then `npm --prefix webadmin ci` and `npm --prefix webadmin run build`. Assets are independent of default CMake builds. To install an already built frontend, configure `-DDC24H_INSTALL_WEBADMIN=ON`; rebuilding assets from the same checkout is the operator responsibility. Browser tests and Apache2 setup: [WEBADMIN.md](WEBADMIN.md).

## INF dependency — alpha-0.2.6

Install `libgcrypt20-dev` in addition to existing C++ dependencies. CMake requires gcrypt.h/libgcrypt for dc24h_adc. INF identity validation uses GCRY_MD_TIGER1 (standard Tiger/192); runtime algorithm/context unavailability fails validation. The module does not switch FIPS/global modes. This is unrelated to TLS ciphers and does not implement Tiger tree file hashing.

## Identity failure tests — alpha-0.2.7

No new dependency is added beyond Libgcrypt/OpenSSL. The isolated adc_identity_failure_tests executable needs a GNU-compatible Linux linker supporting --wrap; CMake applies these flags only to that test target. Runtime/server targets contain no fault wrappers or injected crypto interface.
