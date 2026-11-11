# Plan rozwoju

**Bieżący etap — alpha-0.5.1:** [Search Core](SEARCH_CORE.md) dodaje niezależną bibliotekę C++20 dla ograniczonych, niezmiennych zapytań tekstowych, wyboru gotowych sesji/funkcji oraz dispatch związanego z czasem życia i domeną silnika. Parser, serializacja i podłączenie SCH/RES do transportu pozostają kolejnymi etapami; działający serwer nadal zwraca jawną odmowę nieobsługiwanej komendy.

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

**Aktualna integracja — alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

| Etap | Zaimplementowany zakres |
| --- | --- |
| alpha-0.1.1 Core | Cykl życia, start/stop, sygnały i logger |
| alpha-0.1.2 Network | TCP IPv4/IPv6, wiele portów i klientów, limity zasobów |
| alpha-0.1.3 ADC | Ograniczony parser strumienia, komendy i walidacja danych |
| alpha-0.1.4 ADCS | Certyfikaty, nieblokujący TLS, testy, dokumentacja i komunikaty US/PL |
| alpha-0.1.5 Configuration | INI, nadpisania CLI, walidacja ścieżek/limitów i logowanie append |
| alpha-0.1.6 Logging | Podsumowania startu, WARN, zdarzenia połączeń, ograniczone UTF-8 i budżety |
| alpha-0.1.7 Security | Ograniczone przyjęcia IP, ramki i terminy bezwzględne |
| alpha-0.1.8 Database | Opcjonalne połączenie lokalne MariaDB, prepared statements i jawny rejestr migracji |
| alpha-0.1.9 API | Lokalne uwierzytelnione HTTP/JSON: stan i health, tylko odczyt |
| alpha-0.1.10 WebAdmin | Układ React/TypeScript/Vite tylko do odczytu, EN/PL, lokalny adapter i statyczny przykład Apache |
| alpha-0.1.11 Apache2 | Przykład statycznego HTTPS, przekierowanie, TLS/nagłówki i testy tymczasowego Apache |
| alpha-0.2.1 Project | Audyt alpha-0.1, poprawki spójności, wymagane integracje w CI i podstawa alpha-0.2 |
| alpha-0.2.2 ADC Parser | Składnia argumentów BASE, pola nazwane, liczby i odzyskiwanie TCP/TLS |
| alpha-0.2.3 ADC State | Niezależny automat huba, jawne zatwierdzanie i kontrola komend/kierunków/SID |
| alpha-0.2.4 SUP | Funkcje AD/RM, BASE/wspólny TIGR, limity i połączenie z automatem |
| alpha-0.2.5 SID | Koder 20-bitowy, ograniczona wspólna pula, rezerwacje RAII i zarządzane sesje SID |
| alpha-0.2.6 INF | Ograniczone metadane, Tiger tożsamości, kontrola IP i publiczne snapshoty bez PD |
| alpha-0.2.7 CID/PID | Dedykowany weryfikator Tiger, wynik publiczny, czyszczenie buforów i testy awarii |
| alpha-0.2.8 Login | NORMAL gości, ograniczone uporządkowane wyjście, rejestr nick/CID, publiczna obecność i reconnect ncdc |
| alpha-0.2.9 STA/QUI | Semantyka błędów, routing STA, końcowe własne QUI i ograniczone opróżnianie |
| alpha-0.2.10 Security | Przyjęcia oczekujące, łączne budżety negocjacji i limity przed weryfikacją |
| alpha-0.3.1 Session Core | Model Users, identyfikator czasu życia, kopie i rzeczywisty cykl połączenia |
| alpha-0.3.2 Session Manager | Centralna własność, ograniczony rejestr, stabilny odczyt, liczenie faz i strony kopii |
| alpha-0.3.3 User Core | Publiczna tożsamość/profil gościa, własność w sesji, projekcja i aktualizacje INF (integracja po Session Manager) |
| alpha-0.3.4 Session Manager | Centralne indeksy tożsamości publicznej i zarządzane zmiany cyklu życia/profilu |
| alpha-0.3.5 User Core | Sprawdzone nazwy/tekst profilu, wartości zmiany nicku i zwalidowana projekcja ADC |
| alpha-0.3.6 Nick Manager | Ograniczony rejestr nicków, walidacja, transakcyjna zmiana i zwalnianie przy usunięciu sesji |
| alpha-0.3.7 User Registry | Ograniczone indeksy SID/CID, User tylko do odczytu, transakcyjne przyjęcie i usuwanie |
| alpha-0.3.8 User Login | Rejestracja zweryfikowanego gościa, typowane błędy i gotowość po końcowym powitaniu |
| alpha-0.3.9 User List | Deterministyczna synchronizacja początkowa, ograniczone powitanie i aktualizacje obecności FIFO |
| alpha-0.3.10 INF Updates | Transakcyjne metadane/profil/nick i zweryfikowane rozsyłanie przyrostowe |
| alpha-0.3.11 User Disconnect | Finalizacja, zwalnianie zasobów i ograniczone publiczne QUI po sprzątaniu |
| alpha-0.3.12 Limits | Konfigurowalne limity połączeń/sesji/użytkowników i bezpieczne przyjęcie/zwalnianie |
| alpha-0.3.13 Statistics | Liczniki cyklu życia, online/szczyt, połączenia, API i WebAdmin |
| alpha-0.4.1 ADC MSG | Kodek MSG i walidacja stanów; rozsyłanie czatu pozostaje planem |
| alpha-0.4.2 Chat Core | Publiczne BMSG i /me; polityka prywatna/grupowa i moderacja pozostają planem |
| alpha-0.4.3 Public Chat | Osobna polityka publicznego BMSG i wspólna ramka; czat prywatny/grupowy pozostaje planem |
| alpha-0.4.4 Private Chat | Prywatne DMSG/EMSG; grupy, historia i moderacja pozostają planem |
| alpha-0.4.5 Message Routing | Wspólny router MSG, domeny silnika i bezpieczne kierowanie do sesji/kolejek |
| alpha-0.4.6 Message Flags | Wspólna typowana walidacja PM/ME i kanoniczna projekcja publiczna/prywatna |
| alpha-0.4.7 Hub Messages | Przygotowanie/rozsyłanie IMSG, ograniczone współdzielone wyjście i powitanie |
| alpha-0.4.8 Chat Validation | Wspólne kontrole tekstu/SID/nadawcy, stan odbiorcy i ponowna kontrola dispatch |
| alpha-0.4.9 Anti-Flood | Włączone limity liczby/bajtów B/D/E, odwracalne odmowy i polityka INI |
| alpha-0.4.10 Chat Configuration | Zgrupowane INI, polityka klienta i własne powitanie bez zmiany gotowości |
| alpha-0.4.11 Chat Logging | Ograniczone logi wyników bez treści i liczniki zbiorcze API/WebAdmin |
| alpha-0.4.12 WebAdmin | Osobny widok wszystkich zbiorczych statystyk czatu tylko do odczytu |
| alpha-0.5.1 Search Core | Sprawdzany model zapytania tekstowego i biblioteka dispatch gotowych sesji; sieciowe SCH/RES są planowane |

