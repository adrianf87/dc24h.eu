# Search Security — alpha-0.5.11

**Bieżący etap — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) dodaje do Transfer Core własne wartości protokołu i jawne rozróżnienie zwykłego ADC/1.0 oraz wymagającego TLS ADCS/1.0. Czysty wybór preferencji/możliwości nie obniża wymaganego TLS. CTM/RCM zachowują wybraną nazwę i zgodność rozszerzeń, a odpowiedzi błędów klientów zachowują tokeny. Rzeczywisty ncdc sprawdza TLS klientów z weryfikacją certyfikatu przez oba transporty huba; zachowano regresję pełnego terminala.

**Etap dostarczony w alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) łączy przyjmowanie i dostarczanie D/E CTM oraz RCM we wspólnym routerze SID, jednym typie niezmiennego pakietu i jednej ścieżce ograniczonych kolejek. Odbiorcami są ustalone okresy życia gotowych sesji: D tylko cel, E cel i niezależne echo źródła. Token nie wybiera odbiorców; zachowano role, walidację, błędy i prywatność. Testy równoległych par używają tego samego tokenu bez mieszania odbiorców.

**Etap dostarczony w alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Etap dostarczony w alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) dodaje niezależny silnik C++20 koordynacji połączeń: ograniczone własne żądania, role aktywne/pasywne IPv4/IPv6 i niezmienne plany jednej próby związane z sesjami, anulowaniem i ponownymi kontrolami. Wspólna klasyfikacja adresów Users zachowuje Search. Routing CTM/RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

**Etap dostarczony w alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

Weryfikacja lokalna 2026-11-13: **118/118 zestawów CTest przeszło w każdej konfiguracji Debug, Release i ASan/UBSan**, z wymaganymi narzędziami i domyślną kontrolą wycieków. Obejmuje limity nadawcy/odbiorcy, bajty, uprawnienia/cofnięcie, granice obiektów, konfigurację, rzeczywiste odmowy w ncdc i dotychczasowe ADC/ADCS/UDP, MariaDB oraz Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja rzeczywistego API C++. Sprawdzono metadane, UTF-8, zgodność języków/linków dokumentacji, składnię zmienionych plików Python, przykładowe INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.

Gałąź utworzono bezpośrednio z main `c20b6d4bdbcf1b2fcdb7f2ded1f814988a37446a`, po scaleniu alpha-0.5.10. Zachowano wcześniejszy kod i historię; gałąź publikowana jest osobno od main.

## Kontrola i uprawnienia

`search/security.hpp` definiuje sprawdzone SecuritySettings, Permissions, Operation, kontrolę uprawnień i SearchGuard o stałym rozmiarze na czas życia połączenia. LoginHub stosuje ochronę na rzeczywistej ścieżce ADC/ADCS. Search Core i adaptery niższego poziomu pozostają zaufanymi elementami; ich bezpośrednie wywołanie nie uruchamia automatycznie limitów transportu.

Kontrole stanu, gotowości i własności SID zachowują pierwszeństwo krytycznych 244/240. Dla wspieranych SCH/RES hub sprawdza następnie globalne włączenie i uprawnienie nadawcy, pobiera budżet operacji/bajtów, opcjonalnie wymaga tokenu wyniku i dopiero wtedy waliduje dane, wybiera odbiorcę, rejestruje token i kieruje ramkę. Próby błędne semantycznie, bez kryteriów, z powtórzonym tokenem lub brakującym celem zużywają budżet nadawcy. Błędy surowej składni/escape/UTF-8 pozostają pod dotychczasowymi limitami parsera/transportu. Odrzucone zapytanie nie tworzy planu, nie rezerwuje TO i nie rozsyła danych.

`queries_enabled` / `results_enabled` mają pierwszeństwo nad zaufanymi uprawnieniami sesji. `guest_queries` / `guest_results` ustalają prawo nowego gościa do wysyłania SCH / DRES. Obecni użytkownicy są zweryfikowanymi gośćmi ADC; nie dodano uwierzytelniania kont/haseł/ról. INF CT/SU, SUP, dowód CID, nazwa i TO nigdy nie nadają uprawnień. Nadal odrzucane są deklaracje uprzywilejowanego CT.

