# Testowanie

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build-release --output-on-failure
```

`core.lifecycle` jest programem C++ sprawdzającym stan początkowy, działanie
run/stop, stop przed startem, idempotencję, odrzucanie ponownego i jednoczesnego
run oraz 20 powtórzeń konkurencyjnych żądań stop z ośmiu wątków. Sprawdza też
anulowanie oczekiwania na sygnał i odbiór zakolejkowanego SIGTERM.
Warunki testów działają również w Release (nie opierają się na `assert`).

`core.process` uruchamia 7 przypadków unittest w Pythonie: wersja, pomoc,
błędne argumenty, SIGINT, SIGTERM, sygnał w trakcie startu oraz seria sygnałów.
Dwa ostatnie scenariusze wykonują po 10 uruchomień procesu. Sprawdza kod
wyjścia 0, log odebranego sygnału i dokładnie jedną sekwencję zatrzymania.
W alpha-0.1.2 używa portu 0, aby unikać konfliktów z innymi procesami.

`network.unit` sprawdza parser adresów, zakresy opcji, stop przed startem,
odrzucenie ponownego run, stan Core po błędzie Network oraz 30 powtórzeń
częściowo nieudanego startu. Zajęty port jest utrzymywany przez gniazdo testowe,
a liczba deskryptorów `/proc/self/fd` musi wrócić do wartości początkowej.

`network.process` obejmuje 10 testów black-box na prawdziwych gniazdach:

- 48 jednoczesnych klientów, równoległy odczyt danych, liczniki, brak echo.
- Dwa listenery i IPv6 loopback (skip tylko jeśli IPv6 jest niedostępne).
- Limit klientów, odrzucenie nadmiaru oraz ponowne użycie zwolnionego slotu.
- Timeout bezczynności i dalsze działanie listenera.
- 30 resetów klienta oraz zamknięcie połowy połączenia przez FIN.
- Zatrzymanie pod ciągłym napływem danych i zamknięcie aktywnych gniazd.
- Odzyskiwanie przy `RLIMIT_NOFILE=40`, bez kończenia serwera.
- Zajęty port, brak fałszywych logów gotowości i kod błędu 1.
- Niepoprawne wartości CLI i kod błędu 2 bez uruchamiania serwera.

Synchronizacja gotowości odbywa się po logach listenerów; każdy proces
i socket testowy jest sprzątany, a oczekiwania mają timeouty. CTest ogranicza
czas suite do 30 sekund (Core/unit) lub 45 sekund (network.process).
Testy nie wymagają zewnętrznego serwera ani pakietów Pythona spoza standardowej
biblioteki. Test limitu deskryptorów ustawia limit wyłącznie procesu potomnego.

Lokalna weryfikacja alpha-0.1.1 i alpha-0.1.2: Debian 13, GCC 14.2,
CMake 3.31, konfiguracje Debug i Release. Definicja GitHub Actions obejmuje
Ubuntu 22.04 i 24.04 w obu konfiguracjach; obecność workflow nie oznacza,
że zdalne CI zakończyło się sukcesem. Debian 12 i Ubuntu 26.04 wymagają
osobnej weryfikacji. Testy nie są benchmarkiem wydajności ani testem DDoS.

Nie są jeszcze testowane moduły ADC/TLS, baza, API ani WebAdmin. Wyczerpanie
limitu watch `epoll`/pamięci i błędy kernela wymagają osobnych testów wstrzykiwania
błędów; test deskryptorów nie pokrywa wszystkich ścieżek braku zasobów.