## Historyczny plan odbioru przed alpha-0.2.8

alpha-0.2.2 realizuje zamówiony pełniejszy parser. Wcześniejsza propozycja serializera pozostaje planowana; numery dalszych etapów zostaną przypisane po określeniu zakresu.

To plan kryteriów odbioru, nie zaimplementowane funkcje. Ostateczny zakres należy ustalić przed rozpoczęciem każdego numerowanego etapu.

| Proponowany etap | Zakres | Kryterium odbioru |
| --- | --- | --- |
| Plan: ADC output | Serializer, escape, walidacja nagłówków i limity zakodowanych bajtów | Dokładne bajty zgodne z gramatyką; parse/serialize, Unicode, granice i błędy; jeszcze bez zapisu do sieci |
| Plan: Network output | Ograniczone kolejki klienta i łączne, częściowy zapis TCP/TLS, WANT_READ/WANT_WRITE i polityka wolnego odbiorcy | Fragmentowany zapis TCP/ADCS, limity kolejek, sprawiedliwa obsługa przy zatrzymanym odbiorcy, błędy klienta i ograniczony stop; bez nieograniczonych alokacji |
| Plan: Sessions | Integracja stanów/SUP/SID/INF z transportem, rejestr użytkowników, unikalność nicku/CID i dopuszczenie | Dozwolone/niedozwolone przejścia, unikalność/zwalnianie SID, odrzucanie podszytego źródła i duplikatów; bez wycieku PID |
| Plan: Client integration | Pełne logowanie gościa przez ADC i ADCS, rozłączenia i znaczenie gotowości | Prawdziwy ncdc osiąga normalny stan zalogowania na obu transportach; reconnect, wielu użytkowników, błędna negocjacja i dotychczasowe regresje przechodzą |

