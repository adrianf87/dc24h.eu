# Testowanie

**Bieżący etap — alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

**Etap dostarczony w alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizuje sprawdzanie tekstu, SID i gotowości/własności nadawcy w obu adapterach. Wybór życia odbiorcy i ponowna kontrola dispatch pozostają obowiązkowe; systemowe IMSG jest oddzielone.

**Etap dostarczony w alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) dodaje zaufane systemowe IMSG powiązane z życiem sesji i stałe powitanie po własnym BINF. Klient nie może nadawać komunikatów systemowych; zachowano limity kolejek i gotowość przy końcowym LF.

**Etap dostarczony w alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizuje dekodowanie PM/ME i kanoniczne wyjście kodeka oraz obu adapterów czatu. Brak ME, ME0 i ME1 pozostają rozróżniane; PM nie nadaje uprawnień do dostarczania.

**Etap dostarczony w alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizuje wybór zakresu MSG i dostarczanie do ograniczonych kolejek. Plany są powiązane z domeną macierzystego Chat Core i ID życia sesji; prywatne wiadomości nigdy nie trafiają zastępczo na czat publiczny.

**Etap dostarczony w alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) dodaje DMSG tylko do celu i EMSG do celu oraz nadawcy, pojedyncze wiadomości do siebie, powiązanie celu z ID życia i kontrolę PM. Czat publiczny pozostaje oddzielony; grupy/historia/moderacja to kolejne etapy.

**Etap dostarczony w alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) dodaje osobny adapter polityki i rozsyłania BMSG oraz jedną wspólną niemutowalną ramkę na publikację. Offsety i logiczne budżety odbiorców pozostają niezależne; zachowano bramki źródła/stanu/prywatności i FIFO.

**Etap dostarczony w alpha-0.4.2:** [Chat Core](CHAT_CORE.md) dodaje osobny silnik wiadomości niezależny od protokołu oraz publiczne BMSG, z kopią dla nadawcy i /me. Czat prywatny/grupowy/feature/hub, moderacja i historia pozostają osobnymi etapami.

**Etap dostarczony w alpha-0.4.1:** [ADC MSG](ADC_MSG.md) dodaje kodek MSG z typowanymi polami PM/ME, sprawdzanym budowaniem komunikatów, ograniczoną serializacją i walidacją w maszynie stanów. Dostarczanie czatu i polityka rozmów pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.3.13:** [Statistics](STATISTICS.md) dodaje liczniki cyklu życia w czasie stałym, użytkowników online/szczyt i sumę logowań, atomowe statystyki transportu, pola chronionego API oraz metryki WebAdmin US/PL. Starsze etapy poniżej zachowują swój zakres historyczny.

## Weryfikacja Limits — alpha-0.3.12

Lokalnie przeszło wszystkich **65 zestawów CTest** w Debug, Release i ASan/UBSan. Nowe users.limits i users.limits.process obejmują surowe/efektywne pułapy, zgodne wartości domyślne, granice pojemności wszystkich faz, odmowę bez częściowego stanu/zużycia ID, sprawdzenie pełnego rejestru bez alokacji, rollback/retry alokacji, ISTA 211 → własne IQUI przed powitaniem, stabilny przypięty bufor i odzyskanie miejsca. Rzeczywiste mieszane ADC/ADCS osobno sprawdza niższy limit połączeń i sesji, niepełne TLS, odrzucenie bez przyjęcia TLS, liczniki API i budżet ostrzeżeń; powtarzane odmowy użytkowników nie dają publicznej obecności i pozwalają przyjąć ten sam CID/nick po sprzątaniu. Testy konfiguracji/Network obejmują błędne programowe/INI/CLI ustawienia, pierwszeństwo i pomoc/ostrzeżenia US/PL. Rzeczywisty ncdc ADC/ADCS z obserwatorem zapełnia max_users=2, aktualizuje dane i wraca pod max_sessions=3. Przeszły wcześniejsze regresje, MariaDB i Apache HTTPS. [Limits](LIMITS.md) opisuje dokładne rozliczanie i zakres.

Powtórz przez `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4` i `ctest --test-dir build-debug --output-on-failure -j 4`. Powtórz dla build-release/Release. W build-sanitize/Debug dodaj `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` i `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`. Kontrolowane błędy alokacji C++ dotyczą badanych ścieżek rejestracji/indeksów, bez dowolnych awarii systemu/alokatora C.

WebAdmin: poprawne typecheck/budowanie produkcyjne, 3 testy Node, 3 Chromium i integracja rzeczywistego API C++. W webadmin uruchom `npm run build`, `npm test`, `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e` i `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h`. Metadane/dane testowe odpowiadają alpha-0.3.12. Wyniki są lokalne, bez potwierdzenia zdalnego CI, wszystkich wspieranych systemów czy obciążenia produkcyjnego.

## Weryfikacja User Disconnect — alpha-0.3.11

Lokalnie przeszły wszystkie **63 zestawy CTest** w Debug, Release i ASan/UBSan. Nowe users.disconnect i users.disconnect.process obejmują finalizację/usunięcie bez alokacji, wszystkie fazy cyklu życia, pierwszą przyczynę/czas, powtórzone/brakujące sprzątanie, ponowne użycie SID/CID/nicka i limitów pending/per-IP, anulowanie powitania, pojedyncze bare publiczne QUI, odzyskanie budżetu źródła przed rozsyłaniem, stabilność przypiętego wyjścia, błędy alokacji powiadomień i dalsze dostarczanie po izolacji jednego odbiorcy. Rzeczywiste mieszane ADC/ADCS obejmuje EOF, reset, fatalny status klienta, odrzucenie błędnego INF, anonimowe błędne TLS i wyłączenie. Rzeczywisty ncdc ADC/ADCS potwierdza też brak routingu do starego SID. Przeszły wcześniejsze regresje, MariaDB i Apache HTTPS. [User Disconnect](USER_DISCONNECT.md) opisuje własność i obsługę błędów.

Powtórz przez `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4` i `ctest --test-dir build-debug --output-on-failure -j 4`. Powtórz dla build-release/Release. W build-sanitize/Debug dodaj `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` i `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`. Wstrzykiwanie błędów obejmuje badane ścieżki sprzątania/powiadomień C++, bez niezwiązanych awarii alokatora C i systemu operacyjnego.

WebAdmin: poprawne typecheck/budowanie produkcyjne, 3 testy Node, 3 Chromium i integracja rzeczywistego API C++. W webadmin uruchom `npm run build`, `npm test`, `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e` i `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h`. Metadane/dane testowe odpowiadają alpha-0.3.11. Wyniki są lokalne i nie oznaczają zdalnego CI, pełnej macierzy systemów ani testów obciążenia.

## Weryfikacja INF Updates — alpha-0.3.10

