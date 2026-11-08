# Historia zmian

## alpha-0.1.1 — 2026-11-09

Pierwszy etap Core:

- Projekt CMake 3.22+ i C++20, biblioteka `dc24h_core` oraz program `dc24h`.
- Jednorazowy cykl życia `created → running → stopping → stopped`.
- Blokujące oczekiwanie na stop bez aktywnej pętli Core; bezpieczne współbieżnie,
  idempotentne `request_stop()`, również przed uruchomieniem.
- Odbieranie SIGINT/SIGTERM przez `pthread_sigmask` i `sigtimedwait`, osobny
  wątek `std::jthread`, kontrolowane dołączenie wątku i wyjście z kodem 0.
- Logi UTC na stderr, obsługa błędów startu i oczekiwania na sygnał.
- Opcje `--help`, `--version` i walidacja argumentów.
- Testy cyklu życia, konkurencyjnych żądań stop, anulowania oczekiwania,
  CLI, rzeczywistych sygnałów podczas startu i wielokrotnych sygnałów.
- Definicja CI Debug/Release dla Ubuntu 22.04 i 24.04; dokumentacja w `doc/`.

TCP/ADC/ADCS, TLS, użytkownicy, MariaDB, API, konfiguracja plikowa i WebAdmin
nie są jeszcze zaimplementowane. Nie jest to działający hub ADC.
