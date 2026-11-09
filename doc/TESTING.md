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
Od alpha-0.1.2 używa portu 0, aby unikać konfliktów z innymi procesami.

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

Lokalna weryfikacja alpha-0.1.1, alpha-0.1.2 i alpha-0.1.3: Debian 13, GCC 14.2,
CMake 3.31, konfiguracje Debug i Release. Definicja GitHub Actions obejmuje
Ubuntu 22.04 i 24.04 w obu konfiguracjach; obecność workflow nie oznacza,
że zdalne CI zakończyło się sukcesem. Debian 12 i Ubuntu 26.04 wymagają
osobnej weryfikacji. Testy nie są benchmarkiem wydajności ani testem DDoS.

Nie są jeszcze testowane handshake/sesje ADC, TLS, baza, API ani WebAdmin. Wyczerpanie
limitu watch `epoll`/pamięci i błędy kernela wymagają osobnych testów wstrzykiwania
błędów; test deskryptorów nie pokrywa wszystkich ścieżek braku zasobów.

## ADC — alpha-0.1.3

`adc.parser` w C++ sprawdza osiem typów nagłówków, pola SID/CID, selektory F,
15 nazw komend BASE i nieznane komendy, escaping, polski tekst i czterobajtowe
znaki Unicode. Odrzuca błędne UTF-8 (overlong, surrogates, out-of-range,
nieukończone sekwencje), brak NFC, surowe control/DEL, błędne separatory,
routing, escape i przekroczenia limitów. Sprawdza dokładną granicę 16 KiB,
128 parametrów dla kilku typów nagłówków, każdy podział przykładowego strumienia
między dwoma odczytami, wejście bajt po bajcie, odrzucenie 1 MiB bez LF,
odzyskanie synchronizacji i idempotentny EOF. Deterministyczny korpus 4000
losowych linii porównuje wyniki dla całego wejścia i losowych fragmentów.
Nie zastępuje to długotrwałego fuzzingu ani audytu bezpieczeństwa.

`adc.process` zawiera pięć testów na prawdziwych socketach: fragmentacja i wiele
ramek, ignorowanie błędnych danych, niezależność buforów klientów i ucięcie,
przekroczenie limitu z działającym drugim klientem oraz brak sekretów w logach.
Połowiczne zamknięcie i EOF serwera potwierdzają przetworzenie całego wejścia
przed odczytem liczników przy stopie.

`adc.ncdc` uruchamia zainstalowany klient ncdc w nowym katalogu tymczasowym
i pseudoterminalu. Nie korzysta z konfiguracji użytkownika, istniejących hubów
ani udziałów. Lokalny relay rejestruje początkową ramkę klienta, sprawdza HSUP
z BASE/TIGR i przekazuje ją do dc24h. Po EOF liczniki muszą wskazać jedną
poprawną, rozpoznaną ramkę i brak błędów. Proces ncdc, katalog i sockety są
sprzątane. CMake pomija test z komunikatem tylko wtedy, gdy brak ncdc.

Lokalnie wykonano 7/7 zestawów CTest w Debug i Release, w tym rzeczywisty ncdc
1.23.1. Parser przeszedł także AddressSanitizer i UndefinedBehaviorSanitizer.
Zależność Unicode: utf8proc 2.9.0. Weryfikacja ncdc dotyczy początkowego HSUP,
nie pełnego logowania ani komunikacji w stanie NORMAL. Workflow CI instaluje
utf8proc i ncdc; zdalny wynik CI wymaga osobnego sprawdzenia.
