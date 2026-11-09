# Changelog

## alpha-0.1.4 — 2026-11-09 (ADCS transport; development)

- Added `dc24h_adcs`, using OpenSSL TLS contexts and per-client nonblocking TLS sessions.
- Introduced `--adcs-listen`, `--tls-cert`, and `--tls-key`; TCP and ADCS listeners can coexist.
- Enforced TLS 1.2 or later, disabled TLS compression, renegotiation and session caching.
- Verified readable PEM certificate chain and matching private key at startup; failed configuration never advertises ready listeners.
- Routed decrypted bytes into the existing ADC stream decoder; isolated failed TLS peers and bounded handshake lifetime.
- Added TLS counters, transport unit and integration test sources, and EN/US and Polish documentation.
- Preserved the existing alpha-0.1.1 to alpha-0.1.3 implementation and commit history.

Scope: secure inbound transport and parsing only. No ADC login, outbound messages, TLS close_notify on server shutdown, credential reload, database, API or WebAdmin. The OpenSSL transport unit was exercised locally; complete CTest and ncdc ADCS end-to-end tests require a matching development/test environment.

## alpha-0.1.3 — 2026-11-09

Etap ADC — parser bez automatu sesji i obsługi poleceń:

- Osobna biblioteka `dc24h_adc`, niezależna od deskryptorów, Core i loggera.
- `parse_line` i `StreamDecoder`: LF, keepalive, fragmentacja TCP, wiele ramek
  w odczycie, częściowa ramka przy zamknięciu i odzyskiwanie po błędzie.
- Nagłówki B/C/D/E/F/H/I/U, SID, CID o długości zależnej od hasha i selektory cech.
- Rozpoznawanie 15 komend BASE; zachowanie nazw nieznanych komend rozszerzeń.
- Dekodowanie escape, ścisłe UTF-8 i NFC przez nową zależność utf8proc.
- Limity 16 KiB treści ramki i 128 parametrów; odrzucanie zbyt długiej ramki
  do LF, bez nieograniczonego buforowania. Lokalna polityka odrzuca surowe
  ASCII control/DEL, w tym CR i NUL; nie przyjmuje CRLF jako LF.
- Oddzielny dekoder każdego klienta TCP i zagregowane liczniki ADC na stopie,
  bez logowania parametrów, prywatnych identyfikatorów ani hashy haseł.
- Testy składni, Unicode, granic, losowego korpusu i integracji TCP. Smoke test
  prawdziwego ncdc w izolowanej sesji/PTY sprawdza początkowy HSUP.
- Debug/Release i parser z AddressSanitizer/UndefinedBehaviorSanitizer;
  aktualizacja CI, zależności i dokumentacji w `doc/`.

Walidacja jest składniowa. Brak walidacji semantyki komend, sesji, autoryzacji,
serializera, routingu i odpowiedzi. Pełne logowanie ncdc nie jest obsługiwane.

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