Zaufana metoda C++ wątku właściciela `LoginHub::set_search_permissions(id, permissions)` aktualizuje tylko istniejący **gotowy identyfikator życia sesji**, zwracając false dla braku/gotowości lub zamykania. Nie istnieje polecenie klienta, endpoint API ani kontrolka WebAdmin. Cofnięcie prawa zapytań usuwa okna tokenów właściciela; ponowne nadanie pozwala tworzyć nowe. Zmiana nie odnawia budżetów ani powiadomień. Reconnect i ponowne użycie SID nie dziedziczą zaufanego nadania. Prawo zapytań dotyczy ich wysyłania, nie ogólnego prawa odbierania niezamówionych danych bez tokenu; restrykcyjna polityka TO wyłącza tę zgodność. Cofnięcie nie usuwa już zakolejkowanych ramek FIFO ani bezpośrednich datagramów UDP.

## Liczniki, bajty i ochrona odbiorcy

Zapytania, wysyłane wyniki i odbierane wyniki mają niezależne budżety liczby operacji i zdekodowanych bajtów. Początkowo są pełne, odnawiają się ciągle według jawnego czasu monotonicznego i pobierają liczbę/bajty atomowo. Odmowa nie zużywa części budżetu. Cofnięty czas jest ograniczany; skrajne różnice czasu/rozmiaru nasycają się bez przepełnienia ze znakiem. Kontrola, powiadomienia i zmiana uprawnień nie alokują pamięci i nie zachowują zapytań, nazw, hashy, tokenów ani historii tożsamości.

Koszt obejmuje zdekodowany FOURCC, nagłówki źródła/celu, selektory/znaki, nazwy/wartości pól (również nieznane) i separatory, bez LF. Lokalne obiekty ponad limit ramki/liczby pól otrzymują nasycony koszt przed parsowaniem typowanym. Koszt nie mnoży się przez liczbę odbiorców; istniejące limity wyjścia, ramek i połączeń nadal ograniczają pracę transportu.

Zmiana nadawcy nie omija osobnego budżetu celu: odbierane DRES współdzielą jeden limit życia sesji odbiorcy. Wyczerpanie odrzuca wynik przed kolejką i zachowuje użyteczność obu klientów. Dane nie są zapamiętywane do ponowienia. Bezpośrednie URES klientów omija hub; TLS i limity huba nie kontrolują zewnętrznego UDP.

Powiadomienia ochrony współdzielą **jedno pozwolenie na sekundę na połączenie źródła** dla zapytań/wyników, uprawnień, wymaganego TO i limitu odbiorcy. Poprawny ruch i zmiany uprawnień nie zerują okna. Błędy typowanej walidacji/aktywnego TO zachowują stałe komunikaty, ale ich próby zużywają budżet nadawcy. Błędy limitu/alokacji kolejki odbiorcy zachowują izolację i nie wymagają alokacji odpowiedzi do źródła.

| Warunek | Stała odpowiedź, gdy dostępny jest budżet powiadomienia |
| --- | --- |
| Operacja wyłączona globalnie | `ISTA 125 Search\soperation\sis\sdisabled FC<FOURCC>` |
| Brak uprawnienia sesji | `ISTA 125 Search\soperation\snot\spermitted FC<FOURCC>` |
| Brak liczby/bajtów nadawcy | `ISTA 100 Search\srate\slimit\sexceeded FC<FOURCC>` |
| Wymagany, lecz nieobecny TO | `ISTA 100 Search\sresult\stoken\srequired FCDRES` |
| Brak liczby/bajtów odbiorcy | `ISTA 100 Search\srecipient\srate\slimit\sexceeded FCDRES` |

Nie dodano potwierdzenia sukcesu, odbijania treści, krytycznego zamknięcia za zwykłą odmowę polityki, logu wyszukiwania, zapisu w bazie ani licznika wyszukiwania API/GUI. Czat i jego osobne limity/liczniki pozostają bez zmian.

## Konfiguracja startowa

