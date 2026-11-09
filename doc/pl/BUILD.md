# Budowanie

Wymagane: Linux, CMake >=3.22, kompilator C++20 (GCC >=11), pthreads, utf8proc 2.x i pliki deweloperskie OpenSSL >=1.1.1. Testy używają Pythona >=3.8 i programu openssl. ncdc dodaje dwa testy rzeczywistego klienta; bez niego CMake jawnie informuje o pominięciu. Zależności nie są pobierane podczas konfiguracji.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
ctest --test-dir build-release --output-on-failure
```

`-DBUILD_TESTING=OFF` wyłącza testy i wymaganie Pythona. `cmake --install build-release --prefix /usr/local` instaluje sam program (może wymagać sudo). Katalogi językowe są wkompilowane. Wersja pochodzi z CMake; `--version` zwraca alpha-0.1.11. Dokładną kompilację identyfikuje także `git rev-parse HEAD`. Nie edytuj plików generowanych. Wymagane są biblioteki Connector/C MariaDB; działający serwer SQL jest opcjonalny. Node.js i Apache2 nie są wymagane do budowania C++; frontend ma osobne wymagania.

Pełne polecenia konfiguracji AddressSanitizer/UndefinedBehaviorSanitizer: [BUILD US](../us/BUILD.md). Osobne budowanie frontendu: [WEBADMIN.md](WEBADMIN.md).

Instalacja obejmuje program i `etc/dc24h/dc24h.ini.example` względem wybranego prefiksu; nie instaluje aktywnej konfiguracji ani nie uruchamia usługi.

Wymagany jest Connector/C (`libmariadb-dev`). Zainstaluj `mariadb-server`, aby uruchomić opcjonalny test `database.process`; CMake informuje o brakujących narzędziach. Instalowana jest też niezmienna migracja SQL w `share/dc24h/migrations`.

## Budowanie WebAdmin

Użyj wspieranego Node/npm, potem `npm --prefix webadmin ci` i `npm --prefix webadmin run build`. Zasoby są niezależne od domyślnego CMake. Instalacja gotowego frontendu wymaga `-DDC24H_INSTALL_WEBADMIN=ON`; operator odpowiada za build z tego samego checkoutu. Testy przeglądarkowe i Apache2: [WEBADMIN.md](WEBADMIN.md).
