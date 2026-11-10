# Sieć i proces

**Bieżący etap — alpha-0.3.13:** [Statistics](STATISTICS.md) dodaje liczniki cyklu życia w czasie stałym, użytkowników online/szczyt i sumę logowań, atomowe statystyki transportu, pola chronionego API oraz metryki WebAdmin US/PL. Starsze etapy poniżej zachowują swój zakres historyczny.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.


**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

## Transport TCP (alpha-0.1.2) i parser ADC (alpha-0.1.3)

`network::TcpServer` obsługuje nieblokujące gniazda IPv4/IPv6 w pojedynczym
wątku pętli `epoll` (level-triggered). Nie tworzy wątku na klienta.
Domyślny nasłuch: **127.0.0.1:1511**, limit 1024 aktywnych połączeń,
timeout bezczynności 120 sekund. Zmiana adresu i portu wymaga jawnej opcji CLI.

```sh
./build/dc24h --listen 127.0.0.1:1511 --listen '[::1]:1512' \
  --max-connections 512 --idle-timeout 60
./build/dc24h --listen 127.0.0.1:0
```

Można wskazać do 16 listenerów. Pierwsze `--listen` lub `--adcs-listen` zastępuje domyślny
nasłuch, kolejne dodają adresy. Samo ADCS nie dodaje zwykłego TCP. Port 0 wybiera wolny port systemowy, podany
w logu `Network listening on ...`. Komunikaty gotowości pojawiają się dopiero
po poprawnym uruchomieniu wszystkich listenerów. `Core running` oznacza start
silnika, a nie gotowość portów. Nie są obsługiwane nazwy DNS ani adresy IPv6
z identyfikatorem strefy. IPv6 używa `IPV6_V6ONLY`; IPv4 konfiguruje się osobno.

Serwer odbiera bajty i przekazuje je do osobnego dekodera ADC każdego klienta.
Parsuje i zlicza ramki, ale nie odpowiada, nie wykonuje echo, nie uwierzytelnia
i nie przekazuje wiadomości. Szczegóły: [ADC_PROTOCOL.md](ADC_PROTOCOL.md). TLS/ADCS obsługuje szyfrowany transport od alpha-0.1.4; brak buforów wyjściowych. Szczegóły: [ADCS_TLS.md](ADCS_TLS.md). Podłączenie
klienta Direct Connect nie zakończy się logowaniem do huba.

## Połączenia, limity i błędy

Każde gniazdo ma właściciela RAII, `SOCK_NONBLOCK` i `SOCK_CLOEXEC`.
Nadmiarowe połączenie jest zamykane, gdy pętla je przyjmie; systemowy handshake
TCP może wcześniej zakończyć się sukcesem. `accepted` liczy klientów dopuszczonych
do pętli, a `rejected` klientów odrzuconych przez limit lub brak zasobów watch.
Limit jest wspólny dla wszystkich portów; nie obejmuje kolejki kernela.

Ostatni poprawny odczyt odświeża timeout klienta. Zamknięcie przez peer, FIN
oraz błędy odczytu/reset zamykają tylko dane połączenie i zwalniają jego slot.
Kontrola bezczynności następuje w pętli z oczekiwaniem do 250 ms, więc timeout
może przekroczyć ustawioną wartość o ten interwał i opóźnienie planisty.

Jedna partia przyjmowania obejmuje do 64 klientów, a odczyt jednego zdarzenia
do 64 KiB. Stały bufor odczytu ma 8 KiB. Każdy dekoder ADC przechowuje do 16 KiB
nieukończonej treści. Po przekroczeniu limitu odrzuca dane aż do LF.
Ograniczenie partii umożliwia obsługę pozostałych klientów oraz stopu przy ciągłym
ruchu. Nie jest to ochrona przed DDoS ani limit przepustowości; limity IP dodaje Security opisane poniżej.

Przejściowe błędy `accept4` są ponawiane. Po wyczerpaniu deskryptorów/pamięci
lub limitu watch `epoll` nasłuchy są wyłączane z `epoll` na sekundę, aby uniknąć
aktywnej pętli i zalewu logów; istniejące połączenia i stop pozostają obsługiwane.
Zajęty port i błędny start zwracają kod 1, zwalniając także wcześniej otwarte
listenery. Nieodwracalny błąd listenera/reactora kończy proces z kodem 1.
Limit aplikacji nie podnosi systemowego `RLIMIT_NOFILE` ani limitów kernela.

## Zatrzymanie

SIGINT/SIGTERM wywołują `Core::request_stop()`, a następnie stop Network.
Atomowa flaga jest zapamiętywana również przed startem, a zapis do `eventfd`
budzi `epoll_wait` bez oczekiwania na timeout klienta. Gniazda są zamykane
wyłącznie w wątku pętli. Stop nie oczekuje na peer i nie opróżnia wiadomości
aplikacyjnych — takie kolejki jeszcze nie istnieją. Log zatrzymania zawiera
liczniki przyjętych/odrzuconych połączeń i odebranych bajtów aplikacji (po odszyfrowaniu w TLS). Oddzielny log
ADC zawiera liczniki parsowania, bez treści danych.

| Sygnał | Obsługa |
| --- | --- |
| SIGINT (Ctrl+C) | Kontrolowany stop, kod 0 |
| SIGTERM | Kontrolowany stop, kod 0 |
| Powtarzane SIGINT/SIGTERM | Brak ponownego sprzątania i eskalacji |
| SIGHUP | Brak własnej obsługi; nie służy do reload |
| SIGKILL/SIGSTOP | Nie są przechwytywane |