Ustawienia znajdują się w `[search]` w [przykładowym INI](../../config/dc24h.ini.example). Kontrola odbywa się przed nasłuchem, również dla zaufanych opcji network/LoginHub. Sekcje/klucze rozróżniają wielkość liter; duplikaty/nieznane nazwy, błędne wartości logiczne, niepoprawne liczby i sprzeczne limity zatrzymują start. Nie dodano przeładowania ani nadpisań CLI.

| Klucz | Domyślnie | Zakres / znaczenie |
| --- | --- | --- |
| queries_enabled / results_enabled | on / on | Globalne SCH / DRES. |
| guest_queries / guest_results | on / on | Uprawnienia wysyłania nowego gościa. |
| require_result_token | off | on odrzuca DRES bez TO; off zachowuje pasywne ncdc. Podany TO zawsze podlega kontroli aktywnego właściciela/generacji. |
| max_active_searches | 4096 | 1..65535 wszystkich okien TO. |
| max_active_per_user | 8 | 1..64, nie więcej niż limit globalny. |
| search_lifetime_seconds | 60 | 1..3600, monotoniczne leniwe wygaszanie; wynik nie odnawia terminu. |
| query_rate / query_burst | 10 / 32 | Operacje na sekundę / kredyt początkowy, każde 1..1000000; wspólne B/F/D/E SCH. |
| query_byte_rate / query_byte_burst | 32768 / 131072 | Bajty zdekodowane na sekundę / kredyt; tempo 1..16777216, kredyt 16384..16777216. |
| result_rate / result_burst | 64 / 128 | DRES na sekundę / kredyt; zakres jak zapytania; niezależnie dla wysyłania i odbierania. |
| result_byte_rate / result_byte_burst | 262144 / 1048576 | Tempo/kredyt bajtów niezależnie dla wysyłania/odbierania; zakres jak bajty zapytań. |

Zaufane `LoginLimits.search` i `network::Options.search` zawierają te same ustawienia; aktywne limity są teraz w `.search.tokens` zamiast dawnego LoginLimits `.search_tokens`. Interfejs TokenLimits Core pozostaje zgodny i współdzieli walidację. Nadal obowiązują limity pól/tekstu/hashy parsera, wyjścia, gotowość/własność źródła, życie sesji i sprzątanie TO.

## Weryfikacja i granice

`search.security` sprawdza niezależne budżety liczby/bajtów/odbiorcy, atomowe pobieranie, dokładne ułamkowe odnawianie, wspólne powiadomienia, skrajny/cofnięty czas i rozmiary, brak alokacji, błędne/nadmierne lokalne próby, pierwszeństwo uprawnień, zaufane nadanie/cofnięcie, brak odnowienia kredytów, reconnect, wymagany TO i wielu nadawców do jednego celu. Dotychczasowe testy obciążające parser jawnie podnoszą limit zapytań, żeby niezależnie zachować zakres semantyczny.

`search.security.process` obejmuje rzeczywiste ADC/ADCS: wyczerpanie nadawcy/odbiorcy, ciche kolejne odmowy, odnowienie, gości/globalne przełączniki, TO, konfigurowalne limity aktywne/wygaszanie, reconnect, stan/źródło, błędy konfiguracji i prywatność API/logów/czatu. Rzeczywiste `adc.searchsecurity.ncdc` / `adcs.searchsecurity.ncdc` pokazują odmowę w ncdc i dalsze działanie połączenia/czatu. Dotychczasowe testy aktywnego/pasywnego UDP, funkcji/wyników, filtrów/TTH, kolejek i sesji pozostają włączone.

Etap dodaje kontrolę transportu huba, nie trwałe konta/role, listę dopuszczonych odpowiadających, uwierzytelnianie plików/filtrów, liczbę wyników na wyszukiwanie/deduplikację, kontrolę NAT/SUDP, nowe rozproszone limity IP ani certyfikację obciążenia. Implementacja pozostaje EN/US, dokumentacja i istniejące katalogi runtime US/PL. Nie dodano zależności, migracji, wpisu operatora, API zapisu ani strony wyszukiwania WebAdmin.
