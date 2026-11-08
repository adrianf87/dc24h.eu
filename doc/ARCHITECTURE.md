# Architektura

## Stan alpha-0.1.1

`src/main.cpp` odpowiada za CLI, przygotowanie sygnałów, uruchomienie Core
i wynik procesu. `dc24h_core` zawiera `core::Server`, `core::TerminationSignals`
oraz prosty `logging::Logger`. Nagłówki publiczne są w `include/dc24h/`.

`Server::run()` jest blokujący i jednorazowy. Stan początkowy to `created`.
Po uruchomieniu przechodzi w `running` i czeka na zmiennej warunkowej.
`request_stop()` przełącza go w `stopping` i budzi oczekiwanie. `run()`
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
plus opóźnienia planisty. Core czeka bez timeoutu na zmiennej warunkowej.

Maska sygnałów jest ustawieniem procesu programu `dc24h`, nie ogólną biblioteką
RAII: celowo pozostaje aktywna aż do wyjścia procesu. Chroni końcowe sprzątanie
przed kolejnymi SIGINT/SIGTERM. Kolejne sygnały nie wymuszają brutalnego stop.
`std::jthread` jest dołączany przed zniszczeniem Server i loggera.
Błąd oczekiwania na sygnał żąda stop i powoduje kod zakończenia 1.

Dokumentacja użytych interfejsów systemowych:
[pthread_sigmask(3)](https://man7.org/linux/man-pages/man3/pthread_sigmask.3.html)
oraz [sigtimedwait(2)](https://man7.org/linux/man-pages/man2/sigtimedwait.2.html).

## Plan modułów

Kolejne moduły będą oddzielane od Core: Network (TCP), ADC Protocol, ADCS/TLS,
Users/Sessions, Chat, Search, Transfers, Security, Database, API, Configuration.
WebAdmin pozostaje osobnym frontendem React/TypeScript/Vite, serwowanym przez
Apache2. Logging obecnie zapewnia tylko współdzielony logger stderr.

Przyszłe uruchamianie modułów wymaga zwijania częściowo rozpoczętego startu,
zatrzymania przyjmowania połączeń, zakończenia sesji i zwalniania zasobów
w odwrotnej kolejności startu. Nie ma jeszcze rejestru ani interfejsu modułów.
