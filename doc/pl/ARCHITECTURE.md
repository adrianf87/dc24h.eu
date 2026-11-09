# Architektura

## Core — alpha-0.1.1 / alpha-0.1.2

`src/main.cpp` odpowiada za CLI, przygotowanie sygnałów, uruchomienie Core
i wynik procesu. `dc24h_core` zawiera `core::Server`, `core::TerminationSignals`
oraz integrację z transportem. `dc24h_network` zawiera TCP,
a `dc24h_logging` współdzielony logger. Nagłówki publiczne są w `include/dc24h/`.

`Server::run()` jest blokujący i jednorazowy. Stan początkowy to `created`.
Po uruchomieniu przechodzi w `running`; bez transportu czeka na zmiennej
warunkowej, a z Network wykonuje jego blokującą pętlę `epoll`.
Gotowość portów potwierdzają dopiero logi `Network listening on ...`.
`request_stop()` przełącza go w `stopping` i budzi oczekiwanie Core
oraz Network przez `eventfd`. `run()`
kończy cykl w `stopped`. Żądanie stop przed `run()` jest zapamiętane:
nie pojawia się wówczas stan `running`. Powtórne `run()` rzuca `logic_error`.
Zatrzymany obiekt nie jest ponownie uruchamiany; restart wymaga nowego procesu.

Stan i żądanie stop chroni mutex; powtórne oraz jednoczesne żądania stop są
dozwolone. Właściciel musi poczekać na zakończenie `run()` przed zniszczeniem
obiektu. Logger musi żyć dłużej niż Server.

## Sygnały i zasoby

Przed utworzeniem wątków `TerminationSignals` blokuje SIGINT i SIGTERM przez
`pthread_sigmask`. Nowy wątek dziedziczy maskę. Jeden `std::jthread` odbiera
sygnał przez `sigtimedwait`, loguje jego nazwę i wywołuje `request_stop()`.
Wątek główny wykonuje `Server::run()`. Nie ma asynchronicznego handlera, który
wywoływałby mutex, logger lub alokację pamięci.

Oczekiwanie na sygnał ma limit 100 ms, aby umożliwić zakończenie wątku także
po programowym stop lub błędzie uruchomienia. Limit nie opóźnia odbioru
oczekującego sygnału; dołączenie anulowanego wątku może potrwać około 100 ms
plus opóźnienia planisty. Core bez Network czeka bez timeoutu na zmiennej
warunkowej; Network używa
`epoll_wait` z interwałem do 250 ms do kontroli timeoutów bezczynności.

Maska sygnałów jest ustawieniem procesu programu `dc24h`, nie ogólną biblioteką
RAII: celowo pozostaje aktywna aż do wyjścia procesu. Chroni końcowe sprzątanie
przed kolejnymi SIGINT/SIGTERM. Kolejne sygnały nie wymuszają brutalnego stop.
`std::jthread` jest dołączany przed zniszczeniem Server i loggera.
Błąd oczekiwania na sygnał żąda stop i powoduje kod zakończenia 1.