Pełny chat, wyszukiwanie i koordynacja transferów pozostają osobnymi etapami po poprawności sesji. Konta wymagają przeglądu uwierzytelniania/uprawnień/haseł oraz migracji aplikacji. Produkcyjna administracja WWW wymaga uwierzytelnionej bramy Apache, sesji/ról przeglądarki, CSRF i audytu przed udostępnieniem. Izolacja/rotacja logów, close_notify/reload TLS, pomiary obciążenia/fuzzing/fault injection i pozostałe systemy to jawne dalsze zadania.

## Gałęzie i ukończenie

alpha-0.2.2 powstaje z main w `ca464e02e714c1e81ed4c37279496168a6c47e23`, po scaleniu obu gałęzi audytu. Nie zmienia samoczynnie main ani znacznika alpha-0.2.

alpha-0.2.1 powstaje z main po scaleniu alpha-0.1.11. alpha-0.2 wskazuje ten sam commit audytu jako znacznik linii; nie zastępuje wymaganych numerowanych gałęzi z main. Scalanie sprawdzonego etapu do main wymaga osobnego polecenia. Zachować historię gałęzi i niezmienne migracje SQL.

Każdy etap wymaga CTest z obowiązkowymi integracjami, testów frontendu gdy zmieniany, kontroli metadanych/dokumentacji i changeloga. Zielony ncdc z początkowym HSUP potwierdza transport, dopóki pełne logowanie nie zostanie zaimplementowane. Zobacz [audyt i priorytety](AUDIT_ALPHA_0.1.md).

alpha-0.2.3 powstaje z main w `4c98e10` po scaleniu alpha-0.2.2. [Moduł stanów](ADC_STATE.md) realizuje samodzielną kontrolę kolejności; wyjście transportu, walidacja funkcji/tożsamości/haseł, unikalność SID i pełne logowanie ncdc pozostają planowane. Etap nie scala się samoczynnie do main.

alpha-0.2.4 powstaje z main w `1dd0b9f1a94626789d1ac9addad991a6eae78910`, po jawnym scaleniu alpha-0.2.3. [Moduł SUP](ADC_SUP.md) implementuje wybór funkcji/hash i łączy go z zatwierdzeniem stanu. Ogólne wyjście, podłączenie transportu, obliczanie hash i pełne logowanie pozostają planowane. Gałąź nie scala się samoczynnie do main.

alpha-0.2.5 powstaje z main w `350a43f69b5134bd049923739cd606f798f846ec` po scaleniu SUP. [SID](ADC_SID.md) realizuje własność alokatora/rezerwacji i połączenie modułów; wyjście/podłączenie sesji i tożsamość pozostają planowane. Gałąź nie scala się samoczynnie do main.

alpha-0.2.6 powstaje z main w `85524067756ac5fcbd177bb47f5d2cfe33fad063`. [INF](ADC_INF.md) realizuje walidację pól/PID-CID i połączenie stosu. Globalny rejestr/dopuszczenie, hasła kont, wyjście i pełne logowanie pozostają planowane. Gałąź nie scala się automatycznie.

alpha-0.2.7 powstaje z main w `9d3443a4d889e745b6490a139435a8f44297cfba`. [CID/PID](ADC_IDENTITY.md) wydziela i wzmacnia sprawdzanie rozpoczęte w INF; nie dodaje logowania, globalnej unikalności CID, uwierzytelniania kont ani drzew plików. Gałąź nie scala się automatycznie.


## alpha-0.2.8 Login

Dostarczono: uporządkowane wyjście/serializację, sesje gości i rejestr, rzeczywiste TCP/TLS NORMAL oraz reconnect ncdc. Pozostają: konta/hasła/uprawnienia, obsługa czatu/wyszukiwania/transferów, TLS close_notify/reload, produkcyjne testy obciążenia/fuzz/fault injection i pozostałych OS. Podstawa: main b8978491fa8e146527eba818f9bc1ab0a31aa90d. Gałąź numerowana nie scala się automatycznie.
