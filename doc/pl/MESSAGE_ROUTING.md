# Message Routing — alpha-0.4.5

**Bieżący etap — alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) grupuje startowe ustawienia czatu i dodaje limit odkodowanego tekstu, niezależne przełączniki publicznego/prywatnego/akcji i opcjonalne własne powitanie. Domyślne wartości zachowują działanie; odwracalne odmowy nie omijają walidacji ani Anti-Flood.

**Etap dostarczony w alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

**Etap dostarczony w alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizuje sprawdzanie tekstu, SID i gotowości/własności nadawcy w obu adapterach. Wybór życia odbiorcy i ponowna kontrola dispatch pozostają obowiązkowe; systemowe IMSG jest oddzielone.

**Etap dostarczony w alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) dodaje zaufane systemowe IMSG powiązane z życiem sesji i stałe powitanie po własnym BINF. Klient nie może nadawać komunikatów systemowych; zachowano limity kolejek i gotowość przy końcowym LF.

**Etap dostarczony w alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizuje dekodowanie PM/ME i kanoniczne wyjście kodeka oraz obu adapterów czatu. Brak ME, ME0 i ME1 pozostają rozróżniane; PM nie nadaje uprawnień do dostarczania.

Etap bazuje bezpośrednio na main `0c627eb1430ab0c750674a8463e64060c96c8a86`, po scaleniu alpha-0.4.4 Private Chat. Zachowano gałęzie alpha-0.4.1–alpha-0.4.4 i historię. Serwer oraz kod używają EN/US UTF-8; dokumentacja i katalogi komunikatów obsługują też polski.

## Zakres i odpowiedzialność

`adc::MessageRouter` w `adc/message_routing.hpp` i `message_routing.cpp` jest wspólnym wejściem walidacji i rozsyłania MSG klientów. Łączy istniejące [Public Chat](PUBLIC_CHAT.md), [Private Chat](PRIVATE_CHAT.md), [kodek MSG](ADC_MSG.md) i niezależny od protokołu [Chat Core](CHAT_CORE.md). LoginHub posiada jeden router i wspólny adapter wyjściowy czatu. Gniazda TCP/TLS, limity, offsety, terminy i sprzątanie błędów pozostają u dotychczasowych właścicieli. Routing STA, obecność/INF/QUI, wyszukiwanie i transfery nie przechodzą przez tę usługę MSG.

Router porównuje zakres enum, aby wybrać adapter. Oba adaptery wywołują wspólne validate_chat przed utworzeniem własnego planu: chroniony dostęp do opcjonalnego SID, ograniczony tekst, pełna składnia MSG i własność źródła przez gotowego nadawcę. Bezpośredni użytkownik adaptera otrzymuje te same kontrole. W działającym serwerze wcześniej działa maszyna stanów sprawdzająca kierunek, NORMAL, źródło i znane pola. Zakresy H/I bez źródła nie mogą być rozsyłane przez router klientów. Zobacz [Chat Validation](CHAT_VALIDATION.md).

| Wejście | Wybór / wynik |
| --- | --- |
| BMSG bez PM | Migawka gotowych odbiorców wraz z nadawcą |
| DMSG, opcjonalne PM równe źródłu | Tylko gotowy cel |
| EMSG, opcjonalne PM równe źródłu | Gotowy cel, następnie nadawca; wiadomość do siebie raz |
| BMSG z PM | `private_broadcast`; odwracalne ISTA 125 |
| D/E z innym początkiem PM | `unsupported_group`; odwracalne ISTA 125 |
| F/H/I lub inny nieobsługiwany zakres MSG | Bez dostarczania; istniejące 125/odrzucenie stanu dla trybów klienta |
| Brakujący, oczekujący lub zamykany cel prywatny | `recipient_unavailable`; odwracalne ISTA 140 |

Prywatne i nieobsługiwane zakresy nigdy nie stają się broadcastem. Nieobsługiwane grupowe PM jest odrzucane bez wyszukiwania członkostwa lub kopii do nadawcy. Projekcje publiczna/prywatna usuwają nieznane pola. Tekst oraz opcjonalne ME/PM zachowują dotychczasową semantykę i ograniczenia kodeka; treść nie jest wykonywana jako kolejna komenda. Błędy stanu/źródła/znanych pól zachowują ISTA 244/240 i zamknięcie z własnym IQUI.

