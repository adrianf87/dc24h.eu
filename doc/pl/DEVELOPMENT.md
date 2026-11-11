# Rozwój projektu

**Bieżący etap — alpha-0.5.2:** [ADC SCH](ADC_SCH.md) dodaje własny typowany parser SCH i odbiór w ADC/ADCS po sprawdzeniu stanu/źródła. Sprawdza pola, selektory, limity i kanoniczne TR; poprawne zapytania otrzymują informację o braku routingu, błędne semantycznie odwracalną odmowę, a zapytania bez kryteriów są pomijane. Routing/wyniki pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.1:** [Search Core](SEARCH_CORE.md) dodaje niezależną bibliotekę C++20 dla ograniczonych, niezmiennych zapytań tekstowych, wyboru gotowych sesji/funkcji oraz dispatch związanego z czasem życia i domeną silnika. Parser, serializacja i podłączenie SCH/RES do transportu pozostają kolejnymi etapami; działający serwer nadal zwraca jawną odmowę nieobsługiwanej komendy.

**Etap dostarczony w alpha-0.4.12:** [Statystyki czatu WebAdmin](WEBADMIN_CHAT.md) dodają osobny widok Czat z dziewięcioma licznikami zbiorczymi, sekcjami wiadomości/kolejek, opisami US/PL, odświeżaniem, obsługą błędów/restartu i bezpiecznym eksportem.

**Etap dostarczony w alpha-0.4.11:** [Chat Logging](CHAT_LOGGING.md) dodaje opcjonalne logi wyników bez treści, z limitem tempa, oraz nasycane liczniki klienckiego B/D/E MSG w uwierzytelnionym API i WebAdmin US/PL. Przyjęcie do kolejki jest odróżnione od dostarczenia przez transport.

**Etap dostarczony w alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) grupuje startowe ustawienia czatu i dodaje limit odkodowanego tekstu, niezależne przełączniki publicznego/prywatnego/akcji i opcjonalne własne powitanie. Domyślne wartości zachowują działanie; odwracalne odmowy nie omijają walidacji ani Anti-Flood.

**Etap dostarczony w alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

**Etap dostarczony w alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizuje sprawdzanie tekstu, SID i gotowości/własności nadawcy w obu adapterach. Wybór życia odbiorcy i ponowna kontrola dispatch pozostają obowiązkowe; systemowe IMSG jest oddzielone.

**Etap dostarczony w alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) dodaje zaufane systemowe IMSG powiązane z życiem sesji i stałe powitanie po własnym BINF. Klient nie może nadawać komunikatów systemowych; zachowano limity kolejek i gotowość przy końcowym LF.

**Etap dostarczony w alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizuje dekodowanie PM/ME i kanoniczne wyjście kodeka oraz obu adapterów czatu. Brak ME, ME0 i ME1 pozostają rozróżniane; PM nie nadaje uprawnień do dostarczania.

**Etap dostarczony w alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizuje wybór zakresu MSG i dostarczanie do ograniczonych kolejek. Plany są powiązane z domeną macierzystego Chat Core i ID życia sesji; prywatne wiadomości nigdy nie trafiają zastępczo na czat publiczny.

**Etap dostarczony w alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) dodaje DMSG tylko do celu i EMSG do celu oraz nadawcy, pojedyncze wiadomości do siebie, powiązanie celu z ID życia i kontrolę PM. Czat publiczny pozostaje oddzielony; grupy/historia/moderacja to kolejne etapy.

**Etap dostarczony w alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) dodaje osobny adapter polityki i rozsyłania BMSG oraz jedną wspólną niemutowalną ramkę na publikację. Offsety i logiczne budżety odbiorców pozostają niezależne; zachowano bramki źródła/stanu/prywatności i FIFO.