Lokalnie przeszło wszystkich **61 zestawów CTest** w Debug, Release i ASan/UBSan. Nowe users.inf_updates i users.inf_updates.process obejmują wspólną transakcję ClientInfo/User/nicka, wskazane pola przyrostowe, anulowania i zera, normalizację adresów, zachowanie pominiętych pól, brak nieznanych/pustych ramek, odrzucenie błędnej tożsamości/cyklu życia/czasu/źródła/uprawnień/wartości, wykrycie niespójnego adaptera i wycofanie/ponowienie alokacji. Rozsyłanie ADC/ADCS zgadza się z późniejszą pełną listą. Rzeczywisty ncdc zmienia opis w działającej sesji przez oba transporty, także z opcjonalnym wskazanym HN; zachowano testy list/reconnect/tożsamości/prywatności. Przeszły wcześniejsze testy serwera, MariaDB i Apache HTTPS. Dokładny kontrakt/ograniczenia opisuje [INF Updates](INF_UPDATES.md).

Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Powtórz dla build-release/Release. W build-sanitize/Debug dodaj `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` i `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`. Kampania błędów C++ obejmuje przygotowanie apply i indeksy przed zatwierdzeniem z zachowaniem innego użytkownika; nie wstrzykuje błędów alokatora C/systemu i nie gwarantuje atomowego rozsyłania dla sieci.

WebAdmin: poprawne typecheck/budowanie produkcyjne, 3 testy Node, 3 Chromium i integracja rzeczywistego API C++. W webadmin uruchom `npm run build`, `npm test`, `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e` i `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h`. Metadane/dane testowe odpowiadają alpha-0.3.10. Wyniki są lokalne i nie oznaczają zdalnego CI, pełnej macierzy systemów ani testów obciążenia.

## Weryfikacja User List — alpha-0.3.9

Lokalnie przeszło wszystkich **59 zestawów CTest** w Debug, Release i ASan/UBSan. Nowe users.list i users.list.process obejmują cykl życia/kursory publicznego członkostwa, deterministyczne plany, własny INF na końcu, dokładny budżet po kodowaniu, brak danych prywatnych/zachowanie metadanych ADC, nieaktualne źródła, 260 użytkowników przez granicę strony, wycofanie alokacji oraz zgodność list przy mieszanych transportach i równoczesnych dołączeniach, zmianach, anulowaniach, odejściach i reconnect. Rzeczywisty ncdc ADC/ADCS pokazuje dwóch użytkowników przy działającym obserwatorze i synchronizuje listy oraz quit/join przy reconnect. Przeszły także wcześniejsze testy logowania/gotowości/przepełnienia/tożsamości/bezpieczeństwa/rejestrów, MariaDB i Apache HTTPS. Dokładny zakres i ograniczenia opisuje [User List](USER_LIST.md).

Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Powtórz dla build-release/Release. W build-sanitize/Debug dodaj `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` i `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`. Testy wstrzykują błędy alokacji C++ przy przygotowaniu i przed publikacją; nie deklaruje się odtwarzania po błędach alokatora C/systemu.

WebAdmin: poprawne typecheck/budowanie produkcyjne, 3 testy Node, 3 Chromium i integracja rzeczywistego API C++. W webadmin uruchom `npm run build`, `npm test`, `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e` i `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h`. Metadane/dane testowe odpowiadają alpha-0.3.9. Wyniki są lokalne i nie oznaczają zdalnego CI, pełnej macierzy systemów ani testów obciążenia.

## Weryfikacja User Login — alpha-0.3.8

Lokalnie przeszło wszystkich **57 zestawów CTest** w Debug, Release i ASan/UBSan. Nowe users.login i users.login.process obejmują zwalidowaną/zatwierdzoną tożsamość, pojedynczą publiczną rejestrację, kolizje SID/CID/nicka, błędy czasu/cyklu życia, zgodność tożsamości przy końcu powitania, pojedynczą gotowość dokładnie po wysłaniu LF własnego INF, przepełnienie, wycofanie alokacji rejestracji i chronione natychmiastowe zamknięcie. Mieszane testy ADC/ADCS potwierdzają brak publicznej obecności przy błędnym dowodzie, rejestrację po fragmentowanym poprawnym INF, routing po gotowości i reconnect z tym samym CID. Rzeczywisty ncdc ADC/ADCS osiąga NORMAL i reconnect z polskimi nickami; przeszły MariaDB, Apache HTTPS i wcześniejsze testy bezpieczeństwa/rejestrów/transportu. Dokładny kontrakt i granice ochrony alokacji opisuje [User Login](USER_LOGIN.md).

Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Powtórz dla build-release/Release. W build-sanitize/Debug dodaj `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` i `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`. Ograniczone testy alokacji C++ obejmują projekcję/rejestrację indeksów i zakończenie bez alokacji; nie wstrzykują błędów alokatora C utf8proc ani systemu.

WebAdmin: poprawne typecheck/budowanie produkcyjne, 3 testy Node, 3 Chromium i integracja rzeczywistego API C++. W webadmin uruchom `npm run build`, `npm test`, `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e` i `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h`. Metadane/dane testowe odpowiadają alpha-0.3.8. Wyniki są lokalne i nie oznaczają zdalnego CI, pełnej macierzy systemów ani testu obciążenia.

## Weryfikacja User Registry — alpha-0.3.7

Lokalnie przeszło wszystkich **55 zestawów CTest** w Debug, Release i ASan/UBSan, w tym rzeczywisty ncdc ADC/ADCS, mieszane transporty, MariaDB i Apache HTTPS. Nowe users.user_registry i users.user_registry.process obejmują unikalność SID/CID i lokalnego właściciela, aktualny User const, niezmienne klucze, rezerwacje/usuwanie cyklu życia, reconnect/routing i wycofanie alokacji C++ z zachowaniem innych użytkowników. Przechwycenie ADC ncdc dodatkowo wymaga nowego SID i niezmienionego CID przy reconnect; oba transporty nadal osiągają NORMAL z polską zmianą nicka. Kontrakt i szczegóły opisuje [User Registry](USER_REGISTRY.md).

Pierwszy przebieg Debug ujawnił założenie istniejącego testu zamykania TLS: natychmiastowe zatrzymanie może zresetować połączenie z nieodczytanym końcowym wejściem handshake. Test używa teraz istniejącego ograniczonego helpera EOF/reset i nadal wymaga braku danych aplikacyjnych; poprawiony zestaw przeszedł. Zachowanie transportu serwera nie zostało zmienione.

Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Powtórz dla build-release/Release. W build-sanitize/Debug dodaj `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` i `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`. Wstrzykiwanie błędów dotyczy sprawdzanych ścieżek rejestracji C++, nie alokatora C utf8proc ani całego systemu.

WebAdmin: poprawne typecheck/budowanie produkcyjne, 3 testy Node, 3 Chromium i integracja rzeczywistego API C++. W webadmin uruchom `npm run build`, `npm test`, `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e` i `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h`. Asercje/dane wersji odpowiadają alpha-0.3.7. Wyniki są lokalne; nie potwierdzają zdalnego CI, pełnej macierzy systemów ani obciążenia produkcyjnego.

## Weryfikacja Nick Manager — alpha-0.3.6

