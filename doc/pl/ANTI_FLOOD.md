# Anti-Flood — alpha-0.4.9

Etap bazuje bezpośrednio na main `162ba385ce9b03a870fe44846943727537c7a35c`, po scaleniu Chat Validation. Kod i opisy protokołu używają EN/US UTF-8; komunikaty/pomoc operatora i dokumentacja obsługują też polski. Wcześniejsze gałęzie i historia są zachowane.

## Polityka i zakres

Każde rzeczywiste połączenie Network otrzymuje własny `chat::FloodGuard`, skonfigurowany przez sprawdzone opcje Network i przechowywany przez odpowiedni peer LoginHub. Wszystkie BMSG, DMSG i EMSG współdzielą dwa budżety źródła: liczbę wiadomości i odkodowane bajty tekstu UTF-8. Zmiana celu, PM/ME, zakresu publicznego/prywatnego lub nicka nie tworzy nowego budżetu. EMSG liczy się raz wraz z kopią do źródła; broadcast liczy wejściowy tekst raz, nie dla każdego odbiorcy.

Budżety zaczynają pełne, odnawiają się ciągle według zegara monotonicznego i nie przekraczają pojemności serii. Oba muszą pozwalać na wiadomość; przyjęcie obciąża je razem. Odmowa nie zużywa żadnego budżetu, ale nie resetuje odnowienia ani okna powiadomień. Wiadomość większa niż pojemność bajtowa nie przejdzie. Minimalna pojemność bajtowa mieści każdą legalną odkodowaną treść MSG.

| Klucz INI `[chat]` | Domyślnie | Zakres |
| --- | --- | --- |
| `max_messages_per_second` | 5 | 1–1000000 |
| `message_burst` | 10 | 1–1000000 |
| `max_bytes_per_second` | 32768 | 1–16777216 |
| `byte_burst` | 131072 | 16384–16777216 |

Nowe połączenie może np. wysłać natychmiast dziesięć małych wiadomości, a następnie zyskuje pięć jednostek na sekundę, z uwzględnieniem budżetu bajtów. Tempo i seria są niezależne: seria może być mniejsza od tempa sekundowego. Koszt bajtowy nie obejmuje nagłówków ADC, flag, separatorów i rozwinięcia escape; te nadal obejmuje pełny limit ramki i ogólne limity bezpieczeństwa. Spacja kosztuje jeden odkodowany bajt, choć `\s` ma dwa bajty na sieci.

Guard przechowuje tylko stan liczb/czasu o stałym rozmiarze. Nie ma historii, odcisków powtarzanego tekstu, nieograniczonej mapy ani logowania treści. Identyczne wiadomości są ograniczane tempem tak jak różne. Jest to ograniczanie spamu przez tempo, nie moderacja, sankcje kont ani mechanizm banów.

## Miejsce kontroli i protokół

Dotychczasowe limity wszystkich ramek, błędnego wejścia, terminów i maszyna stanów pozostają pierwsze. Kierunek, NORMAL/gotowość, własność źródła i znane pola MSG są sprawdzane przed Anti-Flood. Błędne źródło lub PM/ME zachowuje fatalną diagnostykę nawet przy pustym budżecie. Ogólnie błędne ramki nadal są odrzucane według limitów bezpieczeństwa.

Dla poprawnego B/D/E gotowej sesji LoginHub sprawdza guard przed przygotowaniem routingu, migawką odbiorców i prywatnym echem. Liczy to także poprawne próby B/D/E, które następnie nie przejdą polityki PM/grupy/celu; ponawiane niedostępne cele lub odmowy PM nie omijają limitu. Odmowa nie publikuje czatu nikomu, także źródłu EMSG. Nie tworzy zaległej kolejki ani zadania ponowienia.

