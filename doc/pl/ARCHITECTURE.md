# Architektura

**Aktualny etap — alpha-0.3.1:** [Session Core](SESSION_CORE.md) dostarcza niezależny model Users i trwałe w obrębie reaktora ID sesji, podłączone do rzeczywistej gotowości, zamykania i czasów. Wcześniejsze opisy zachowują zakres pierwotnego etapu.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

W alpha-0.2.7 dc24h_adc wydziela weryfikację Tiger z INF do tiger_identity.hpp/cpp. TigerCid reprezentuje kanoniczne publiczne bajty; VerifiedIdentity powstaje tylko po poprawnej parze i nie zawiera PID. Bezpośrednie wyjście dekodowania/hash eliminuje prywatne bufory optional zwracane przez wartość; niekopiowalne lokalne bufory PID/wyniku są czyszczone przy wyjściu. INF używa weryfikatora i zachowuje publiczny wynik przy zwykłych aktualizacjach. Wrappery awarii należą wyłącznie do izolowanego testu. Zobacz [ADC_IDENTITY.md](ADC_IDENTITY.md).

W alpha-0.2.6 dc24h_adc dodaje publiczny snapshot ClientInfo, InfValidator, funkcje Tiger/base32 i ClientSession nad SidSession. Walidacja buduje ograniczonego kandydata przed zmianą stanu, sprawdza PID/CID przez Libgcrypt TIGER1, wiąże deklarowany IP z zaufanym numerycznym adresem klienta i odrzuca PD z wyniku. Dopuszczenie/unikalność pozostają zewnętrznymi decyzjami. Brak zależności od gniazd, SQL, logowania i HTTP; Network nadal używa samego parsera. Zobacz [ADC_INF.md](ADC_INF.md).

W alpha-0.2.5 dc24h_adc dodaje Sid, SidPool, SidLease i SidSession. Jedna pula huba należąca do reaktora obejmuje przyszłe sesje ADC/ADCS; ograniczona bitmapa zastępuje losowe próby unikania kolizji. Przenoszalna rezerwacja posiada aktywną wartość, a SidSession zwalnia ją po zdarzeniu terminalnym/zniszczeniu. Współdzielona własność utrzymuje stan alokatora bez zależności od gniazd, Core i Database. Parser i automat używają wspólnego kodera. Zobacz [ADC_SID.md](ADC_SID.md); podłączenie transportu pozostaje planowane.

W alpha-0.2.4 `dc24h_adc` dodaje `SupportNegotiation` dla ograniczonych funkcji klienta/huba i wyboru TIGR oraz `ProtocolConnection` łączący negocjację z automatem. Poprawny pierwszy HSUP zatwierdza protokół; pierwszy ukończony ISUP musi dokładnie odpowiadać wygenerowanym funkcjom lokalnym. Aktualizacje klienta są transakcyjne w NORMAL; hash sesji jest stały. Odbiorczy Network pozostaje niepodłączony. Zobacz [ADC_SUP.md](ADC_SUP.md).

W alpha-0.2.3 `dc24h_adc` udostępnia też `ConnectionStateMachine` po stronie huba, bez zależności od gniazd, logów, bazy i Core. Zachowuje tylko czteroznakowy SID i stałe flagi handshake. Network nadal używa samego parsera; [ADC_STATE.md](ADC_STATE.md) określa granicę integracji i jawne zatwierdzanie.

W alpha-0.2.2 niezależna biblioteka ADC sprawdza też układy argumentów BASE i udostępnia funkcje pól nazwanych/liczb. Network używa pełniejszej składni przez dotychczasowy dekoder; bez nowej zależności od Core/bazy/frontendu i bez wykonania/wyjścia. Zobacz [ADC_PROTOCOL.md](ADC_PROTOCOL.md).

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
Users/Sessions, Chat, Search, Transfers, uwierzytelnianie i operacje API zmieniające stan.
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

## Lokalne API — alpha-0.1.9

`dc24h_api` posiada ograniczony parser HTTP, weryfikację Bearer, nieblokujące gniazdo Unix i jeden wątek RAII. Zależy od Logging/Security, nie Core, Network i Database. Main przekazuje bezpieczny współbieżnie callback stanu i callback stopu po krytycznym błędzie infrastruktury. API startuje przed pętlą Core i jest zatrzymywane/dołączane przed zniszczeniem celów callbacków, także po błędzie startu Network. JSON zawiera tylko typowane liczniki/flagi; API nie wykonuje SQL ani zmian administracyjnych. Wspólny czytnik prywatnych plików przeniesiono do Security, zachowując błędy Database. Zobacz [API.md](API.md).

## WebAdmin — alpha-0.1.10

Frontend jest niezależny od budowania/działania C++. Kod przeglądarki sprawdza schemat API i oferuje widoki monitorowania; zapisuje wyłącznie język. Opcjonalny adapter Vite trzyma sekret po stronie serwera, wymaga loopback/same-origin i przekazuje dwie trasy odczytu przez gniazdo tego samego UID. Build produkcyjny jest statyczny, bez adaptera/tokena. Apache może serwować zasoby; projekt produkcyjnej bramy/sesji/ról pozostaje osobny. CMake opcjonalnie instaluje gotowe zasoby przez `DC24H_INSTALL_WEBADMIN=ON`, domyślnie OFF. Zobacz [WEBADMIN.md](WEBADMIN.md).

## Apache2 HTTPS — alpha-0.1.11

Osobny instalowany przykład serwuje gotowe zasoby WebAdmin przez TLS 1.2/1.3 i przekierowuje HTTP na stałą nazwę kanoniczną. Zachowuje lokalny dostęp i blokadę prywatnych tras, bez proxy Unix API i zmian transportów C++. Testy ładują tę politykę pod niezależnym ServerRoot; certyfikaty i aktywacja usługi hosta należą do operatora. Zobacz [APACHE2_HTTPS.md](APACHE2_HTTPS.md).
