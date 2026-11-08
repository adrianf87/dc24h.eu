# dc24h.eu

Modularny serwer Direct Connect Hub rozwijany od podstaw w C++20 dla Linux.
Docelowo: ADC/ADCS, MariaDB oraz WebAdmin w React + TypeScript + Vite.

**Aktualna wersja: alpha-0.1.1 — Core.** Działa uruchamianie na pierwszym
planie, kontrolowane zatrzymywanie przez SIGINT/SIGTERM i programowe żądanie
stop, logowanie oraz testy. Proces nie otwiera jeszcze portów i nie obsługuje
klientów ADC. TCP, TLS, parser ADC, baza danych i WebAdmin są planowane.

## Szybki start

Wymagania: Linux, kompilator C++20 (GCC 11+), CMake 3.22+, pthreads;
Python 3.8+ do testów.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/dc24h --version
./build/dc24h
```

Zatrzymanie: `Ctrl+C` albo `kill -TERM <PID>`. Poprawne zatrzymanie zwraca kod 0.
Logi z czasem UTC trafiają na stderr. `--help` wyświetla dostępne opcje.

## Dokumentacja

Indeks: [doc/README.md](doc/README.md). Zobacz szczególnie
[budowanie](doc/BUILD.md), [Core i architekturę](doc/ARCHITECTURE.md),
[testowanie](doc/TESTING.md), [plan rozwoju](doc/ROADMAP.md)
i [historię zmian](CHANGELOG.md).

Kod objęty jest istniejącą licencją [GPL-3.0](LICENSE).
