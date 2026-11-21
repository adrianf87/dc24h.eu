# dc24h.eu

dc24h.eu is an ADC/ADCS Direct Connect hub project targeting Alpha 0.1 (`0.1.0-alpha.1`). It includes the `dc24h-hub` command-line program, a Webadmin welcome screen, and automated builds and tests. The `dc24h_adc` C++20 library implements ADC frame parsing and the hub-side connection state machine. The planned hub configuration uses TOML at `/etc/dc24h/hub.toml`.

## Target systems

Alpha 0.1 targets Ubuntu Server 22.04 LTS, 24.04 LTS, and 26.04 LTS, Debian 12 (Bookworm), and Debian 13 (Trixie) on `amd64`.

## C++20 backend

Build with a C++20 compiler, CMake 3.22 or later, Make, utf8proc for Unicode handling, and Libgcrypt for the ADC session hash. The Debian and Ubuntu package names are `g++`, `cmake`, `make`, `ca-certificates`, `libutf8proc-dev`, and `libgcrypt20-dev`. CMake downloads GoogleTest and verifies the archive's SHA-256 checksum the first time it configures the tests. Use `-DBUILD_TESTING=OFF` for a build of the hub CLI and ADC library.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

./build/dc24h-hub
./build/dc24h-hub --help
./build/dc24h-hub --version
```

Use the `release` preset to build and test the optimized configuration.

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/dc24h-hub --version
```

The `debug` preset uses the same commands and writes its output to `build/debug/`. The program exits with status `0` on a valid invocation and `2` on invalid arguments.

## Webadmin

Use Node.js 24 LTS with npm. The CI version is pinned in `webadmin/.nvmrc`.

```sh
cd webadmin
npm ci
npm run lint
npm run typecheck
npm test
npm run build
```

Vite writes the compiled application to `webadmin/dist/`. Run `npm run dev` to start the development server at `http://localhost:5173/` by default.

## CI

GitHub Actions builds and tests the Debug and Release configurations and checks Webadmin on Ubuntu 22.04, 24.04, and 26.04 runners (`amd64`). The C++ formatting check uses `clang-format-18` on Ubuntu 24.04.

The same build and test commands work locally on Debian 12 and 13. The ADC parser guide covers the optional Clang/libFuzzer build.

## Documentation and license

- [Alpha 0.1 plan (en-US)](docs/us/alpha-0.1/ALPHA-0.1-PLAN.md)
- [Alpha 0.1 plan (pl-PL)](docs/pl/alpha-0.1/ALPHA-0.1-PLAN.md)
- [ADC parser and connection state machine (en-US)](docs/us/alpha-0.1/ADC-PARSER.md)
- [ADC parser and connection state machine (pl-PL)](docs/pl/alpha-0.1/ADC-PARSER.md)
- [GNU General Public License v3.0](LICENSE)
