# dc24h.eu

Modularny serwer Direct Connect Hub rozwijany od podstaw w C++20 dla Linux.
Docelowo: ADC/ADCS, MariaDB oraz WebAdmin w React + TypeScript + Vite.

**Aktualna wersja: alpha-0.1.3 — ADC (parser).**

- **alpha-0.1.1 Core:** uruchamianie, zatrzymywanie i SIGINT/SIGTERM.
- **alpha-0.1.2 Network:** nieblokujący TCP IPv4/IPv6, wiele portów i klientów.
- **alpha-0.1.3 ADC:** ramkowanie strumienia, rozpoznawanie komend, nagłówki,
  escaping, UTF-8/NFC, limity i odzyskiwanie po błędnej ramce.

Domyślny nasłuch: **127.0.0.1:1511**. Serwer parsuje i zlicza wiadomości,
bez odpowiedzi i wykonywania poleceń. Handshake, sesje i pełne logowanie
klienta nie są jeszcze zaimplementowane. TLS/ADCS, MariaDB, API i WebAdmin
pozostają planowane. Test ncdc sprawdza odbiór początkowego `HSUP`.

## Szybki start

Wymagania: Linux, kompilator C++20 (GCC 11+), CMake 3.22+, pthreads, `libutf8proc-dev`;
Python 3.8+ do testów; ncdc do testu rzeczywistego klienta.

```sh
sudo apt-get install build-essential cmake libutf8proc-dev python3 ncdc
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
[parser ADC](doc/ADC_PROTOCOL.md), [sieć i porty](doc/NETWORK.md), [konfigurację](doc/CONFIGURATION.md),
[testowanie](doc/TESTING.md), [plan rozwoju](doc/ROADMAP.md)
i [historię zmian](CHANGELOG.md).

Kod objęty jest istniejącą licencją [GPL-3.0](LICENSE).