Reguły odbiorców B/D/E odpowiadają [ADC 1.0.4, typy komunikatów i MSG](https://adc.sourceforge.io/ADC.html). Dostarczanie czatu grupowego i według cech pozostaje poza tym etapem.

## Domeny silnika i czas życia

Każdy Chat Core tworzy prywatny współdzielony token domeny routingu. Plany Broadcast i Directed przechowują token wraz z niemutowalnymi danymi i wybranymi identyfikatorami życia sesji. Dispatch odrzuca obcą domenę przez `chat::Error::foreign_plan` przed wywołaniem odbiorcy. Identyczne SID/ID sesji w innym rejestrze lub silniku nie autoryzują planu. Zachowanie tokena chroni także przed użyciem tego samego adresu pamięci po zniszczeniu Core. Token nie wydłuża życia Core ani rejestru; rozsyłać może wyłącznie istniejący silnik. Konstruktor Core może alokować i nie jest już `noexcept`.

Macierzysty Core ponownie sprawdza nadawcę podczas dispatch i rozwiązuje każdego zapisanego odbiorcę bezpośrednio przed kolejkowaniem. Prywatny dispatch sprawdza też pierwotne życie celu i SID przed dostarczeniem lub kopią do nadawcy. Usunięty/zastąpiony cel nie otrzymuje treści i nie powstaje echo. Zmiana nicka aktywnej osoby nie zmienia routingu. Plany publiczne pomijają późniejsze logowania oraz oczekujące/zamykane/usunięte sesje. Plan adaptera po przeniesieniu nie ma pakietu i jest odrzucany.

Zaufany callback może zamykać/usuwać sesje. Przechodzenie po odbiorcach nie zachowuje iteratora rejestru ani pożyczonego modelu między wywołaniami. Po rozpoczęciu publikacji odmowa/zamknięcie jednego odbiorcy, także nadawcy, nie anuluje już przyjętej próby dla pozostałych. Brak atomowości między klientami i gwarancji ponawiania. Moduły pozostają w jednym wątku reaktora.

## Kolejki i błędy

`RoutedMessage` wybiera PublicBroadcast lub PrivateDelivery; `RoutedFrame` wybiera współdzielony niemutowalny pakiet publiczny/prywatny. Router nie serializuje tekstu osobno dla odbiorców i nie alokuje podczas dispatch po utworzeniu callbacka. LoginHub używa jednej funkcji kolejkowania dla obu typów pakietów. Odbiorcy zachowują niezależne FIFO/offsety/pełne logiczne obciążenie w istniejących limitach klienta i globalnych. Współdzielenie pamięci nie zmniejsza logicznych opłat wyjściowych. Częściowe wysłanie TLS zachowuje adres i pozostałą długość niemutowalnego początku kolejki.

Błąd alokacji kolejki zamyka tylko danego odbiorcę przez `internal_error`; przekroczenie limitu zamyka go przez `output_limit`. Pozostali wybrani odbiorcy są nadal obsługiwani. Błąd alokacji przygotowania/diagnostyki zamyka źródło bez częściowej publikacji. DispatchReport liczy przyjętych, niedostępnych i odrzuconych odbiorców; przyjęcie oznacza kolejkę, a nie odebranie/przeczytanie. Zaufane callbacki nie mogą rzucać wyjątków; częściowego dispatch nie wolno ponawiać. Zakończenie, anulowanie ogona i rozłączenie zachowują dotychczasowe zwalnianie obciążeń/zasobów.

Nie dodano konfiguracji, zależności, migracji SQL, pól API, funkcji WebAdmin, kluczy tłumaczeń, uprawnień kont, historii, moderacji ani nowej polityki limitowania czatu. Nadal obowiązują limity wejścia/wyjścia. Treść i nieznane pola nie trafiają do logów, bazy ani eksportów administracyjnych. ADCS chroni transport; routing odbywa się przez hub i nie dodaje szyfrowania między klientami.

## Weryfikacja i bieżące środowisko

`chat.routing` sprawdza macierz B/D/E, pojedyncze wiadomości do siebie, oczyszczone ramki, tożsamość współdzielonych pakietów, dispatch bez alokacji, zakresy prywatne/nieobsługiwane, wadliwe obiekty, brakującego/oczekującego nadawcę, ponowne użycie SID celu, brakujące/zamykane źródło, przeniesione plany, kolizje między rejestrami i odtworzenie Core pod tym samym adresem. Kampania awarii przygotowania sprawdza brak mutacji rejestru. Test callbacka zamyka źródło, zastępuje inny SID i sprawdza dalsze izolowane dostarczanie do pozostałego zapisanego życia sesji.

`chat.routing.process` dodaje przeplatane serie B/D/E na czterech rzeczywistych połączeniach ADC/ADCS, dokładne FIFO każdego odbiorcy, tekst przypominający komendy z escapowanymi znakami, odzyskiwanie po nieobsługiwanych trybach, pomijanie oczekującego powitania, reconnect, odmowę starego SID, odrzucenie podszytego źródła i prywatność logów/API. Istniejące `adc.ncdc` i `adcs.ncdc` sprawdzają rzeczywisty ncdc: czat publiczny/prywatny, akcje i ponowne logowanie przez nowy router.

Weryfikacja lokalna z 2026-11-11: wszystkie 78/78 zestawów CTest przeszło w Debug, Release i ASan/UBSan z wymaganiem narzędzi integracyjnych i bez wyłączania wykrywania wycieków. Obejmuje to rzeczywisty ncdc ADC/ADCS z czatem publicznym/prywatnym, akcjami i reconnect, routing na obu transportach, tymczasową MariaDB, Apache HTTPS oraz uwierzytelnione API. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły również typecheck i kompilacja produkcyjna WebAdmin, 4 testy Node, 3 testy Chromium oraz integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych, lokalne odnośniki Markdown i białe znaki Git. Obciążenie produkcyjne, zdalne CI oraz pełna macierz systemów/klientów pozostają niezweryfikowane.

Odtworzenie pełnego zestawu integracyjnego:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 2
ctest --test-dir build-debug --output-on-failure
npm --prefix webadmin run build
npm --prefix webadmin test
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm --prefix webadmin run test:e2e
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm --prefix webadmin run test:integration -- ../build-debug/dc24h
```
