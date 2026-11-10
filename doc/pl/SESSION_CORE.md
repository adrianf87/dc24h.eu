# Session Core — alpha-0.3.1

**Bieżący etap — alpha-0.3.9:** [User List](USER_LIST.md) tworzy samodzielne, deterministyczne powitanie ze stronicowanej listy identified/ready i zwalidowanych metadanych publicznych ADC. Sprawdza cały rozmiar po kodowaniu przed kolejkowaniem/rejestracją. Własny INF jest ostatni; dalsze join/update/quit zachowują synchronizację FIFO. Starsze opisy modułów zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

**Etap dostarczony w alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) posiada indeksy SID/CID/nick wraz z żywymi modelami. Odczyt const i zarządzane zmiany cyklu życia/User zachowują unikalność przy przyjęciu, zmianie nicku i sprzątaniu. LoginHub zachowuje protokół i kolejki. Starsze opisy dotyczą zakresu historycznych etapów.

**Etap dostarczony w alpha-0.3.3:** [User Core](USER_CORE.md) dodaje do Session publiczny model gościa i profilu, projekcję całego zwalidowanego INF oraz aktualizacje przez bramkę cyklu życia. Powstał bezpośrednio z main z Session Core; main łączy go teraz z wcześniej scalonym alpha-0.3.2 Session Manager. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.

Nowa biblioteka C++20 `dc24h_users` jest właścicielem modelu cyklu życia sesji używanego przez rzeczywisty hub ADC/ADCS. Podstawa gałęzi: main `5bc79d0a12eb37d0869651eb48b57d2bf338d7fe`, po scaleniu ochrony negocjacji alpha-0.2.10. Protokół, reguły gości, odpowiedzi sieciowe, konfiguracja bezpieczeństwa, SQL i API v1 pozostają dotychczasowe. Historia jest zachowana; gałąź nie scala się automatycznie.

## Identyfikator i własność

`users::SessionId` jest niezerowym lokalnym identyfikatorem 64-bitowym czasu życia sesji, niezależnym od ADC SID, deskryptora gniazda, CID, nicka i konta. Jedna niekopiowalna sekwencja SessionIds należy do reaktora. Wartości rosną, mogą pomijać nieudane inicjalizacje i nie zawijają się ani nie są ponownie używane. Wyczerpanie odrzuca nowych klientów, zachowując istniejące sesje. To nie jest trwały UUID między uruchomieniami, poświadczenie ani sekret.

Network używa tej samej liczby jako tokenu epoll, identyfikatora w opcjonalnym logu i SessionId. Wartości portów/wakeup pozostają zarezerwowane. LoginHub wymaga rosnących ID i odrzuca ponowne użycie również po usunięciu, więc stare odwołanie nie rozwiąże się do zastępczej sesji. Snapshot starego ID jest pusty. Test nie obiecuje konkretnego ponownego użycia fd przez system: identyfikator od tego nie zależy.

Jeden niekopiowalny/nieprzenoszalny Session należy do każdego zarejestrowanego Peer; unique_ptr stabilizuje adres przy rehash mapy. ClientSession nadal odpowiada za ADC/state/SUP/SID/INF/Tiger i dzierżawę SID, Network/Security za gniazda/TLS i limity. W Users nie ma ukrytego gniazda, parsera ADC, SQL, OpenSSL, loggera, poświadczeń ani odczytu zegara.

## Cykl życia i czasy

| Faza Users | Znaczenie / przejście | Związek z ADC |
| --- | --- | --- |
| accepted | Zarejestrowany transport przyjęty; ADCS czeka na TLS | PROTOCOL bez wejścia aplikacyjnego |
| negotiating | ADC gotowy albo ukończony TLS | PROTOCOL i IDENTIFY przed dopuszczeniem |
| identified | Poprawna tożsamość gościa, unikalność i dopuszczenie; ograniczone powitanie w kolejce | Wewnętrzne ADC NORMAL, jeszcze bez gotowości |
| ready | Pełne własne końcowe początkowe INF wysłane wraz z LF | NORMAL, udział w routingu |
| closing | Jedna decyzja zamknięcia, bez dalszej negocjacji/aktualizacji | Stan ADC zachowany podczas opróżniania |
| closed | Zakończone sprzątanie protokołu/rejestru | Wpis usunięty; transport zamykany w tej samej turze reaktora |

