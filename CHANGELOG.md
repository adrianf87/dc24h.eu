# Historia zmian

## alpha-0.1.2 — 2026-11-09

Etap Network, rozwijający istniejący Core:

- Osobne targety `dc24h_network` i `dc24h_logging`, C++20 bez zewnętrznych bibliotek.
- Nieblokujący TCP IPv4/IPv6, do 16 listenerów, wiele klientów w pętli `epoll`.
- RAII deskryptorów, tokeny zdarzeń niezależne od fd oraz close-on-exec.
- Domyślnie `127.0.0.1:1511`; CLI `--listen`, `--max-connections`, `--idle-timeout`.
- Limit 1024 klientów i timeout 120 sekund domyślnie; odrzucanie nadmiaru,
  zwalnianie slotów po FIN/reset oraz ograniczone partie odczytu/przyjmowania.
- Zatrzymanie przez Core i `eventfd`, zamykanie wszystkich gniazd przy stopie.
- Izolowanie błędów klienta, ponawianie po braku zasobów, zwijanie częściowego
  startu i kod 1 po błędzie listenera. Liczniki połączeń i odebranych bajtów.
- Testy parsera/limitów, deskryptorów, 48 równoczesnych klientów, wielu portów,
  IPv6, resetów, timeoutu, przeciążenia deskryptorów i stopu przy ciągłym ruchu.
- Aktualizacja CI oraz dokumentacji modułów, konfiguracji i testowania.

Dane TCP są odbierane i odrzucane. ADC, zapis odpowiedzi, TLS/ADCS,
uwierzytelnianie, MariaDB, API i WebAdmin pozostają niezaimplementowane.

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