Lokalnie przeszły wszystkie **53 zestawy CTest** w Debug, Release oraz ASan/UBSan. Nowe zestawy to users.nicknames i users.nicknames.process; istniejące testy obejmują rzeczywisty ncdc ADC/ADCS, mieszane transporty, MariaDB i Apache HTTPS. ncdc zmienił dc24hŻółw na dc24hŁoś przy reconnect i ponownie osiągnął NORMAL. Sprawdzono błędy alokacji C++ w rezerwacji, zmianie nicka i przyjęciu SessionManager; nie wstrzykiwano błędów alokatora utf8proc. Dokładny kontrakt i zakres opisuje [Nick Manager](NICK_MANAGER.md).

Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Powtórz dla build-release/Release. Dla build-sanitize/Debug użyj `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` i `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`.

WebAdmin: poprawne typecheck/budowanie produkcyjne, 3 testy Node, 3 testy Chromium i integracja API C++. W webadmin uruchom `npm run build`, `npm test`, `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e` i `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h`. Użyto systemowego Chromium; alternatywnie zainstaluj przeglądarkę Playwright. Asercje wersji i dane testowe odpowiadają alpha-0.3.6. Wyniki lokalne nie potwierdzają zdalnego GitHub Actions ani pełnej macierzy systemów operacyjnych.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

## Weryfikacja User Core — alpha-0.3.5

Pełna bramka ma **51 zestawów**, z users.values na zintegrowanej bazie main. Wszystkie przeszły w Debug, Release i ASan/UBSan, w tym rzeczywisty ncdc ADC/ADCS z logowaniem/reconnect jako dc24hŻółw, dotychczasowe testy rejestru/tożsamości/dostawcy kryptografii/bezpieczeństwa/STA/wyjścia, mieszane transporty, MariaDB i Apache. WebAdmin: typecheck/budowanie, 3 testy Node, 3 Chromium i rzeczywista integracja API C++ poprawne. Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug -j 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Flagi sanitizerów opisano poniżej. [User Core](USER_CORE.md) opisuje sprawdzenia tekstu, projekcji i granice lokalnych wyników.

**Etap dostarczony w alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

## Weryfikacja Session Manager — alpha-0.3.4

Pełna bramka ma **50 zestawów**, z users.registry na zintegrowanej bazie main. Wszystkie przeszły w Debug, Release i ASan/UBSan, w tym rzeczywisty ncdc ADC/ADCS, mieszane transporty/duplikaty/zmiana nicku/reconnect, regresje tożsamości/dostawcy kryptografii/bezpieczeństwa/STA/końcowego wyjścia, MariaDB i Apache. WebAdmin: typecheck/budowanie, 3 testy Node, 3 Chromium i rzeczywista integracja API poprawne. Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug -j 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Flagi sanitizerów opisano poniżej. [Session Manager](SESSION_MANAGER.md) opisuje nowe sprawdzenia własności/braku częściowych zmian i granice lokalnych wyników.

**Etap dostarczony w alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) posiada indeksy SID/CID/nick wraz z żywymi modelami. Odczyt const i zarządzane zmiany cyklu życia/User zachowują unikalność przy przyjęciu, zmianie nicku i sprzątaniu. LoginHub zachowuje protokół i kolejki. Starsze opisy dotyczą zakresu historycznych etapów.

## Integracja main — alpha-0.3.2, następnie alpha-0.3.3

Połączony main przeszedł **49/49 pełnych zestawów CTest w Debug, Release i ASan/UBSan**. Zachowano users.manager i users.core. Nowe sprawdzenia integracji potwierdzają profil przyjętego użytkownika i jego aktualizacje w stronach managera, zgodność faz z żywymi modelami, własność wcześniejszych kopii oraz usuwanie rozłączonego użytkownika z rejestru. Rzeczywisty ncdc ADC/ADCS, regresje transportu/bezpieczeństwa/tożsamości/STA, tymczasowa MariaDB i izolowany Apache poprawne. WebAdmin: budowanie/typecheck, 3 testy Node, 3 Chromium i integracja C++ API → Vite → React poprawne. To wyniki lokalne; opisane poniżej pierwotne bramki 48 zestawów dotyczą stanu przed scaleniem.

## Weryfikacja pierwotnej gałęzi User Core — alpha-0.3.3

Pierwotna bramka gałęzi miała **48 zestawów**, z users.core na gałęzi User Core z main. Wszystkie przeszły w Debug, Release i ASan/UBSan, wraz z rzeczywistym ncdc ADC/ADCS, MariaDB i Apache. WebAdmin: budowanie/typecheck, 3 testy Node, 3 Chromium i integracja rzeczywistego API C++ poprawne. Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug -j 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Flagi sanitizerów opisano poniżej. Dotychczasowy test zamykania API akceptuje EOF/reset przy nieodczytanym prefiksie żądania; odpowiedzi aplikacji nadal są sprawdzane normalnie.

**Etap dostarczony w alpha-0.3.3:** [User Core](USER_CORE.md) dodaje do Session publiczny model gościa i profilu, projekcję całego zwalidowanego INF oraz aktualizacje przez bramkę cyklu życia. Powstał bezpośrednio z main z Session Core; main łączy go teraz z wcześniej scalonym alpha-0.3.2 Session Manager. Starsze opisy zachowują zakres historyczny.

## Weryfikacja pierwotnej gałęzi Session Manager — alpha-0.3.2