Pierwsza odmowa kolejkuje do źródła stałe `ISTA 100 Chat\smessage\srate\slimit\sexceeded FC<FOURCC>`. Kolejne odmowy tworzą najwyżej jeden taki komunikat na sekundę łącznie dla B/D/E; sukces nie resetuje tego okna. Kod 100 używa odwracalnej powagi i kategorii ogólnej z [ADC STA](https://adc.sourceforge.io/ADC.html). Opis nie zawiera tekstu klienta, SID, nicka ani adresu. Powiadomienie korzysta z ograniczonej kolejki; błędy alokacji/limitu zachowują zwykłe zamknięcie źródła.

Sama odmowa tempa pozostawia połączenie NORMAL i gotowe. Po odnowieniu późniejsze wiadomości mogą przejść. Odrzucone wiadomości są tracone, a nie automatycznie ponawiane. Ogólne limity wejścia/wyjścia nadal mogą zamknąć nadużywające lub wolne połączenie. Odmowy kolejek odbiorców i przypięte wyjście TCP/TLS pozostają bez zmian.

Powitania IMSG/zaufane wiadomości huba, obecność INF, keepalive, STA oraz nieobsługiwane H/F są poza budżetami B/D/E klienta. Nadal podlegają dotychczasowym kontrolom stanu, ogólnego wejścia, terminów i wyjścia. Nie dodano nowej nielimitowanej ścieżki czatu publicznego/prywatnego.

## Własność, zegary i konfiguracja

`chat/anti_flood.hpp` i `src/chat/anti_flood.cpp` dostarczają `FloodLimits`, walidację i `FloodGuard::admit(decoded_bytes, now)`. Jednostki są całkowitymi nano-tokenami, zachowując ułamkowe odnowienie bez dryfu zmiennoprzecinkowego. Mnożenie jest ograniczone brakującą pojemnością; nadmierna liczba bajtów nie przepełnia obciążenia. Skrajne zegary testowe nasycają odnowienie, a cofnięcie czasu jest ograniczane bez tworzenia kredytu. Sprawdzenia nie alokują i należą do wątku reaktora.

Network zawsze przekazuje skonfigurowane limity do `LoginLimits::anti_flood`; nie dodano zera/opcji wyłączenia w INI ani nowych przełączników CLI. Niższy zaufany interfejs LoginHub pozwala pominąć politykę, tak jak inne kontrole należące do transportu. Bezpośrednie Core/adaptery i zaufane systemowe API nie są bramkami tempa sieci. Testy korzystające z tego interfejsu muszą jawnie podać politykę przy testowaniu Anti-Flood.

`LoginHub::receive` przyjmuje opcjonalny monotoniczny czas dla deterministycznych testów; produkcyjnie używa aktualnego czasu. Ten parametr steruje guardem czatu, nie wszystkimi zegarami modelu/terminami. Nie zmienia się zegara systemowego. Sprzątanie połączenia usuwa guard. Reconnect zaczyna nowy budżet; brak trwałego limitu czatu konta/CID/IP. Dotychczasowe wspólne limity połączeń/przyjęć/tożsamości IP nadal działają, a klienci za jednym IP nie współdzielą nowego budżetu wiadomości.

Przykładowe INI opisuje cztery wartości domyślne. Nieznane/powtórzone klucze, zero/liczby ujemne/przepełnienia i limity spoza zakresu kończą walidację błędem przed startem. Pomoc US/PL opisuje domyślne wartości i odzyskiwanie. Ustawienia są startowe; nie dodano przeładowania ani edycji przez WebAdmin.

Nie dodano zależności, migracji, pól JSON API ani endpointu. Dotychczasowe `adc_frames` nadal liczy sparsowane ramki wejściowe, także czat odrzucony tempem; nie jest licznikiem dostarczeń. Nie eksportuje się nowych liczników flood ani treści. Limity wyjścia nadal obciążają każdego rzeczywistego odbiorcę; limit bajtów wejścia nie gwarantuje przepustowości fanout ani wydajności produkcyjnej.

## Weryfikacja

`chat.anti_flood` obejmuje dokładne ułamkowe odnowienie wiadomości/bajtów, granice serii, łączne obciążenie budżetów, sekundowe powiadomienia także po sukcesie, stałą pamięć i brak alokacji, przepełnienia rozmiaru i min/max/cofnięcie zegara, zakresy konfiguracji, wspólne B/D/E/ME, niezależność źródeł, odzyskiwanie, wiadomości systemowe, pierwszeństwo fatalnego źródła i zwalnianie obciążeń.

`chat.anti_flood.process` sprawdza rzeczywiste ADC/ADCS i INI: mieszane serie publiczne/prywatne/akcji, dokładnych odbiorców przyjętych ramek, ograniczoną diagnostykę, brak kopii/echa po odmowie, zdrowych klientów, odnowienie, fatalne podszywanie się, reset nowego życia po reconnect i prywatność logów/API. Osobny test bajtów sprawdza koszt odkodowanych spacji i odnowienie niezależnie od liczby wiadomości. Testy jednostkowe/konfiguracji sprawdzają wszystkie klucze, zakresy, duplikaty i polską diagnostykę. Rzeczywisty ncdc ADC/ADCS zachowuje zwykłe testy czatu publicznego/prywatnego, akcji, powitania i reconnect przy włączonej polityce domyślnej.

Dotychczasowe procesowe testy dużych serii czatu jawnie ustawiają wysoki poprawny budżet `[chat]`, aby niezależnie sprawdzać kolejność/kodek/transport; Anti-Flood nie jest wyłączany produkcyjnie ani pomijany bez informacji w testach.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(chat.anti_flood|configuration|adc.ncdc|adcs.ncdc)' --output-on-failure
```

Bieżące wyniki opisuje [CHANGELOG.md](../../CHANGELOG.md). Zdalne CI, pełna macierz systemów/klientów, długie wrogie obciążenie i fuzzing wymagają osobnych dowodów.

Weryfikacja lokalna z 2026-11-11: **86/86 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganiem narzędzi integracyjnych i bez wyłączania wykrywania wycieków. Obejmuje to rzeczywisty ncdc ADC/ADCS przy włączonych limitach domyślnych, Anti-Flood/konfigurację, regresje czatu/sesji/kolejek, tymczasową MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły kompilacja/typecheck WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono przykładowe INI, metadane, UTF-8, zgodność plików językowych, lokalne odnośniki, składnię zmienionych testów Pythona, pomoc US/PL i białe znaki Git. Zdalne CI, pełna macierz systemów/klientów, długie wrogie obciążenie i fuzzing pozostają niezweryfikowane.