**Etap dostarczony w alpha-0.4.2:** [Chat Core](CHAT_CORE.md) dodaje osobny silnik wiadomości niezależny od protokołu oraz publiczne BMSG, z kopią dla nadawcy i /me. Czat prywatny/grupowy/feature/hub, moderacja i historia pozostają osobnymi etapami.

**Etap dostarczony w alpha-0.4.1:** [ADC MSG](ADC_MSG.md) dodaje kodek MSG z typowanymi polami PM/ME, sprawdzanym budowaniem komunikatów, ograniczoną serializacją i walidacją w maszynie stanów. Dostarczanie czatu i polityka rozmów pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.3.13:** [Statistics](STATISTICS.md) dodaje liczniki cyklu życia w czasie stałym, użytkowników online/szczyt i sumę logowań, atomowe statystyki transportu, pola chronionego API oraz metryki WebAdmin US/PL. Starsze etapy poniżej zachowują swój zakres historyczny.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

**Etap dostarczony w alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) posiada indeksy SID/CID/nick wraz z żywymi modelami. Odczyt const i zarządzane zmiany cyklu życia/User zachowują unikalność przy przyjęciu, zmianie nicku i sprzątaniu. LoginHub zachowuje protokół i kolejki. Starsze opisy dotyczą zakresu historycznych etapów.

**Etap dostarczony w alpha-0.3.3:** [User Core](USER_CORE.md) dodaje do Session publiczny model gościa i profilu, projekcję całego zwalidowanego INF oraz aktualizacje przez bramkę cyklu życia. Powstał bezpośrednio z main z Session Core; main łączy go teraz z wcześniej scalonym alpha-0.3.2 Session Manager. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.


**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Podstawę alpha-0.2 i priorytety odbioru opisuje [audyt alpha-0.1](AUDIT_ALPHA_0.1.md). Numerowane gałęzie nadal powstają z aktualnego main. `alpha-0.2` oznacza sprawdzony commit audytu. Pełna weryfikacja używa `DC24H_REQUIRE_INTEGRATION_TESTS=ON`; zachować spójność project.metadata, npm lock, wersji programu/panelu i obu języków dokumentacji.

Pracuj na gałęziach wydań, zachowując historię `main`. Aktualny etap:
`alpha-0.2.7`. Każdy istotny etap otrzymuje opisowy commit. Nie dodawaj
artefaktów kompilacji, sekretów ani zależności vendored bez uzasadnienia.

| Katalog | Rola |
| --- | --- |
| `src/core`, `include/dc24h/core` | Core i adapter sygnałów |
| `src/network`, `include/dc24h/network` | TCP, listenery, klienci i epoll |
| `src/adc`, `include/dc24h/adc` | Parser składni i dekoder strumienia ADC |
| `src/logging`, `include/dc24h/logging` | Logowanie |
| `cmake/` | Generowana informacja o wersji |
| `tests/` | Testy C++ i testy procesu w Pythonie |
| `config/` | Przykładowy INI i lokalne ustawienia |
| `src/database`, `include/dc24h/database` | Adapter połączenia MariaDB i zapytań |
| `database/migrations/` | Niezmienna migracja rejestru wersji MariaDB |
| `webadmin/` | Frontend React/TypeScript/Vite i lokalny adapter |
| `doc/` | Dokumentacja techniczna |
| `.github/workflows/` | Automatyzacja budowania/testów |

Nowe moduły dodawaj do odrębnych katalogów. Stosuj C++20, RAII, jawne
właścicielstwo i sprawdzanie błędów. Nie używaj globalnych obiektów silnika.
Zachowuj niezależność Core od UI, SQL i parsowania protokołu.

Przed commitem wykonaj budowanie i CTest; aktualizuj `doc/` i `CHANGELOG.md`.
Wszystkie deklaracje ukończenia funkcji muszą odpowiadać kodowi i testom.