Pierwotna bramka gałęzi miała **48 zestawów**, w tym users.manager. Debug, Release i ASan/UBSan przeszły wraz z rzeczywistym ncdc ADC/ADCS, MariaDB i Apache. Pierwsze sprawdzenie zamykania w Debug otrzymało poprawny reset gniazda Unix; test akceptuje EOF/reset przy nieodczytanych danych i został powtórzony we wszystkich wariantach. WebAdmin: budowanie, 3 testy Node, 3 Chromium i rzeczywista integracja API poprawne. Odtwarzanie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug -j 4`, `ctest --test-dir build-debug --output-on-failure -j 4`. Flagi sanitizerów opisano poniżej.

**Etap dostarczony w alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.


**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

## Session Core — alpha-0.3.1

Pełny zestaw rejestruje **47 testów CTest**, dodając users.session, users.session.adc i users.session.process. [Session Core](SESSION_CORE.md) opisuje model, osobną gotowość, własność tożsamości/kopii i zamykanie. Testy obejmują UINT64_MAX, stare ID, sztuczny zegar/rollback, niezmienne klucze, pierwszą/wymuszoną/końcową przyczynę, bajtowe pojedyncze ukończenie powitania, anulowanie/przepełnienie i rzeczywisty TCP/TLS. Dotychczasowy reconnect ncdc wymaga jednego rosnącego lokalnego ID, gotowości i zamknięcia na połączenie.

Wynik lokalny: **47/47 w Debug, Release i ASan/UBSan**, z wymaganymi ncdc, MariaDB, Apache HTTPS i poprzednimi regresjami protokołu/bezpieczeństwa/provider/blokowanego wyjścia. WebAdmin build/typecheck, 3 testy Node, 3 Chromium i rzeczywista integracja API/Vite/React przeszły. Dotychczasowe flagi sanitizera/no-PIE i ustawienia Chromium pozostają aktualne. Metadane, UTF-8/linki/parytet języków, składnia Python i białe znaki Git poprawne. Pozostałe OS/CI, awarie alokacji i produkcyjne obciążenie/bezpieczeństwo wymagają osobnych dowodów.

Zakres: `ctest --test-dir build-debug -R 'users.session|adc.ncdc|adcs.ncdc' --output-on-failure`; przed zakończeniem etapu obowiązuje cały wymagany zestaw integracji.

## Bezpieczeństwo negocjacji — alpha-0.2.10

Pełny zestaw rejestruje **44 testy CTest**, dodając security.negotiation, security.negotiation.process, adc.security.ncdc i adcs.security.ncdc. [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) opisuje naliczanie, gotowość, okna przed Tiger i przypadki jednostkowe/sieciowe/ncdc. Sztuczny zegar sprawdza odmowę globalną i opóźnioną pierwszą naliczoną próbę: odmowa nie zaczyna wcześniej minuty IP, a pruning zachowuje nowsze okno. Rzeczywiste TCP/TLS sprawdzają miejsca przed TLS, dokładne budżety, łączne limity niezależne od czasu, izolację IP i odmowę przed błędną parą. Duży strumień TLS najpierw kończy logowanie, później bada NORMAL.

Wynik lokalny: **44/44 testów CTest w Debug, Release i ASan/UBSan**, także końcowy test opóźnionej pierwszej naliczonej próby, rzeczywisty ncdc ADC/ADCS, MariaDB i Apache HTTPS. WebAdmin: build/typecheck, 3 testy Node, 3 Chromium i rzeczywista integracja API/Vite/React przeszły. Metadane, UTF-8/linki/parytet języków, składnia Python i białe znaki Git poprawne.

Powtórzenie: pełny wymagany zestaw CMake jak poniżej; zakres etapu: `ctest --test-dir build-debug -R 'security.negotiation|security.ncdc' --output-on-failure`. Dotychczasowe flagi sanitizera/no-PIE i systemowy Chromium pozostają aktualne. To lokalne dowody, nie inne OS/CI ani audyt produkcyjnego obciążenia/bezpieczeństwa.

## STA/QUI — alpha-0.2.9

Pełna konfiguracja rejestruje **40 testów CTest**, dodając `adc.status`, `adc.status.process`, `adc.close.process`, `adc.status.ncdc` i `adcs.status.ncdc`. [STA/QUI](ADC_STATUS.md) opisuje semantykę, końcową kolejność/własność, przekazywanie statusów oraz rzeczywiste TCP/OpenSSL/ncdc. Test zamykania używa wyłącznie testowego GNU `--wrap=accept4`, wymusza zablokowane zapisy TCP/TLS i sprawdza bezwzględny termin bez zmian produkcyjnych gniazd lub sysctl hosta. Dotychczasowy relay ncdc dopuszcza reset/broken-pipe jako zamknięcie danej sesji przy reconnect/quit; pełne asercje protokołu/UI pozostają wymagane.

Wynik lokalny: **40/40 w Debug, Release i ASan/UBSan**, z wymaganymi integracjami, MariaDB i Apache HTTPS. WebAdmin: build/typecheck, 3 testy Node, 3 Chromium i rzeczywista integracja API C++/Vite/React także przeszły. Dotychczasowe flagi sanitizera/no-PIE i ustawienia Chromium opisane poniżej pozostają aktualne. Metadane, UTF-8/linki/parytet języków, składnia Python i białe znaki Git poprawne. Pozostałe OS/CI, dowolne awarie, wszystkie zależności TLS WANT_READ i produkcyjne audyty wymagają osobnych dowodów.

Zakres etapu: `ctest --test-dir build-debug -R 'adc.status|adcs.status|adc.close' --output-on-failure`; przed zakończeniem etapu należy uruchomić pełny zestaw wymaganych integracji.

## Logowanie — alpha-0.2.8

Konfiguracja wymagająca integracji rejestruje **35 testów CTest**. Nowe `adc.login` i `adc.login.process` sprawdzają serializację z limitami, częściowe uporządkowane wysyłki, unikalność/przyjęcia, równoczesne listy, aktualizacje/anulowanie pól, prywatność, odrzucenia/sprzątanie, rzeczywiste sesje TCP/TLS, termin logowania i izolację wolnego odbiorcy w obu transportach. Rozszerzony `adcs.crypto` wymusza blokadę zapisu SSL i porównuje dokładne odszyfrowane bajty. `adc.ncdc` i `adcs.ncdc` dowodzą pełnego NORMAL gościa i reconnect do C++; wcześniejsze testy modułów zachowują węższy historyczny zakres. Zobacz [ADC_LOGIN.md](ADC_LOGIN.md).

Wynik lokalny: **35/35 testów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganymi integracjami, MariaDB i Apache HTTPS. Test błędnego TLS dopuszcza odrzucenie rekordu przed półzamknięciem klienta; poprawione testy ADCS/logowania przeszły we wszystkich konfiguracjach. WebAdmin: build/typecheck, 3 testy Node, 3 testy Chromium i integracja rzeczywistego API C++/Vite/React. Flagi sanitizera: `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie`, linkowanie `-fsanitize=address,undefined -no-pie`. To wyniki lokalne; pozostałe OS/CI, obciążenie/fuzz i pełne wymuszanie WANT_READ/awarii pozostają osobno.

Powtórzenie: dotychczasowe polecenia CMake z wymaganymi integracjami; zakres modułu: `ctest --test-dir build-debug -R 'adc.login|adc.ncdc|adcs.ncdc|adcs.crypto' --output-on-failure`. Systemowy Chromium wybiera `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`. Kontrola metadanych/linków/parytetu języków, składni Python i białych znaków Git także przeszła.

## Etap CID/PID — alpha-0.2.7

Pełny zestaw rejestruje **33 pozycje CTest**, dodając `adc.identity`, `adc.identity.failure` i `adc.identity.ncdc`. Obejmują wektory/alias Tiger, kanoniczne CID/PID, wszystkie bajty spoza alfabetu w każdej pozycji, znaki końca, długości, czyszczenie błędnego wyjścia, 1000 par/mismatch, własność wyniku i zachowanie INF. Lokalne wrappery linkera wstrzykują awarie algorytmu/kontekstu/odczytu, sprawdzają brak zatwierdzenia, odzyskanie i sprzątanie. Prawdziwy ncdc odrzuca zmieniony CID przed prawdziwą parą w TCP i TLS Pythona, pozostając w niezatwierdzonym IDENTIFY. Testy rzeczywistego transportu C++ są osobne. Zobacz [ADC_IDENTITY.md](ADC_IDENTITY.md).

Wynik lokalny: **33/33 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z deterministycznymi awariami dostawcy, rzeczywistym C++ ncdc ADC/ADCS, fixture tożsamości/INF/SID/SUP, tymczasową MariaDB i izolowanym Apache HTTPS. Przeszły typecheck/build WebAdmin, 3 testy Node, 3 testy Chromium i rzeczywista integracja C++ API/Vite/React. Chromium używał `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`; wersje zależności są bez zmian. Wyniki lokalne nie dowodzą innych systemów/zdalnego CI ani audytu kanałów bocznych.

Odtworzenie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure --parallel 3`; powtórzyć z Release. Nowy zakres: `ctest --test-dir build-debug -R '^adc.identity' --output-on-failure`. Awarie wymagają linkera zgodnego z GNU --wrap; produkcja nie używa wrapperów. Inicjalizacja wersji i dowolne błędy alokacji nie są wstrzykiwane. Historyczne wyniki zachowują swój zakres.