Interfejsy systemowe: [epoll(7)](https://man7.org/linux/man-pages/man7/epoll.7.html),
[accept(2)/accept4](https://man7.org/linux/man-pages/man2/accept.2.html),
[eventfd(2)](https://man7.org/linux/man-pages/man2/eventfd.2.html).

## Źródło ustawień

Od alpha-0.1.5 adresy i limity pochodzą z INI oraz CLI. Lista CLI zastępuje dany transport i zachowuje drugi jawny transport. Szczegóły: [konfiguracja](CONFIGURATION.md). Domyślne wartości i semantyka stopu pozostają zachowane.

## Integracja logów

Opcjonalne logi INFO przyjęcia/gotowości TLS/zamknięcia mają wyłącznie wewnętrzny token i transport. Zwykła bezczynność i termin nieukończonego TLS mają osobne zdarzenia/kategorie. Błędne ramki, limity, TLS i I/O mają niezależne budżety ostrzeżeń na pięć sekund, bez treści ramek i adresów klientów. Liczniki pozostają zachowane. Zobacz [LOGGING.md](LOGGING.md).

## Security — alpha-0.1.7

Opcje Network zawierają `security::Limits`. Jeden ograniczony `PeerLimiter` obejmuje wszystkie porty TCP/TLS, z kanonicznymi IP kernela przechowywanymi tylko w RAM. Sprawdzenie poprzedza TLS/watch; slot jest zwalniany po błędzie inicjalizacji i zamknięciu. Każdy klient ma osobne tempo ramek, liczbę błędów i terminy TLS/częściowej ramki. Kontrola działa przed/po I/O i przed segmentami, poza przeglądem okresowym. INFO i `statistics()` dodają security_rejected/security_closed, zachowując wcześniejsze liczniki. [SECURITY.md](SECURITY.md) podaje wartości, okna i ograniczenia.

## Gniazdo administracyjne — alpha-0.1.9

Opcjonalne lokalne HTTP API ma osobne prywatne gniazdo Unix i wątek, niezależne od listenerów ADC/ADCS. Limity klientów/czasu API są niezależne od liczników bezpieczeństwa TCP/TLS. Nie jest otwierany port HTTP; Apache2 nie bierze udziału w testach transportu. Zobacz [API.md](API.md).

## Granica integracji stanów — alpha-0.2.3

Nowa [biblioteka ADC State](ADC_STATE.md) jest niezależna od transportu i nie jest jeszcze dołączona do obiektów Client. Zachowanie ADC/ADCS i liczniki składni pozostają dotychczasowe. Podłączenie wymaga ograniczonego uporządkowanego wyjścia, zdarzeń ukończenia wysyłki, walidacji protokołu/tożsamości i alokatora unikalnych SID; automat wyłącznie odbiorczy odrzucałby poprawnych klientów bez możliwości odpowiedzi. Terminy ramek/TLS pozostają w Network; przyszły termin sesji musi ograniczać całe logowanie niezależnie od keepalive.

## Adapter SUP — alpha-0.2.4

[Adapter SUP/stanów](ADC_SUP.md) realizuje wybór funkcji/hash jako samodzielny moduł C++. Network nadal używa samego parsera i nie wysyła odpowiedzi SUP. Nowy fixture SUP łączy prawdziwy ncdc z tymczasowymi gniazdami/TLS Pythona i sterownikiem modułu C++; dotychczasowe testy ADC/ADCS nadal obejmują rzeczywisty reaktor C++. Testy potwierdzają różne granice integracji. Podłączyć ProtocolConnection dopiero z ograniczonym wyjściem, zdarzeniami ukończenia wysyłek, unikalnymi SID, obliczaniem/walidacją tożsamości/hash i terminami logowania.

## Granica SID — alpha-0.2.5

[SidSession](ADC_SID.md) dodaje własność ISID opartą na alokatorze w niezależnym stosie protokołu. Jedna współdzielona SidPool musi obejmować wszystkie listenery huba na wątku reaktora. Obecny Network Client jeszcze nie posiada SidSession; ścieżka sieciowa nadal jest odbiorcza. Integracja musi zatwierdzić ISUP, zarezerwować SID, wysłać/zatwierdzić ISID, anulować/opróżnić wyjście starej sesji i usunąć referencje rejestru przed zwolnieniem wartości. SID może być ponownie użyty; deskryptory gniazd i skopiowane wartości SID nie są tokenami własności. Ograniczone wyjście, rejestr/kolejność QUIT, tożsamość i terminy pozostają przyszłym etapem.

## Granica INF — alpha-0.2.6

[ClientSession](ADC_INF.md) sprawdza teraz wewnętrzną poprawność informacji klienta w niezależnym stosie. Przyszłe podłączenie Network musi przekazywać rzeczywisty adres z kernela (nie deklarację INF), serializować odkodowane publiczne snapshoty z escape i wymuszać globalną unikalność nicku/CID przed zatwierdzeniem/ogłoszeniem informacji. Odbiorczy reaktor pozostaje bez zmian. Nowy fixture ncdc sprawdza rzeczywisty PID/CID przez TCP/TLS Pythona; dotychczasowe testy transportu C++ są osobne. Nadal wymagane są uporządkowane ograniczone wyjście, rejestr/cykl życia i terminy logowania.

## Granica CID/PID — alpha-0.2.7

ClientSession/INF używa teraz dedykowanego [weryfikatora Tiger](ADC_IDENTITY.md), ale rzeczywisty reaktor nadal jest odbiorczy. Nowy fixture ncdc odrzuca zmieniony kanoniczny CID przed przyjęciem prawdziwej pary, zachowuje tylko wynik publiczny i pozostaje w IDENTIFY. Gniazda/TLS należą do Pythona; testy transportu C++ ADC/ADCS są osobne. Po poprawnym PID/CID nadal potrzebne są unikalność/dopuszczenie, ograniczone wyjście i cykl całego logowania przed podłączeniem.