Nowe gałęzie twórz z aktualnego `main`. Gałąź alpha-0.1.4 zachowuje przez merge historię referencyjnej chatgpt-alpha-0.1.4. Kod i domyślne komunikaty: EN/US UTF-8; dokumentacja: `doc/us` i `doc/pl`, katalogi komunikatów: `languages/us` i `languages/pl`. Kolejność wpisów katalogu musi być zgodna. Nie tłumacz danych protokołu.

alpha-0.1.5 powstaje z main po jawnym scaleniu alpha-0.1.4. Kod konfiguracji/loggera i testy chatgpt-alpha-0.1.5 sprawdzono i dostosowano, zachowując poprawki TLS i języków z main. Nie importowano dodatkowego katalogu en/. Usunięto zbędne indeksy modułów w korzeniu doc/.

alpha-0.1.6 powstaje z main po alpha-0.1.5. Sprawdzono i dostosowano zmiany loggera chatgpt-alpha-0.1.6; zachowano bezpieczniejsze INI/pliki, planowanie TLS i US/PL z main. Używaj enum ConnectionEvent, bez ramek i danych klientów w logach.

alpha-0.1.7 powstaje z main po scaleniu alpha-0.1.6. Sprawdzono i dostosowano kod/testy chatgpt-alpha-0.1.7. Zachowuj ograniczoną pamięć, kanoniczne IP, jedno zwolnienie slotu i bezwzględne terminy ramek; nie loguj kluczy IP i danych. Security pozostaje niezależne od planowanego uwierzytelniania.

## Etap Database

alpha-0.1.8 powstaje z main po alpha-0.1.7. Nie przenoś SQL i haseł do Core/Network, wiąż wartości zapytań i nie edytuj zastosowanych migracji. Przed nieidempotentnym DDL dodaj obsługę częściowej awarii. Testy bazy muszą mieć własny katalog danych/socket i `--no-defaults`; nie korzystaj z bazy dewelopera.

## Etap API

alpha-0.1.9 powstaje z main po scaleniu alpha-0.1.8. Zachowuj wąski kontrakt HTTP bez body i lokalne zabezpieczenia. Zmiany stanu, parser body, udostępnienie Apache i token w przeglądarce wymagają projektu uprawnień/ról/audytu. Callbacki muszą być bezpieczne współbieżnie, ograniczone czasowo i żyć dłużej niż API. Testy mają używać tymczasowych prywatnych gniazd/tokenów oraz weryfikować liczniki ncdc i stan MariaDB przez rzeczywiste API.

## Etap WebAdmin

alpha-0.1.10 powstaje z main po alpha-0.1.9. Oddzielaj schemat/formatowanie przeglądarki od serwerowego adaptera Vite, zachowuj parzystość EN/PL, nie dodawaj sekretów/node_modules/dist/wyników testów do Git i aktualizuj package-lock przy zależnościach. Sprawdzaj build, testy jednostkowe/przeglądarkowe/integrację z C++ i istniejące testy serwera. Zmiany administracyjne i produkcyjne logowanie WWW wymagają osobnego projektu.

## Etap Apache2

alpha-0.1.11 powstaje z main po alpha-0.1.10. Oddzielaj aktywację hosta, rzeczywiste certyfikaty i reload od tworzenia/testowania przykładu. Testuj dokładnie dostarczaną politykę TLS/dostępu/nagłówków z tymczasowymi ścieżkami/portami, bez konfiguracji hosta. Sukces apache2 -t nie dowodzi zgodności certyfikatu/klucza; sprawdzaj inicjalizację. Zachowuj oba języki dokumentacji i spójne wersje fixture.

## Etap ADC Parser

alpha-0.2.2 powstaje z main po jawnym scaleniu alpha-0.2/alpha-0.2.1. Oddzielać składnię argumentów BASE od autoryzacji, tożsamości i wykonania. Zachować nieinterpretowane układy rozszerzeń, kolejność pól i powtórzenia. NamedParameterView wskazuje odkodowany token; nie przechowywać go po modyfikacji/zniszczeniu. Nowe reguły wymagają testów aktywnych w Release i porównywania pełnych wyników strumienia, następnie TCP, TLS i obu transportów ncdc.

