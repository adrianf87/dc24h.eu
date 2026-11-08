# dc24h.eu

Modularny serwer Direct Connect Hub rozwijany od podstaw w C++20 dla Linux.
Docelowo: ADC/ADCS, MariaDB oraz WebAdmin w React + TypeScript + Vite.

**Aktualna wersja: alpha-0.1.2 — Network.** Core z alpha-0.1.1 zapewnia
uruchamianie, zatrzymywanie i SIGINT/SIGTERM. Network dodaje nieblokujący TCP
IPv4/IPv6, wiele portów i klientów, limity, timeouty oraz zamykanie gniazd.
Domyślny nasłuch: **127.0.0.1:1511**. Dane są odbierane i odrzucane;
parser ADC, TLS/ADCS, MariaDB, API i WebAdmin pozostają planowane.

## Szybki start

Wymagania: Linux, kompilator C++20 (GCC 11+), CMake 3.22+, pthreads;
Python 3.8+ do testów.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/dc24h --version
./build/dc24h
# Kilka portów; IPv6 ma oddzielny listener:
./build/dc24h --listen 127.0.0.1:1511 --listen '[::1]:1512' --max-connections 512
```

Zatrzymanie: `Ctrl+C` albo `kill -TERM <PID>`. Poprawne zatrzymanie zwraca kod 0.
Logi z czasem UTC trafiają na stderr. `--help` wyświetla dostępne opcje.

## Dokumentacja

Indeks: [doc/README.md](doc/README.md). Zobacz szczególnie
[budowanie](doc/BUILD.md), [Core i architekturę](doc/ARCHITECTURE.md),
[sieć i porty](doc/NETWORK.md), [konfigurację](doc/CONFIGURATION.md),
[testowanie](doc/TESTING.md), [plan rozwoju](doc/ROADMAP.md)
i [historię zmian](CHANGELOG.md).

Kod objęty jest istniejącą licencją [GPL-3.0](LICENSE).
