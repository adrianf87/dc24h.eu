# GitHub Actions CI

Od alpha-0.7.13 workflow `Check status` w `.github/workflows/build.yml` korzysta
ze wskazanego własnego runnera `server-747585786`, z etykietami `self-hosted`,
`Linux`, `X64` i `server-747585786`. Przed budowaniem sprawdza Ubuntu 24.04,
nazwę runnera i konto usługi `github-runner`. Historyczne opisy innych runnerów
nie określają bieżącej konfiguracji CI.

Uruchomienie następuje po push, dla pull requestów z tego samego repozytorium
do `main` lub `alpha-0.7.*` oraz ręcznie. Pull requesty z forków nie wykonują kodu
na trwałym runnerze. Uprawnienia repozytorium są tylko do odczytu, a checkout
nie zapisuje poświadczeń. Workflow nie zawiera hasła konta runnera ani bazy.

Jedna grupa współbieżności dla repozytorium wykonuje tylko jedno zadanie naraz.
Debug i Release są budowane i testowane kolejno w osobnych katalogach, potem
uruchamiany jest WebAdmin. Zapobiega to równoczesnej pracy checkout i kompilacji
w tym samym katalogu własnego runnera. CMake, CTest, Node i Playwright korzystają
z liczby procesorów podanej przez `nproc`.

Administrator powinien zapewnić narzędzia C++20, CMake, Python 3, OpenSSL,
libutf8proc-dev, libgcrypt20-dev, libssl-dev, libmariadb-dev, serwer i klienta
MariaDB, Apache2, ncdc i biblioteki systemowe Chromium. Workflow sprawdza narzędzia,
a CMake nagłówki i biblioteki. Zadania nie instalują pakietów ani nie zmieniają
usług przez sudo. setup-node wybiera Node 22, npm ci instaluje zależności według
lockfile, a Playwright pobiera Chromium do katalogu użytkownika.

Obie kompilacje wymagają `DC24H_REQUIRE_INTEGRATION_TESTS=ON` i uruchamiają
wszystkie zestawy CTest, w tym odzyskanie i restart tymczasowej MariaDB,
rzeczywiste ncdc ADC/ADCS oraz Apache. WebAdmin sprawdza typy, budowanie produkcyjne,
testy jednostkowe, przeglądarkę i integrację z C++ API Release. Raporty awarii
przechowywane są siedem dni. CI nie zmienia produkcyjnej bazy, kont, schematu,
kluczy TLS ani usług. Całe zadanie ma limit 180 minut.

Push gałęzi uruchamia workflow. Lokalne powodzenie nie potwierdza dostępności
runnera ani wyniku zdalnego. Sprawdź odpowiedni commit w
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml).