ADC przechodzi z accepted do negotiating od razu w czasie przyjęcia. ADCS dopiero przez `transport_ready` po udanym TLS. Wejście aplikacyjne przed tym jest błędem osadzającego i nie zmienia modelu. Upływ terminu przed gotowością TLS zamyka natychmiast, bez kolejkowania komunikatów aplikacyjnych.

Czasy przyjęcia, negocjacji, tożsamości, gotowości, zamykania i zamknięcia mają dostarczane wartości `steady_clock::time_point`. Model sam nie czyta zegara i niczego nie planuje. Są to monotoniczne pomiary lokalne procesu, nie UTC ani daty do serializacji. Udana zmiana wymaga czasu niemalejącego; błąd zachowuje model. Niedozwolony skok, ponowna tożsamość/gotowość, rename po zamykaniu i cofnięty czas zmiany są odrzucane. Równe czasy są dozwolone.

Zamykanie i końcowe sprzątanie są idempotentne. Pierwsza przyczyna/czas pozostają stałe. `force_close` zapisuje pierwszą wymuszoną przyczynę i przełącza żądany tryb na immediate bez odnawiania terminu. `finish_close` zapisuje faktyczną końcową przyczynę/czas; bezpośrednie EOF/błąd z wcześniejszej fazy tworzy immediate closing i closed w tej samej chwili. Żądany drain nie dowodzi doręczenia powiadomień: EOF, błąd zapisu lub close_timeout mogą zakończyć inaczej. was_ready pochodzi z zachowanego czasu gotowości i pozostaje prawdą podczas zamykania.

## Publiczna tożsamość i kopie

`users::Identity` posiada tylko trzy publiczne wartości: klucz sesji protokołu (SID), klucz klienta (CID) i nick. Fabryka sprawdza niepustość, brak NUL i ograniczenia przechowywania 64, 128, 256 bajtów. To ogólne granice modelu, nie drugi walidator ADC. Adapter wcześniej stosuje ostrzejsze reguły SID/CID/Tiger/NFC/nick/IP. Sama fabryka nie potwierdza kryptografii, unikalności, konta ani roli. PID/PAS/hasła, IP i dowolne rozszerzenia INF nie są kopiowane do modelu.

Tożsamość jest przypisywana raz. Klucze sesji/klienta potem są niezmienne; tylko sprawdzony unikalny nick można zmienić przez bramkę cyklu życia. ClientInfo zachowuje pozostałe poprawne metadane protokołu. Session Core zastępuje poprzednie powielone flagi admitted/ready/closing/abort i kopie SID/CID/nicka; nie tworzy drugiej bazy użytkowników ani nie przenosi wszystkich indeksów protokołu do Users.

`LoginHub::session_model(id)` zwraca stałą pożyczoną referencję, ważną tylko do usunięcia wpisu i w wątku reaktora. `snapshot(id)` daje własną kopię albo nullopt dla nieobecnego/starego ID. Kopia zachowuje wcześniejszy nick/czasy/przyczyny po rename, zamknięciu i zniszczeniu źródła. Nie jest żywym uprawnieniem ani uchwytem zastępczego połączenia. Odczyt żywego modelu/tworzenie kopii nie jest API między wątkami; przyszła administracja musi zapewnić synchronizowaną publikację. Hub nie przechowuje historii zamkniętych sesji lub tombstone.

## Ukończenie wysyłki i sprzątanie

Model należy do LoginHub. identified powstaje po walidacji, unikalności i udanym zbudowaniu ograniczonego powitania; przepełnienie wcześniej nie publikuje tożsamości ani rezerwacji rejestru. `consume(id, bytes)` zwraca teraz true dokładnie raz, po pełnym własnym INF. Częściowa/zwykła/końcowa wysyłka lub anulowane powitanie zwracają false. Network wykorzystuje to zdarzenie do zwolnienia oczekującego miejsca i login_ready, bez drugiej trwałej flagi gotowości.