## ADC State — alpha-0.2.3

Gałąź powstaje z main w `4c98e10` po scaleniu parsera. Zachować [kontrakt stanów](ADC_STATE.md), jedną niekopiowalną instancję na przyszłe połączenie, jawne zatwierdzanie semantyczne, brak zmian po odrzuceniu i terminalne rozłączenie. Testy muszą działać w Release; każda nowa komenda wymaga aktualizacji macierzy stanów, kierunków i SID. Nie wnioskować o uwierzytelnieniu z ramki ani nie awansować stanu po samym dodaniu odpowiedzi do kolejki. Integracja transportu/wyjścia, walidacja i unikalne SID wymagają osobnego odbioru.

## SUP — alpha-0.2.4

Gałąź z main w `1dd0b9f` po scaleniu ADC State. Oddzielać semantykę SUP, kolejność protokołu i obliczanie tożsamości/hash. Przy przyszłej integracji transportu preferować ProtocolConnection względem ręcznego zatwierdzania protokołu w automacie niskiego poziomu. Nowy hash wymaga jawnego rejestru/metadanych i testów wyboru/kolejności odpowiedzi; nie rozpoznawać hash po nieznanej nazwie. Zachować transakcyjność, stały hash sesji, ograniczony łączny zbiór funkcji i deterministyczną odpowiedź lokalną. Sukces fixture ncdc nie oznacza logowania w hubie. Zobacz [ADC_SUP.md](ADC_SUP.md).

## SID — alpha-0.2.5

Gałąź z aktualnego main w `350a43f`. Używać jednej wspólnej SidPool na hub i unikalnych przenoszalnych uchwytów; nie dodawać zwalniania przez sam SID ani kopiowalnej własności. Zachować przydział/zwalnianie bez alokacji, koder 20-bitowy, test pełnej przestrzeni i granic słów oraz jednowątkowy czas życia. SidSession zastępuje dowolny ISID wywołującego przy przyszłej integracji transportu. Sprawdzać sprzątanie przed bind, terminalne odpowiedzi, destrukcję i ponowne użycie; nie obserwować ukończenia po samym zakolejkowaniu. Własność SID nie potwierdza tożsamości osoby. Zobacz [ADC_SID.md](ADC_SID.md).

## INF — alpha-0.2.6

Gałąź z main w `8552406` po scaleniu SID. Budować z libgcrypt20-dev; nowe znane pola wymagają jawnego typu/zakresu/budżetu i testów anulowania. Zachować kanoniczne base32/kolejność bajtów TIGER1, powiązanie SID/IP, stały CID, brak PD w zachowaniu/eksporcie i kandydata przed zatwierdzeniem stanu. Przyszła integracja powinna używać ClientSession; samo zatwierdzenie niższego automatu nie waliduje INF. Oddzielać rejestr/dopuszczenie i nie przyznawać NORMAL na podstawie samego parsowania INF. Nowa zależność kryptograficzna musi być w obu zadaniach CI i instalacji. Zobacz [ADC_INF.md](ADC_INF.md).

## CID/PID — alpha-0.2.7

Gałąź z main w `9d3443a` po scaleniu INF. Prywatny PID weryfikować przez verify_tiger_identity; nie powielać porównywania tekstowych hash w konsumentach pól. Nie dodawać PID do publicznych wyników. Zachować TIGER1/kanoniczne bity końca, czyszczenie po błędzie i każdym wyjściu oraz brak fallback/globalnych zmian kryptografii. Wrappery awarii są tylko testowe. Sprawdzać błędne kodowanie, mismatch, brak dostawcy, odzyskanie i rzeczywisty ncdc. Zobacz [ADC_IDENTITY.md](ADC_IDENTITY.md).
