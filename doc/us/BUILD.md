# Build

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

Language catalogs are compiled into the binary. CMake generates `dc24h/version.hpp` from project version 0.1.11; `--version` prints alpha-0.1.11. Identify an exact build with that version plus `git rev-parse HEAD`. Do not edit generated files. MariaDB Connector/C development/runtime libraries are required; a running SQL server is optional. Node.js and Apache2 are not required for the C++ build; the optional frontend has separate requirements.

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
