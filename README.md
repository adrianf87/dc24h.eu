# dc24h.eu

Fundament serwera ADC/ADCS Direct Connect Hub w wersji Alpha 0.1 (`0.1.0-alpha.1`). Etap E0 obejmuje program `dc24h-hub`, ekran startowy Webadmin oraz automatyczne kompilacje i testy. Docelowy format konfiguracji huba to TOML (`/etc/dc24h/hub.toml`).

## Systemy docelowe

Na architekturze `amd64` plan Alpha 0.1 obejmuje Ubuntu Server 22.04 LTS, 24.04 LTS i 26.04 LTS oraz Debian 12 (Bookworm) i Debian 13 (Trixie).

## Backend C++20

Potrzebujesz kompilatora obsługującego C++20, CMake 3.22 lub nowszego oraz Make. Na Ubuntu i Debianie użyj pakietów `g++`, `cmake`, `make` i `ca-certificates`. Przy pierwszej konfiguracji testów CMake pobiera GoogleTest i sprawdza sumę SHA-256 archiwum. Flaga `-DBUILD_TESTING=OFF` ogranicza kompilację do programu i jego biblioteki.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

./build/dc24h-hub
./build/dc24h-hub --help
./build/dc24h-hub --version
```

Użyj presetu `release`, aby zbudować wersję zoptymalizowaną i uruchomić jej testy.

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/dc24h-hub --version
```

Preset `debug` działa analogicznie i zapisuje wynik w `build/debug/`. Program zwraca `0` przy poprawnym wywołaniu i `2` przy błędnych argumentach.

## Webadmin

Użyj Node.js 24 LTS z npm. Dokładna wersja używana przez CI jest w `webadmin/.nvmrc`.

```sh
cd webadmin
npm ci
npm run lint
npm run typecheck
npm test
npm run build
```

Vite zapisuje skompilowaną aplikację w `webadmin/dist/`. Polecenie `npm run dev` uruchamia lokalny podgląd, domyślnie pod adresem `http://localhost:5173/`.

## CI

GitHub Actions kompiluje i testuje konfiguracje Debug i Release oraz sprawdza Webadmin na runnerach Ubuntu 22.04, 24.04 i 26.04 (`amd64`). Kontrola formatu C++ używa `clang-format-18` na Ubuntu 24.04.

Na Debianie 12 i 13 użyj powyższych poleceń, aby lokalnie sprawdzić Debug, Release i Webadmin.

## Dokumentacja i licencja

- [Plan Alpha 0.1 po polsku](docs/pl/alpha-0.1/ALPHA-0.1-PLAN.md)
- [Plan Alpha 0.1 w języku angielskim](docs/us/alpha-0.1/ALPHA-0.1-PLAN.md)
- [GNU General Public License v3.0](LICENSE)