## Etap INF — alpha-0.2.6

Pełny zestaw rejestruje teraz **30 pozycji CTest**, dodając `adc.inf` i `adc.inf.ncdc`. Zakres obejmuje wektory Tiger empty/abc i kolejność bajtów, kanoniczne base32/bity końca i 1000 rund, wymagane/duplikowane/prywatne/nieznane pola, granice liczb/tekstu/SU/pamięci, zgodność/normalizację IP, stały CID i aktualizacje, publiczne snapshoty oraz bramki dopuszczenia. Każdy podział na dwa fragmenty i bajtowy UTF-8 sprawdzają odzyskiwanie parsera i atomowe odrzucanie. Nowy fixture prawdziwego ncdc sprawdza PID/CID i eksportuje bez PD przez TCP loopback i TLS Pythona, pozostając w niezatwierdzonym IDENTIFY. Jest osobny od testów rzeczywistego transportu C++. Zobacz [ADC_INF.md](ADC_INF.md).

Wynik lokalny: **30/30 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganymi narzędziami, rzeczywistym C++ ncdc ADC/ADCS, fixture modułów, tymczasową MariaDB i izolowanym Apache HTTPS. Dodatkowe dokładne granice nicku/SU/łącznej pamięci INF i fixture ncdc przeszły we wszystkich trzech konfiguracjach. Przeszły build/typecheck WebAdmin, 3 testy Node, 3 testy Chromium i rzeczywista integracja C++ API/Vite/React. Chromium używał `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`. Zainstalowano Libgcrypt 1.11.0; wersje npm są bez zmian. Wynik lokalny nie dowodzi pozostałych systemów/zdalnego CI.

Zainstalować libgcrypt20-dev: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure --parallel 3`; powtórzyć z Release. Nowy zakres: `ctest --test-dir build-debug -R '^adc.inf' --output-on-failure`. Polecenia sanitizerów/Chromium poniżej nadal obowiązują. Brak algorytmu i pamięci jest obsłużony, ale nie wstrzykiwany w tych testach. Wyniki historyczne zachowują zakres swoich etapów.

## Etap SID — alpha-0.2.5

Pełny zestaw z wymaganymi narzędziami rejestruje **28 pozycji CTest**, w tym nowe `adc.sid` i `adc.sid.ncdc`. SID obejmuje wszystkie wartości kodera 20-bitowego, pełną przestrzeń puli/wyczerpanie, granice bitmapy 64-bit/zawijanie, reuse/churn, własność move-only i życie po zniszczeniu alokatora, błędne/powtórzone ISID, rollback przed wysyłką, wspólne sesje i sprzątanie terminalne. Fixture ncdc używa przydzielonego SID i osobno zgłasza ukończenia SUP/SID w TCP i TLS Pythona; kończy w niezatwierdzonym IDENTIFY. Dotychczasowe testy rzeczywistego Network/OpenSSL C++ są osobne. Zobacz [ADC_SID.md](ADC_SID.md).

Wynik lokalny: **28/28 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganymi narzędziami, rzeczywistym C++ ADC/ADCS ncdc, fixture SID/SUP, tymczasową MariaDB i izolowanym Apache HTTPS. Przeszły npm ci, typecheck/build WebAdmin, 3 testy Node, 3 testy przeglądarkowe Chromium i rzeczywista integracja C++ API/Vite/React. Przeglądarka używała `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`; zależności są bez zmian. To weryfikacja lokalna, nie dowód innych systemów/zdalnego CI.

Odtworzenie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure --parallel 3`; powtórzyć dla Release. Nowy zakres: `ctest --test-dir build-debug -R '^adc.sid' --output-on-failure`. Pełna przestrzeń SID ma limit 60 sekund; fixture ncdc 30 sekund. Wyniki historyczne poniżej zachowują zakres nazwanych etapów.

## Etap SUP — alpha-0.2.4

Pełny zestaw z wymaganymi narzędziami rejestruje **26 pozycji CTest**, dodając `adc.sup` i `adc.sup.ncdc`. Pierwszy obejmuje BASE/wspólny TIGR, deterministyczne osobne odpowiedzi lokalne, własność funkcji, kolejność AD/RM, limity zbioru/polityki i rollback, stały hash sesji, ręcznie błędne Message i połączenie z automatem. Strumień sprawdza każdy podział pierwszego SUP/powtórzenia. Drugi uruchamia prawdziwy ncdc przez tymczasowe TCP i TLS Pythona ze sterownikiem modułu C++, odbiera BINF i pozostaje w niezatwierdzonym IDENTIFY. Dotychczasowe testy prawdziwego Network/OpenSSL ncdc są osobne; brak pełnego logowania huba. Zobacz [ADC_SUP.md](ADC_SUP.md).

Wynik lokalny: **26/26 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z obowiązkowym wykrywaniem narzędzi integracyjnych. Przeszły build/typecheck WebAdmin, 3 testy Node, 3 testy przeglądarkowe Chromium i rzeczywista integracja C++ API/Vite/React. Przeglądarka używała `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`; wersje zależności są bez zmian. To wyniki lokalne; pozostałe systemy i zdalne CI wymagają osobnych dowodów.

