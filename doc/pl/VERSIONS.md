# Wersjonowanie

**Bieżący etap — alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) przekazuje pełny 192-bitowy hash pliku z SCH do niezmiennych zapytań/planów Core, opisów i filtrów. Zapytania z samym hashem lub łączone używają dokładnej równości i zachowują dodatkowe kryteria. Testowane są rzeczywiste zapytania TTH ncdc ADC/ADCS; routing zapytań/wyników pozostaje kolejnym etapem.

**Etap dostarczony w alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) dodaje skompilowane filtry nazw/ścieżek Unicode bez rozróżniania wielkości liter, alternatywy rozszerzeń, domknięte/dokładne limity bajtów i typ plik/katalog dla sprawdzonych opisów. Zapytania z samymi filtrami i nieprzezroczyste tokeny NFC przechodzą do Search Core. Dopasowanie nie alokuje pamięci; routing/wyniki, indeksy i administracja wyszukiwaniem pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.2:** [ADC SCH](ADC_SCH.md) dodaje własny typowany parser SCH i odbiór w ADC/ADCS po sprawdzeniu stanu/źródła. Sprawdza pola, selektory, limity i kanoniczne TR; poprawne zapytania otrzymują informację o braku routingu, błędne semantycznie odwracalną odmowę, a zapytania bez kryteriów są pomijane. Routing/wyniki pozostają kolejnymi etapami.

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

| Wersja | Gałąź | Zakres |
| --- | --- | --- |
| alpha-0.1.1 | alpha-0.1.1 | Core |
| alpha-0.1.2 | alpha-0.1.2 | Network |
| alpha-0.1.3 | alpha-0.1.3 | Parser ADC |
| alpha-0.1.4 | alpha-0.1.4 | ADCS/TLS, zachowana historia chatgpt-alpha-0.1.4 |
| alpha-0.1.5 | alpha-0.1.5 | Konfiguracja: INI, CLI, ścieżki i logowanie plikowe |
| alpha-0.1.6 | alpha-0.1.6 | Logi startu, połączeń, ostrzeżeń i błędów |
| alpha-0.1.7 | alpha-0.1.7 | Ograniczone przyjęcia IP, ramki i terminy bezwzględne |
| alpha-0.1.8 | alpha-0.1.8 | Połączenie MariaDB i rejestr migracji |
| alpha-0.1.9 | alpha-0.1.9 | Lokalne API administracyjne HTTP/JSON tylko do odczytu |
| alpha-0.1.10 | alpha-0.1.10 | Układ WebAdmin React/TypeScript/Vite i lokalny adapter tylko do odczytu |
| alpha-0.1.11 | alpha-0.1.11 | Szablon HTTPS Apache2 i izolowane testy TLS/dostępu |
| alpha-0.2.1 | alpha-0.2.1 | Project: audyt alpha-0.1 i przygotowanie alpha-0.2 |
| alpha-0.2.2 | alpha-0.2.2 | ADC Parser: argumenty BASE, pola nazwane i ograniczona składnia liczb |
| alpha-0.2.3 | alpha-0.2.3 | ADC State: niezależny automat połączenia huba |
| alpha-0.2.4 | alpha-0.2.4 | SUP: negocjacja funkcji/hash i integracja stanów |
| alpha-0.2.5 | alpha-0.2.5 | SID: generowanie, rezerwacje RAII i powiązanie sesji |
| alpha-0.2.6 | alpha-0.2.6 | INF: informacja klienta i walidacja tożsamości Tiger |
| alpha-0.2.7 | alpha-0.2.7 | CID/PID: dedykowana weryfikacja Tiger i obsługa błędów |
| alpha-0.2.8 | alpha-0.2.8 | Login: NORMAL gościa, ograniczone wyjście TCP/TLS, publiczna obecność |
| alpha-0.2.9 | alpha-0.2.9 | STA/QUI: diagnostyka protokołu, routing i ograniczone zamykanie |
| alpha-0.2.10 | alpha-0.2.10 | Security: oczekujące sesje, łączne logowanie i limity prób tożsamości |
| alpha-0.3.1 | alpha-0.3.1 | Session Core: cykl Users, stabilne lokalne ID i snapshoty |
| alpha-0.3.2 | alpha-0.3.2 | Session Manager |
| alpha-0.3.3 | alpha-0.3.3 | User Core |
| alpha-0.3.4 | alpha-0.3.4 | Session Manager: central identity registry |
| alpha-0.3.5 | alpha-0.3.5 | User Core: checked public values and ADC projection |
| alpha-0.3.6 | alpha-0.3.6 | Nick Manager: walidacja, ograniczony rejestr i atomowa zmiana nicka |
| alpha-0.3.7 | alpha-0.3.7 | User Registry: indeksy SID/CID i usuwanie po właścicielu sesji |
| alpha-0.3.8 | alpha-0.3.8 | User Login: rejestracja sesji po zwalidowanej identyfikacji ADC |
| alpha-0.3.9 | alpha-0.3.9 | User List: synchronizacja początkowej listy użytkowników |
| alpha-0.3.10 | alpha-0.3.10 | INF Updates: walidacja, anulowania i rozsyłanie przyrostowe |
| alpha-0.3.11 | alpha-0.3.11 | User Disconnect: QUI, zakończenie cyklu życia i zwalnianie zasobów |
| alpha-0.3.12 | alpha-0.3.12 | Limits: połączenia, sesje, użytkownicy i zwalnianie po sprzątaniu |
| alpha-0.3.13 | alpha-0.3.13 | Statistics: online, peak, logins, connections, API/WebAdmin |
| alpha-0.4.1 | alpha-0.4.1 | ADC MSG: parser, PM/ME, checked serialization and live validation |
| alpha-0.4.2 | alpha-0.4.2 | Chat Core: independent engine, lifecycle-bound public chat and bounded delivery |
| alpha-0.4.3 | alpha-0.4.3 | Public Chat: dedicated BMSG adapter, shared immutable frames, independent queue accounting |
| alpha-0.4.4 | alpha-0.4.4 | Private Chat: DMSG/EMSG, PM origin, lifetime-bound targets and isolated queues |
| alpha-0.4.5 | alpha-0.4.5 | Message Routing: wspólna polityka MSG, domeny silnika i routing według ID życia |
| alpha-0.4.6 | alpha-0.4.6 | Message Flags: wspólny kodek PM/ME i kanoniczna projekcja czatu |
| alpha-0.4.7 | alpha-0.4.7 | Hub Messages: zaufane IMSG, gotowi odbiorcy i uporządkowane powitanie gościa |
| alpha-0.4.8 | alpha-0.4.8 | Chat Validation: wspólna kontrola tekstu/SID/sesji i bezpośrednich adapterów |
| alpha-0.4.9 | alpha-0.4.9 | Anti-Flood: budżety liczby/bajtów czatu połączenia, INI i ograniczona diagnostyka |
| alpha-0.4.10 | alpha-0.4.10 | Chat Configuration: sprawdzona polityka startowa, tekst/kanały/akcje i opcjonalne własne powitanie |

