# Rozwój projektu

**Bieżący etap — alpha-0.3.10:** [INF Updates](INF_UPDATES.md) waliduje i przygotowuje aktualizacje gotowej sesji przed wspólnym zatwierdzeniem ClientInfo, User/profilu i nicka. Rozsyła zweryfikowane wskazane pola i jawne anulowania; początkowe listy pozostają pełnymi snapshotami publicznymi. Aktualizacja pusta/tylko z nieznanymi polami nie daje publicznej ramki. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

**Etap dostarczony w alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) posiada indeksy SID/CID/nick wraz z żywymi modelami. Odczyt const i zarządzane zmiany cyklu życia/User zachowują unikalność przy przyjęciu, zmianie nicku i sprzątaniu. LoginHub zachowuje protokół i kolejki. Starsze opisy dotyczą zakresu historycznych etapów.

**Etap dostarczony w alpha-0.3.3:** [User Core](USER_CORE.md) dodaje do Session publiczny model gościa i profilu, projekcję całego zwalidowanego INF oraz aktualizacje przez bramkę cyklu życia. Powstał bezpośrednio z main z Session Core; main łączy go teraz z wcześniej scalonym alpha-0.3.2 Session Manager. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.

**Aktualny etap — alpha-0.3.1:** [Session Core](SESSION_CORE.md) dostarcza niezależny model Users i trwałe w obrębie reaktora ID sesji, podłączone do rzeczywistej gotowości, zamykania i czasów. Wcześniejsze opisy zachowują zakres pierwotnego etapu.

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
