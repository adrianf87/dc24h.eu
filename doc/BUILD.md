# Budowanie

Wymagane: Linux, CMake >= 3.22, GCC >= 11 z biblioteką C++20, pthreads
i pakiet `libutf8proc-dev` (utf8proc 2.x).
Testy wymagają Pythona >= 3.8. CMake dodaje `adc.ncdc`, gdy znajdzie ncdc;
bez klienta wyświetla komunikat o pominięciu tego testu. CI instaluje ncdc.
Utf8proc zapewnia ścisłą walidację UTF-8 i porównanie z NFC; CMake wymaga
nagłówka i biblioteki, bez pobierania zależności podczas konfiguracji.
Program nie wymaga obecnie OpenSSL, MariaDB,
Node.js ani Apache2. C++20 jest wymaganiem publicznym targetu `dc24h_core`;
rozszerzenia języka są wyłączone.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Wariant wydania:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
ctest --test-dir build-release --output-on-failure
```

Instalacja samego programu bez testów i Pythona:

```sh
cmake -S . -B build-runtime -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-runtime --parallel 2
sudo cmake --install build-runtime --prefix /usr/local
```

Wersja pochodzi z CMake i jest generowana w `build/generated/dc24h/version.hpp`.
Nie edytuj wygenerowanego pliku. Artefakty budowania są ignorowane przez Git.
WebAdmin nie ma jeszcze kodu ani procesu budowania.

Test parsera z sanitizatorami (GCC/Clang):

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-sanitize --target adc_tests --parallel 2
./build-sanitize/adc_tests
```