Odtworzenie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4`, `ctest --test-dir build-debug --output-on-failure --parallel 3`; powtórzyć dla Release. Nowe testy: `ctest --test-dir build-debug -R '^adc.sup' --output-on-failure`. Dotychczasowe polecenia sanitizerów i Chromium poniżej nadal obowiązują. Wyniki historyczne zachowują zakres swoich etapów.

## Aktualny moduł stanów — alpha-0.2.3

Pełny zestaw obejmuje teraz **24 pozycje CTest**, w tym `adc.state`. Wszystkie przeszły lokalnie w Debug, Release i ASan/UBSan z obowiązkowym wykrywaniem narzędzi integracyjnych. Zakres stanów: gość/hasło, macierz komenda/stan/kierunek wszystkich komend BASE, jawne zatwierdzanie, powtórzenia/kolejność, podszyte SID, izolacja instancji, fatalne/lokalne/obce rozłączenia i pipeline/fragmentacja parser → automat. Nie oznacza to egzekwowania stanów w transporcie ani pełnego logowania. Oba transporty prawdziwego ncdc, tymczasowa MariaDB i izolowany Apache HTTPS pozostają osobno testowane. Zobacz [ADC_STATE.md](ADC_STATE.md).

Przeszły typecheck/build frontendu, 3 testy Node, 3 testy przeglądarkowe Chromium i rzeczywista integracja C++ API/Vite/React. Przeglądarka używała `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`, ponieważ domyślna pamięć przeglądarki Playwright była niedostępna. Wersje zależności są takie jak w alpha-0.2.2.

Odtworzenie: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, `cmake --build build-debug --parallel 4` i `ctest --test-dir build-debug --output-on-failure --parallel 3`; powtórzyć z Release. Sanitizery używały `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"` i `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"`. Wyniki historyczne poniżej zachowują zakres swoich etapów.

## Aktualny etap parsera — alpha-0.2.2

Pełny zestaw ma teraz **23 pozycje CTest**. Lokalnie wszystkie przeszły w Debug, Release i ASan/UBSan, w tym nowe `adc.syntax`, rozszerzone odzyskiwanie składni TCP/TLS i oba transporty prawdziwego ncdc. `adc.syntax` sprawdza 15 układów BASE, wymagane liczby argumentów, pola/anulowania/powtórzenia, nieinterpretowane rozszerzenia, granice liczb/selektorów/parametrów i własność wyników. Każdy podział oraz strumień bajtowy porównują wszystkie pola nagłówków, odkodowane wartości, układy i błędy zamiast samych liczników komend.

Build/typecheck frontendu, 3 testy Node, 3 przeglądarkowe i rzeczywista integracja C++/Vite/Chromium przeszły z nową wersją. Przeszła instalacja z panelem i bez niego, dokładne bajty instalacji, konfiguracja/wersja i kontrole metadanych/dokumentacji. Zobacz [kontrakt parsera](ADC_PROTOCOL.md), [polecenia budowania](BUILD.md) i [changelog](../../CHANGELOG.md). Pełne logowanie i semantyczna walidacja pól/stanu/tożsamości pozostają niezaimplementowane. Wyniki poniżej opisują etapy historyczne.

## Etap audytu — alpha-0.2.1

Pełny bieżący zestaw ma **22 pozycje CTest**, w tym `project.metadata`. Wszystkie przeszły lokalnie w Debug, Release i ASan/UBSan; przeszły też build frontendu, 3 testy Node, 3 przeglądarkowe i rzeczywista integracja API C++. CI używa `-DDC24H_REQUIRE_INTEGRATION_TESTS=ON`, odrzucając brak openssl/ncdc/MariaDB/Apache. Domyślny tryb opcjonalny może rejestrować mniej testów. Kontrola metadanych potrzebuje wyłącznie Pythona, bez Node i Git. Osobno sprawdzono negatywne przypadki konfiguracji i metadanych.

[Audyt](AUDIT_ALPHA_0.1.md) zapisuje dokładny SHA podstawy, zaobserwowany historyczny sukces CI main, środowisko, ustalenia i polecenia odtworzenia. Starsze wyniki poniżej opisują wskazane etapy historyczne. Początkowy HSUP ncdc nadal sprawdza transport, nie pełne logowanie huba. Debian 12, Ubuntu 26.04, długotrwałe obciążenie i fuzzing pozostają osobnymi zadaniami odbioru.

Uruchom `ctest --test-dir build --output-on-failure` i analogicznie dla `build-release`. Testy korzystają wyłącznie z loopback, portów 0 i danych tymczasowych, bez konfiguracji i udziałów użytkownika ncdc. Oczekiwania mają timeout i sprzątanie.

| Zestaw CTest | Zakres |
| --- | --- |
| `core.lifecycle` | Stany, jednorazowy run, stop przed startem, współbieżność, sygnały |
| `core.process` | CLI, wersja, pomoc, języki US/PL, SIGINT/SIGTERM i serie sygnałów |
| `network.unit` | Adresy, limity, stop i brak wycieków deskryptorów przy błędzie startu |
| `network.process` | 48 klientów, wiele portów, IPv6, limit i ponowne użycie slotu, FIN/reset, timeout, flood i limit deskryptorów |
| `adc.parser` | Nagłówki/komendy, escape, UTF-8/NFC, limity, fragmentacja, ucięcie i korpus losowy |
| `adc.process` | Strumienie, resynchronizacja, niezależne bufory i brak sekretów w logach |
| `adcs.crypto` | Negocjacja i odszyfrowanie OpenSSL z wcześniej zapełnioną kolejką błędów |
| `adcs.process` | Weryfikacja nazwy/certyfikatu, TLS 1.2/1.3, samo TLS, mieszane porty, fragmentacja, odrzucenie starego TLS, timeout negocjacji, błędne/zaszyfrowane klucze, reset w trakcie zapisu TLS, wielu klientów, duże strumienie i stop |
| `configuration.unit` | Składanie INI/CLI, ścieżki, granice i współbieżność loggera |
| `configuration.process` | Rzeczywiste limity, konfiguracja TLS, tryb sprawdzania i bezpieczeństwo plików |
| `logging.unit` | Poziomy, czyszczenie/UTF-8, współbieżność, odnowienie budżetu i PL |
| `logging.process` | Zdarzenia TCP/TLS, prywatność, niezależne ostrzeżenia, INI i błędy |
| `security.unit` | Zakresy, okna sztucznego zegara, limity klucza/mapy, czyszczenie i sloty |
| `security.process` | Rzeczywiste IP/TLS, ramki/błędy, terminy bezwzględne i INI/CLI |
| `adc.ncdc` | Początkowy HSUP rzeczywistego ncdc przez zwykły TCP |
| `adcs.ncdc` | ncdc negocjuje TLS z C++; przezroczysty relay, jeden odszyfrowany HSUP, brak jawnego HSUP na łączu |

Testy ncdc mają osobne katalogi i PTY. Relay ADCS nie kończy TLS w Pythonie: szyfrowanie jest realizowane przez ncdc i C++. Zamknięcie po timeout synchronizuje odczyt liczników. Testy TLS ufają jednorazowemu certyfikatowi i weryfikują nazwę hosta. Brak close_notify serwera jest jawnie uwzględniony. Pełne logowanie ADC i stan NORMAL nie są obsługiwane ani deklarowane.

Lokalnie: 16/16 zestawów CTest w Debug, Release i ASan/UBSan na Debian 13 (GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7, ncdc 1.23.1). Szczegóły: [CHANGELOG](../../CHANGELOG.md). CI ma macierz Ubuntu 22.04/24.04 Debug/Release, ale sam plik workflow nie potwierdza zdalnego sukcesu. Debian 12 i Ubuntu 26.04 wymagają osobnej weryfikacji. Nie są to benchmarki, audyt, długotrwały fuzzing ani test DDoS. Nie ma pełnego wstrzykiwania błędów pamięci i kernela.

## Konfiguracja — alpha-0.1.5

`configuration.unit` sprawdza wartości domyślne, pierwszeństwo, ścieżki, BOM/CRLF, UTF-8/control/rozmiary, równoważne IPv6, samo TLS, brakujący klucz z CLI, append, poziomy i wiersze logu z wielu wątków. `configuration.process` sprawdza rzeczywiste limity/timeout, kontrolę bez zajmowania portu i tworzenia logu, ścieżki i kontrolę TLS, chronione pliki/hardlinki/alias katalogu, FIFO/symlink/katalog, oczyszczone błędy i polski język. Oba testy ncdc uruchamiają teraz C++ przez INI z nadpisaniami CLI.

## Logging — alpha-0.1.6

Unit sprawdza UTF-8 i granice ucięcia, znaki ASCII/Unicode/ANSI, poziomy, domyślną flagę, współbieżne budżety, odnowienie sekundy/raport destruktora i PL. Procesy sprawdzają pliki warning/error, start, 200 błędnych ramek z jednym ostrzeżeniem, niezależne limity/bezczynność, błędne/zawieszone/poprawne TLS, brak sekretów/adresów i pierwszeństwo CLI. ncdc ADC/ADCS ma teraz włączone logi i wymaga zdarzeń przyjęcia/zamknięcia lub gotowości TLS. Nadal jest to początkowy HSUP, bez pełnego logowania ADC.

## Security — alpha-0.1.7

Unit obejmuje dolne/górne zakresy, dokładne granice sekundy/minuty bez rzeczywistego czekania, sloty, długie klucze, zapełnienie 65536 wpisów, czyszczenie nieaktywnych i zachowanie aktywnych. Procesy obejmują niezależne źródła loopback, wspólne TCP/TLS, odzysk slotu, flood poprawnych/błędnych/keepalive, dokładne 16 KiB, parametry/UTF-8, powolne częściowe ramki, nowy termin po ukończeniu poprzedniej ramki w tym samym odczycie, odrzucanie za długość, powolne TLS i odzysk oraz wszystkie granice CLI/INI. ncdc ma ustawienia Security i wymaga braku odrzuceń/zamknięć. Test dużego transportu TLS jawnie podnosi tempo do 100000 dla weryfikacji transportu niezależnie od domyślnej ochrony. Nie jest to benchmark DDoS ani audyt.

Rzeczywisty dostarczany przykład INI jest sprawdzany przez `--check-config`; instalacja do katalogu tymczasowego potwierdza zgodność kopii i akceptację przez zainstalowany program.

## Database — alpha-0.1.8

`database.unit` sprawdza opcje i granice prywatnego hasła, CRLF, uprawnienia, symlinki, hardlinki, odrzucenie katalogów/FIFO i limit 4096 bajtów. `database.process` inicjuje tymczasową MariaDB z własnym katalogiem i `--no-defaults`, całkowicie wyłącza TCP i sprząta proces. Zainstaluj `mariadb-server`, aby go włączyć; CMake jawnie informuje o brakujących narzędziach. Testy nie korzystają z bazy hosta. Sprawdzane są rzeczywiste połączenia, Unicode i ciągi przypominające SQL injection jako parametry, NULL, graniczne/zbyt duże/błędne wyniki skalarne i dalsze użycie po błędzie, konflikt blokady migracji, pierwsza/powtórna migracja, SHA-256 źródła, zmienione/obce wpisy migracji, start/stop z bazą, brak skutków ubocznych sprawdzenia, komunikaty US/PL, brak/błędne/publiczne hasło, aliasy log/hasło i błędne INI/operacje. TLS bazy nie został zaimplementowany w tym etapie.

Lokalna weryfikacja alpha-0.1.8: wszystkie 18 zestawów przeszło w Debug, Release i ASan/UBSan na Debian 13, MariaDB 11.8.6 i Connector/C 3.4.9, w tym oba testy rzeczywistego ncdc. Zdalne CI i pozostałe kombinacje systemów/baz nie są oznaczone jako zweryfikowane.

## Lokalne API — alpha-0.1.9

`api.unit` obejmuje ścisłe nagłówki, fragmentację w każdym prefiksie bajtów, kompletną granicę 8192 bajtów, duplikaty/niejednoznaczności, format/porównanie tokena, HEAD, deterministyczny korpus błędnych bajtów, jednorazowy start, 0600 gniazda, konkurencyjne serwery, zachowanie podmienionego inode i izolację błędu callbacku. `api.process` sprawdza rzeczywiste JSON/liczniki, autoryzację przed routingiem, 401/404/405/400/431, nagłówki ochronne, fragmenty, odrzucenie duplikatów/ramkowania/body/pipeline, wielu klientów, odzyskiwanie slotów, nieprzesuwalne terminy, aktywne zatrzymanie/restart, prywatne/błędne tokeny, stare/symboliczne/zwykłe ścieżki gniazda, kolizje, dry-run, wyłączenie, US/PL, rollback po błędzie Network oraz wymóg UID po poluzowaniu uprawnień plików (przypadek innego UID wymaga root, inaczej jest jawnie pomijany).

Oba testy rzeczywistego ncdc włączają API i odczytują rzeczywiste liczniki ADC/ADCS przed zatrzymaniem; nadal sprawdzają początkowe HSUP, nie pełne logowanie ADC. Test tymczasowej MariaDB sprawdza obecność włączonej bazy przez API i usuwanie gniazda. Testy API nie wymagają przeglądarki, curl ani dodatkowej usługi. CI zachowuje Ubuntu 22.04/24.04 Debug/Release; wyniki zdalne i inne systemy nie wynikają z lokalnych testów.

Lokalny wynik alpha-0.1.9: **20/20 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, w tym ncdc ADC/ADCS, tymczasowa MariaDB i odrzucenie innego UID, na Debian 13 / GCC 14.2 / OpenSSL 3.5.7 / MariaDB 11.8.6 / Connector/C 3.4.9. Sprawdzono instalację tymczasową, zgodność zainstalowanych INI/migracji, curl do zainstalowanego programu przez Unix z tokenem na stdin, względne ścieżki API, sprzątanie, UTF-8/linki/parzystość dokumentacji i białe znaki Git. Zdalne CI i inne systemy nie są tutaj oznaczone jako zweryfikowane.

## WebAdmin — alpha-0.1.10

Lokalnie: 20/20 CTest przeszło w Debug, Release i ASan/UBSan, w tym ncdc ADC/ADCS i tymczasowa MariaDB. Frontend: sprawdzenie TypeScript/build Vite, 3 testy jednostkowe Node, 3 testy Playwright oraz rzeczywista integracja C++ → Unix API → adapter Vite → Chromium zakończyły się sukcesem. Sprawdzono EN/PL i pamięć preferencji, desktop/mobile, dane dostępne/brakujące/błędne/utracone, eksport/nawigację, schemat i bezpieczne liczby, prywatne pliki/izolację katalogu, token tylko po stronie serwera, ochronę host/origin/site, przeciążenie oraz limity czasu/rozmiaru. Token nie pojawił się w żądaniach/HTML/JSON przeglądarki ani przez serwowanie plików Vite.

Przykład Apache2 przeszedł sprawdzenie składni i odizolowane serwowanie lokalne z CSP/blokadą API. Opcjonalna instalacja CMake zachowała dokładne bajty builda/przykładu/INI; zainstalowany program przyjął przykład. Audyt runtime npm nie zgłosił podatności w chwili wykonania; nie jest to audyt bezpieczeństwa. Środowisko: Debian 13, Node 20.19.2, npm 9.2, React 19.3.1, TypeScript 7.0.2, Vite 8.3.6, Chromium 156, Apache 2.4.69. CI ma osobny job Node 22, ale wyniki zdalne i inne systemy/przeglądarki nie są oznaczone jako zweryfikowane. Zobacz [WEBADMIN.md](WEBADMIN.md).

## Apache2 HTTPS — alpha-0.1.11

Lokalnie **21/21 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, w tym nowe `apache2.https`, rzeczywiste ncdc ADC/ADCS i tymczasowa MariaDB. Siedem przypadków Apache korzysta z polityki dostarczanego szablonu pod osobnym tymczasowym ServerRoot, na lokalnych efemerycznych portach, z jednorazowym certyfikatem/kluczem i zasobami fixture. Sprawdzono zaufane TLS 1.2/1.3, odrzucenie starszych wersji, stały redirect/ścieżki/query/Host, nagłówki sukcesu/błędów, HSTS tylko na HTTPS, blokady API/ukrytych ścieżek/sekretów/indeksów/symlinków, błędy zaufania/nazwy klienta, brak plików przy sprawdzaniu składni i rzeczywiste odrzucenie startu z niezgodną parą. Nie zmieniano witryn, zaufania, certyfikatów ani konfiguracji usługi hosta.

Wykrywanie Apache2/mod_ssl jest opcjonalne, a brak raportowany; CI instaluje Apache2. Typecheck/build frontendu, 3 testy Node, 3 przeglądarkowe i rzeczywista integracja C++/Vite/Chromium także przeszły z alpha-0.1.11. Instalacje tymczasowe sprawdziły zgodność szablonu HTTPS z opcjonalnymi zasobami WebAdmin i bez nich oraz poprawność zainstalowanego INI. Sprawdzono UTF-8/linki/parzystość języków i białe znaki Git. Lokalnie Apache 2.4.69 / OpenSSL 3.5.7 na Debian 13; zdalne CI i pozostałe systemy nie są oznaczone jako zweryfikowane. Zobacz [APACHE2_HTTPS.md](APACHE2_HTTPS.md).

Lokalna weryfikacja alpha-0.3.13: 67/67 zestawów CTest w Debug, Release i ASan/UBSan, w tym ncdc ADC/ADCS, MariaDB i Apache HTTPS. Przeszły produkcyjne budowanie WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Wyniki nie oznaczają wykonania zdalnego CI, pełnej macierzy systemów ani testów obciążenia produkcyjnego.

Lokalna weryfikacja alpha-0.4.1: 69/69 zestawów CTest w Debug, Release i ASan/UBSan, w tym kodek/proces MSG, prawdziwy ncdc ADC/ADCS, MariaDB i Apache HTTPS. Przeszły produkcyjne budowanie WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Bez deklaracji wykonania zdalnego CI, pełnej macierzy systemów ani obciążenia produkcyjnego.

Lokalna weryfikacja alpha-0.4.2: 72/72 zestawów CTest w Debug, Release i ASan/UBSan, w tym Chat Core/ADC/proces, publiczny czat prawdziwego ncdc ADC/ADCS, MariaDB i Apache HTTPS. Przeszły budowanie WebAdmin, 4 testy Node, 3 testy Chromium i integracja API C++. Końcowe testy Chat/logowania obejmują poprawkę instrumentacji alokatora. Bez deklaracji zdalnego CI, pełnej macierzy systemów/klientów i obciążenia produkcyjnego.

Lokalna weryfikacja alpha-0.4.3: 74/74 zestawów CTest w Debug, Release i ASan/UBSan, w tym Public Chat unit/proces i prawdziwy ncdc ADC/ADCS. Powtórzone testy Public Chat przeszły po poprawce instrumentacji alokatora. Przeszły MariaDB, Apache HTTPS, budowanie WebAdmin, 4 testy Node, 3 testy Chromium i integracja API C++. Bez deklaracji zdalnego CI, pełnej macierzy systemów/klientów i benchmarku.

Lokalna weryfikacja alpha-0.4.4: 76/76 zestawów CTest w Debug, Release i ASan/UBSan, w tym Private Chat unit/proces, prywatne rozmowy ncdc ADC/ADCS, regresje publiczne, MariaDB i Apache HTTPS. Przeszły budowanie WebAdmin, 4 testy Node, 3 testy Chromium i integracja API C++. Bez ostrzeżeń kompilatora. Wyniki lokalne nie oznaczają zdalnego CI, wszystkich systemów/klientów ani obciążenia produkcyjnego.

Weryfikacja lokalna alpha-0.4.5 (2026-11-11): wszystkie 78/78 zestawów CTest przeszło w Debug, Release i ASan/UBSan bez wyłączania wykrywania wycieków, w tym Message Routing oraz rzeczywisty ncdc ADC/ADCS z czatem publicznym/prywatnym, akcjami i reconnect. Przeszły również tymczasowa MariaDB, Apache HTTPS, kompilacja produkcyjna WebAdmin, 4 testy Node, 3 testy Chromium oraz integracja z rzeczywistym API C++. Zakres i polecenia odtworzenia opisuje [Message Routing](MESSAGE_ROUTING.md). Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają osobnymi zadaniami.

alpha-0.4.6: Weryfikacja lokalna z 2026-11-11: **80/80 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, w tym rzeczywisty ncdc ADC/ADCS. Nie wyłączano wykrywania wycieków; kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły kompilacja produkcyjna/typecheck WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych, lokalne odnośniki, składnię Pythona, pomoc US/PL i białe znaki Git. Są to wyniki lokalne; nie potwierdzają zdalnego CI, pełnej macierzy systemów/klientów ani obciążenia produkcyjnego.

alpha-0.4.7: Weryfikacja lokalna z 2026-11-11: **82/82 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganiem narzędzi integracyjnych i bez wyłączania wykrywania wycieków. Obejmuje to rzeczywisty ncdc ADC/ADCS, Hub Messages, regresje czatu/sesji/kolejek, tymczasową MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły kompilacja/typecheck WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych, lokalne odnośniki, składnię zmienionych testów Pythona, pomoc US/PL i białe znaki Git. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.

alpha-0.4.8: Weryfikacja lokalna z 2026-11-11: **84/84 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganiem narzędzi integracyjnych i bez wyłączania wykrywania wycieków. Obejmuje to rzeczywisty ncdc ADC/ADCS, Chat Validation, regresje czatu/sesji/kolejek, tymczasową MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły kompilacja/typecheck WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych, lokalne odnośniki, składnię zmienionych testów Pythona, pomoc US/PL i białe znaki Git. Zdalne CI, pełna macierz systemów/klientów, obciążenie produkcyjne i długotrwały fuzzing pozostają niezweryfikowane.

alpha-0.4.9: Weryfikacja lokalna z 2026-11-11: **86/86 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganiem narzędzi integracyjnych i bez wyłączania wykrywania wycieków. Obejmuje to rzeczywisty ncdc ADC/ADCS przy włączonych limitach domyślnych, Anti-Flood/konfigurację, regresje czatu/sesji/kolejek, tymczasową MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły kompilacja/typecheck WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono przykładowe INI, metadane, UTF-8, zgodność plików językowych, lokalne odnośniki, składnię zmienionych testów Pythona, pomoc US/PL i białe znaki Git. Zdalne CI, pełna macierz systemów/klientów, długie wrogie obciążenie i fuzzing pozostają niezweryfikowane.