Dokumentacja użytych interfejsów systemowych:
[pthread_sigmask(3)](https://man7.org/linux/man-pages/man3/pthread_sigmask.3.html)
oraz [sigtimedwait(2)](https://man7.org/linux/man-pages/man2/sigtimedwait.2.html).

## Network — alpha-0.1.2

`main` tworzy Network i przekazuje do Core wskaźnik niewłaścicielski. Network
żyje dłużej niż Core. Pętla transportu działa w wątku głównym; wątek sygnałów
wywołuje tylko żądanie stop. Wszystkie operacje gniazd są wykonywane w pętli,
a jedyną współdzieloną operacją I/O jest nieblokujący zapis do `eventfd`.
Liczniki i flaga stop są atomowe. Jednorazowość `run()` wymusza atomowa flaga.

RAII zarządza listenerami, klientami i `epoll`; destruktor Network zamyka
`eventfd` po zakończeniu pętli i wątku sygnałów. Błąd startu zamyka również
wcześniej otwarte listenery. Wyjątek Network ustawia stan Core na `stopped`
i trafia do `main`, które zwraca 1. Zwykłe błędy peer zamykają tylko klienta.
Tokeny zdarzeń klientów są monotoniczne i nie zależą od numerów fd, co pozwala
odrzucać stare zdarzenia po ponownym użyciu deskryptora przez system.

Moduł ADCS (`src/adcs`) dodaje kontekst OpenSSL i sesje TLS. Logger używa katalogów `languages/us` i `languages/pl`; domyślny język to US. Program ignoruje SIGPIPE, aby zapis TLS po rozłączeniu nie zatrzymywał procesu. Szczegóły TLS: [ADCS_TLS.md](ADCS_TLS.md).

Szczegóły: [NETWORK.md](NETWORK.md), kontrakt: [API.md](API.md).

## ADC — alpha-0.1.3

`dc24h_adc` zawiera model wiadomości, parser pojedynczej treści oraz dekoder
strumienia. Jego jedyną zależnością zewnętrzną jest utf8proc (UTF-8 i NFC).
Network zależy od ADC; ADC nie zależy od Network ani Core.
Każdy `Connection` ma własny dekoder, używany tylko w wątku pętli `epoll`.
Wyniki są przetwarzane synchronicznie i zliczane, bez kolejki i dispatchu.
Niepoprawna ramka nie rzuca wyjątku ani nie zatrzymuje innych klientów;
brak pamięci i inne awarie infrastruktury nadal mogą zakończyć proces.
Na zamknięciu klienta `finish()` raportuje nieukończoną ramkę. Stop najpierw
zamyka listenery, potem klientów i wypisuje zbiorcze liczniki Network oraz ADC.
Szczegóły kontraktu i limitów: [ADC_PROTOCOL.md](ADC_PROTOCOL.md).

## Plan modułów

Kolejne moduły będą oddzielane od Core: automat sesji ADC,
Users/Sessions, Chat, Search, Transfers, uwierzytelnianie i API.
WebAdmin pozostaje osobnym frontendem React/TypeScript/Vite, serwowanym przez
Apache2. Logging obsługuje współdzielony logger stderr lub append do pliku.

Network już zwija częściowy start i zamyka gniazda. Rozszerzanie modułów wymaga
zatrzymania przyjmowania połączeń, zakończenia sesji i zwalniania zasobów
w odwrotnej kolejności startu. Nie ma jeszcze rejestru ani interfejsu modułów.

## Konfiguracja i logowanie — alpha-0.1.5

`dc24h_configuration` zawiera Settings, ograniczony parser INI, składanie CLI i walidację zależności pól. main najpierw sprawdza ustawienia, potem tworzy logger, sygnały, transport i Core. Błąd otwarcia logu daje kod 1; działający logger pozostaje best effort. Zobacz [CONFIGURATION.md](CONFIGURATION.md).

## Logging — alpha-0.1.6

Start raportuje niesekretne liczby nasłuchów/limity przed przeniesieniem opcji do Network. Reaktor przekazuje stałe kategorie ostrzeżeń i typowane zdarzenia. Mutex chroni ograniczone wiersze UTF-8, bez nowego wątku i nieograniczonej kolejki logów. Zobacz [LOGGING.md](LOGGING.md).

## Security — alpha-0.1.7

Niezależny target C++20 `dc24h_security` implementuje walidację i limity IP/ramek sterowane zegarem, bez zależności od logów, gniazd i Core. Network posiada limiter IP, klient swój stan tempa/czasu. Stop i rollback zwalniają sloty w reaktorze; telemetria podaje liczby, nie adresy. Moduł dotyczy zasobów, nie uwierzytelniania kont. Zobacz [SECURITY.md](SECURITY.md).

## Database — alpha-0.1.8

`dc24h_database` ukrywa uchwyty Connector/C za niekopiowalnym połączeniem RAII. Configuration zawiera opcje; main opcjonalnie łączy przed startem Network i niszczy bazę po zakończeniu Core/Network. SQL nie trafia do reaktora ani Core. Jawne sprawdzenie/migracja nie tworzą loggera i transportu. Brak automatycznych migracji i reconnect. Zobacz [DATABASE.md](DATABASE.md).
