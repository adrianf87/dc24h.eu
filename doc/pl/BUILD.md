# Budowanie

**Bieżący etap — alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

**Etap dostarczony w alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) posiada indeksy SID/CID/nick wraz z żywymi modelami. Odczyt const i zarządzane zmiany cyklu życia/User zachowują unikalność przy przyjęciu, zmianie nicku i sprzątaniu. LoginHub zachowuje protokół i kolejki. Starsze opisy dotyczą zakresu historycznych etapów.

**Etap dostarczony w alpha-0.3.3:** [User Core](USER_CORE.md) dodaje do Session publiczny model gościa i profilu, projekcję całego zwalidowanego INF oraz aktualizacje przez bramkę cyklu życia. Powstał bezpośrednio z main z Session Core; main łączy go teraz z wcześniej scalonym alpha-0.3.2 Session Manager. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.

**Aktualny etap — alpha-0.3.1:** [Session Core](SESSION_CORE.md) dostarcza niezależny model Users i trwałe w obrębie reaktora ID sesji, podłączone do rzeczywistej gotowości, zamykania i czasów. Wcześniejsze opisy zachowują zakres pierwotnego etapu.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Pełny zestaw integracji wymaga `-DDC24H_REQUIRE_INTEGRATION_TESTS=ON` przy włączonym BUILD_TESTING. Potrzebne są openssl, ncdc, narzędzia serwera/klienta MariaDB i Apache2/mod_ssl; CI używa tego trybu. Brak narzędzi przerywa konfigurację. Domyślne OFF zachowuje opcjonalność testów dla deweloperów. `project.metadata` potrzebuje tylko Pythona i sprawdza spójność wersji/dokumentacji. Zobacz [audyt](AUDIT_ALPHA_0.1.md).

Wymagane: Linux, CMake >=3.22, kompilator C++20 (GCC >=11), pthreads, utf8proc 2.x i pliki deweloperskie OpenSSL >=1.1.1. Testy używają Pythona >=3.8 i programu openssl. ncdc dodaje dwa testy rzeczywistego klienta; bez niego CMake jawnie informuje o pominięciu. Zależności nie są pobierane podczas konfiguracji.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
ctest --test-dir build-release --output-on-failure
```

`-DBUILD_TESTING=OFF` wyłącza testy i wymaganie Pythona. `cmake --install build-release --prefix /usr/local` instaluje sam program (może wymagać sudo). Katalogi językowe są wkompilowane. Wersja pochodzi z CMake; `--version` zwraca alpha-0.2.7. Dokładną kompilację identyfikuje także `git rev-parse HEAD`. Nie edytuj plików generowanych. Wymagane są biblioteki Connector/C MariaDB; działający serwer SQL jest opcjonalny. Node.js i Apache2 nie są wymagane do budowania C++; frontend ma osobne wymagania.

Pełne polecenia konfiguracji AddressSanitizer/UndefinedBehaviorSanitizer: [BUILD US](../us/BUILD.md). Osobne budowanie frontendu: [WEBADMIN.md](WEBADMIN.md).

Instalacja obejmuje program i `etc/dc24h/dc24h.ini.example` względem wybranego prefiksu; nie instaluje aktywnej konfiguracji ani nie uruchamia usługi.

Wymagany jest Connector/C (`libmariadb-dev`). Zainstaluj `mariadb-server`, aby uruchomić opcjonalny test `database.process`; CMake informuje o brakujących narzędziach. Instalowana jest też niezmienna migracja SQL w `share/dc24h/migrations`.

## Budowanie WebAdmin

Użyj wspieranego Node/npm, potem `npm --prefix webadmin ci` i `npm --prefix webadmin run build`. Zasoby są niezależne od domyślnego CMake. Instalacja gotowego frontendu wymaga `-DDC24H_INSTALL_WEBADMIN=ON`; operator odpowiada za build z tego samego checkoutu. Testy przeglądarkowe i Apache2: [WEBADMIN.md](WEBADMIN.md).

## Zależność INF — alpha-0.2.6

Zainstalować `libgcrypt20-dev` obok dotychczasowych zależności C++. CMake wymaga gcrypt.h/libgcrypt dla dc24h_adc. Tożsamość INF używa GCRY_MD_TIGER1 (standardowy Tiger/192); brak algorytmu/kontekstu odrzuca walidację. Moduł nie zmienia FIPS/globalnych trybów. To nie szyfry TLS ani obliczanie drzew Tiger plików.

## Testy awarii tożsamości — alpha-0.2.7

Brak nowej zależności poza Libgcrypt/OpenSSL. Izolowany adc_identity_failure_tests wymaga linkera Linux zgodnego z GNU i --wrap; CMake stosuje flagi wyłącznie do tego testu. Program serwera nie zawiera wrapperów ani interfejsu wstrzykiwania kryptografii.