Dokładną kompilację identyfikuje wersja wraz z SHA commita. Budowanie nie zmienia plików źródłowych. Istotne zmiany trafiają do CHANGELOG.md. Alpha oznacza rozwój, nie pełną funkcjonalność huba. Tagi i wdrożenie nie powstają automatycznie.

Wcześniejsze gałęzie i historia commitów są zachowane. alpha-0.1.4 zachowuje przez merge historię referencyjnego TLS i zawiera poprawki.

## Wersja historyczna — alpha-0.2.7

CMake PROJECT_VERSION i główne wersje npm package/lock to **0.2.7**; program/WebAdmin mają **alpha-0.2.7**. Kontrola metadanych wymusza spójność. Gałąź powstaje z main w `9d3443a` po scaleniu INF i dodaje [dedykowaną weryfikację CID/PID](ADC_IDENTITY.md). Wymagania Libgcrypt/OpenSSL są dotychczasowe. Podłączenie Network, rejestr/dopuszczenie i pełne logowanie pozostają planowane. Main i wcześniejsze gałęzie są zachowane.


## alpha-0.2.8

**Aktualna integracja — alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Wersja CMake/npm 0.2.8; runtime alpha-0.2.8. Gałąź alpha-0.2.8 pochodzi z main b8978491fa8e146527eba818f9bc1ab0a31aa90d. Bez automatycznego scalania main i tagu wydania.


## alpha-0.2.9

CMake/npm 0.2.9; runtime/WebAdmin alpha-0.2.9. Podstawa: main `da1521502c4a8936b3427a7d38fea6b904686b39`. [STA/QUI](ADC_STATUS.md) rozwija sesje gości bez nowego schematu API, zależności i migracji SQL. Bez automatycznego merge/tagu.


## alpha-0.2.10

CMake/npm 0.2.10, runtime/WebAdmin alpha-0.2.10. Podstawa: main `a3ca70d65a0fcbb563147e8a44a1ce2669a0ec53`. [Security](NEGOTIATION_SECURITY.md) dodaje lokalną politykę negocjacji bez zależności, migracji SQL i zmian schematu API. Bez automatycznego scalania main i tagu.


## alpha-0.3.1

CMake/npm 0.3.1, runtime/WebAdmin alpha-0.3.1. Podstawa: main `5bc79d0a12eb37d0869651eb48b57d2bf338d7fe`. [Session Core](SESSION_CORE.md) dodaje dc24h_users bez zależności, migracji, opcji bezpieczeństwa i zmian API. Bez automatycznego scalania main/tagu.