Network wyprowadza zwalnianie pending i naliczanie negocjacji z model.was_ready, a terminy logowania/TLS z model.timeline.accepted. Termin kończenia bierze z model.timeline.closing: od decyzji, nie późniejszego przebiegu pętli. Usunięto powielone czasy przyjęcia/zamykania, przyczynę i flagę gotowości Connection. Czas częściowej ramki, bezczynność, postęp wyjścia i stan I/O TLS pozostają własnością transportu.

Normalne odrzucenie/timeout/limit rozpoczyna drain ze stałą przyczyną Users. Fatalny raport klienta, przepełnienie wyjścia lub wewnętrzny błąd obserwacji wyjścia wymuszają immediate. Pierwsza przyczyna jest niezależna od końcowej transportowej (np. idle_timeout, potem close_timeout). Stałe mapowanie zachowuje typowane zdarzenia logów. Log zawiera wewnętrzne ID/transport/zdarzenie, nigdy snapshot, nick, CID, tekst statusu lub IP. Ostrzeżenie wyczerpania ID ma US/PL.

Sprzątanie ogłasza jedno samo IQUI dla dopuszczonej osoby (oprócz shutdown), usuwa jej indeksy nick/CID/SID, anuluje kolejkę, rozłącza protokół/dzierżawę SID, oznacza model closed i usuwa Peer. Limity używają historycznej gotowości, więc gotowy klient nie zwalnia cudzej rezerwacji pending. Powtórne sprzątanie jest bezpieczne. Shutdown zamyka przyjęty TLS i gotowych klientów bez czekania na wyjście; własność zasobów nadal podlega RAII/sprzątaniu wyjątków. Nie dodano trwałego audytu sesji.

## Testy i zakres

Pełny zestaw ma 47 testów CTest. `users.session` sprawdza ID i granicę UINT64_MAX, brak kopiowania/przenoszenia, granice tożsamości, legalne/błędne przejścia, monotoniczny sztuczny zegar, niezmienne klucze, własne kopie, rollback rename, pierwszą/wymuszoną/końcową przyczynę, wczesne EOF i idempotencję. `users.session.adc` sprawdza TLS, różnicę NORMAL i identified/ready, bajtowe pojedyncze ukończenie powitania, anulowanie, przepełnienie przed publikacją, stare ID, reconnect i kopie po usunięciu źródła.

`users.session.process` używa rzeczywistych TCP/OpenSSL: IDENTIFY blokuje następne pending do wysłania powitania; reconnect zachowuje publiczną tożsamość, ale dostaje nowy token; fatalny raport/TLS failure/shutdown sprzątają własne cykle bez uszkodzenia innych. Dotychczasowy ncdc ADC/ADCS sprawdza teraz różne rosnące ID, jedno login_ready i jedno zamknięcie na połączenie, z TLS-ready tylko dla ADCS. Poprzednie testy negocjacji, provider/STA/QUI/blokowanego zapisu, MariaDB, Apache i frontend pozostają wymagane.

Etap to model sesji gości w pamięci i rzeczywista integracja. Konta, role/bany, ekspozycja sesji przez HTTP/WebAdmin, mutacje, czat/wyszukiwanie/transfery, trwałe/rozproszone ID i historia pozostają planowane. Brak nowych zależności, migracji i pól JSON. Pozostałe OS/CI, dowolne awarie alokacji i produkcyjne audyty wydajności/bezpieczeństwa wymagają osobnych dowodów.

Zobacz [architekturę](ARCHITECTURE.md), [logowanie](ADC_LOGIN.md), [STA/QUI](ADC_STATUS.md), [ochronę negocjacji](NEGOTIATION_SECURITY.md) i [ADC 1.0.4](https://adc.sourceforge.io/ADC.html).
