# Message Flags — alpha-0.4.6

**Bieżący etap — alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) dodaje zaufane systemowe IMSG powiązane z życiem sesji i stałe powitanie po własnym BINF. Klient nie może nadawać komunikatów systemowych; zachowano limity kolejek i gotowość przy końcowym LF.

Etap bazuje bezpośrednio na main `1e83114eb613dc59b10c6956025f14e5467f24a8`, po scaleniu alpha-0.4.5 Message Routing. Wcześniejsze gałęzie i historia są zachowane. Serwer używa EN/US UTF-8, polskich komunikatów operatora oraz dokumentacji US/PL.

## Kontrakt PM i ME

Ten etap wydziela obsługę PM/ME wprowadzoną wcześniej w kodeku MSG i adapterach czatu do `adc/msg_flags.hpp` oraz `src/adc/msg_flags.cpp`. Kodek, lokalny builder i obie projekcje czatu współdzielą teraz dekodowanie flag i budowanie kanonicznych parametrów. Zachowano dotychczas akceptowane komunikaty i politykę routingu.

Znaczenie flag odpowiada [ADC 1.0.4, MSG](https://adc.sourceforge.io/ADC.html): PM określa SID odpowiedzi w rozmowie; zwykła rozmowa prywatna używa SID źródła. ME1 żąda prezentacji w trzeciej osobie. Flagi nie uwierzytelniają nadawcy ani nie określają członkostwa odbiorców.

| Wejście | Wartość typowana / zachowanie routingu |
| --- | --- |
| Brak PM | Nie są dodawane metadane prywatnej rozmowy |
| PM z poprawnym czteroznakowym SID base32 | `private_group` przechowuje sprawdzoną wartość SID |
| Brak ME | `third_person` jest nieobecne |
| ME0 | Jawne `third_person=false`; wyjście zachowuje ME0 |
| ME1 | `third_person=true`; wyjście zachowuje ME1 |
| ME-0, ME000 | Poprawne całkowite zero; routing zapisuje ME0 |
| ME01, ME0001 | Poprawna całkowita jedynka; routing zapisuje ME1 |
| Samo PM/ME, niepoprawny SID, inne liczby/przepełnienie | Błąd semantyczny; fatalne ISTA 240 i własne IQUI |
| Powtórzone PM lub ME, także identyczne albo ME0 | Błąd semantyczny; brak częściowej publikacji |

Znane pola nazwane mogą występować w dowolnej kolejności i być przeplatane nieznanymi. Flagami są wyłącznie tokeny po pojedynczym parametrze tekstu. Tekst `PMBBBB ME1`, również ze spacjami i nowymi liniami zapisanymi przez escape, pozostaje zwykłą treścią. Każde parsowanie tworzy nowe wartości; flagi nie przechodzą z poprzedniej wiadomości.

Dotychczasowy profil liczb akceptuje zapis dziesiętnej liczby całkowitej ze znakiem mieszczącej się w 64 bitach, o wartości zero lub jeden, w tym początkowe zera i ujemne zero. Plus, białe znaki, pusty zapis, inne wartości i przepełnienie są odrzucane. Surowy kodek zachowuje poprawną pisownię wejścia, wartości rozszerzeń, powtórzenia nieznanych pól i kolejność. Lokalne buildery i projekcje routingu zapisują wyłącznie znane flagi, PM przed ME, z kanonicznym ME0/ME1. Nieznane metadane nie są rozsyłane.

## Wspólne API i granica walidacji

- `MsgFlags` posiada `optional<Sid> private_group` oraz `optional<bool> third_person`. Nie przechowuje widoków tekstu, odwołań do rejestru ani zasobów transportu.
- `parse_msg_flags(span<const string>) noexcept` czyta odkodowane tokeny nazwane bez tekstu pozycyjnego. Bez alokacji sprawdza postać tokenów, wartości i duplikaty PM/ME, ogranicza liczbę pól do 127 i odrzuca pojedynczy token powyżej 16384 bajtów. Pomocniczy parser ignoruje wartości nieznanych pól.
- `make_msg_parameters(text, flags)` zwraca nowy wektor tekstu i kanonicznych opcjonalnych flag. Sprawdza niepusty, ograniczony tekst UTF-8/NFC, dopuszcza odkodowane LF i odrzuca pozostałe sterujące ASCII/DEL. Niepoprawny tekst lokalny powoduje `invalid_argument`; błędy alokacji są propagowane. Tekst i flagi wywołującego nie są modyfikowane.
- `MsgView::flags()` zwraca samodzielną wartość flag, natomiast tekst MsgView nadal pożycza pamięć komunikatu. Zachowano istniejące pola MsgView/MsgOptions i wejścia kodeka.

Sam moduł flag nie sprawdza nieznanych wartości, nagłówka, całkowitej długości zakodowanej ani uprawnień. Pełne `parse_msg`/`validate_msg`/`serialize_msg` zachowują te kontrole, w tym UTF-8/NFC nieznanych pól, 128 wszystkich parametrów i limit 16384 bajtów zakodowanej treści ramki plus końcowe LF. Bajty PM/ME, separatory i rozwinięcia escape są wliczane. Wektor składników nie jest zatwierdzoną ramką sieciową.

## Routing i prywatność

`MessageRouter` nadal wiąże gotowe źródło z połączeniem i domeną Chat Core. BMSG bez PM jest publiczne, także z ME0/ME1. Każde BMSG z PM jest odrzucane przez odwracalne ISTA 125 i nigdy nie staje się publiczną treścią. DMSG wybiera cel; EMSG cel i nadawcę, pojedynczo przy wiadomości do siebie. Obecne PM musi odpowiadać źródłu zwykłej rozmowy prywatnej; inny poprawny grupowy SID pozostaje nieobsługiwany z ISTA 125. PM nie jest używane jako drugi adres odbiorcy. ME zmienia wyłącznie metadane prezentacji, nigdy odbiorców ani tożsamość źródła.

Przykładowo wejście `EMSG AAAB AAAC hello ME01 XXopaque PMAAAB` jest rozsyłane jako `EMSG AAAB AAAC hello PMAAAB ME1` z końcowym LF. Następna wiadomość bez flag nadal nie ma flag. Nie są dla niej tworzone PM ani ME.

Błędy składni nadal są odrzucane przez parser ogólny w istniejących limitach; błędy znanych pól są fatalne, nieobsługiwane grupy/zakresy odwracalne, a brak celu zachowuje ISTA 140. Kolejność kontroli stanu/źródła, czas życia kolejek, FIFO, limity, współdzielone niemutowalne pakiety i izolacja błędów odbiorców pozostają zachowane. Tekst i nieznane metadane nie trafiają do logów, zapisów bazy ani API/eksportów administracyjnych.

Nie dodano konfiguracji, zależności, migracji SQL, pól JSON API ani funkcji WebAdmin. Istniejąca pomoc US/PL objaśnia teraz PM/ME. Czat grupowy, historia/moderacja, wiadomości offline i uprawnienia kont pozostają osobnymi etapami.

## Weryfikacja

`adc.msg.flags` obejmuje brak/false/true, własność SID, dekodowanie bez alokacji, klasyfikację duplikatów/błędów, granice pól, odrzucanie UTF-8/NFC przez pełny kodek, tekst przypominający flagi, nieznane rozszerzenia, pełne limity zakodowane oraz macierz odbiorców/projekcji B/D/E z PM/ME. Dotychczasowe testy awarii alokacji, domen/czasu życia, przeciążenia i współdzielonych ramek sprawdzają zmienione ścieżki.

`adc.msg.flags.process` sprawdza oba transporty źródła, przeplatane pola i kanoniczne wyjście, publiczne/prywatne kombinacje akcji i tekstu, brak dziedziczenia flag, dokładnych odbiorców z uporządkowanymi znacznikami publicznymi, odmowę grup/broadcastu, fatalne zamknięcie tylko źródła i prywatność logów/API. Rzeczywiste `adc.ncdc` i `adcs.ncdc` dodatkowo odbierają prywatne ME0/ME1 i wysyłają prywatną akcję z karty rozmowy PM. Zachowano sprawdzenia publicznego/prywatnego czatu, akcji, reconnect i round-trip kodeka dla nieszyfrowanego transportu.

Odtworzenie z wymaganiem narzędzi integracyjnych:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(adc.msg.flags|adc.ncdc|adcs.ncdc)' --output-on-failure
npm --prefix webadmin run build
npm --prefix webadmin test
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm --prefix webadmin run test:e2e
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm --prefix webadmin run test:integration -- ../build-debug/dc24h
```

Bieżące wyniki weryfikacji zapisano w [CHANGELOG.md](../../CHANGELOG.md). Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne wymagają osobnych dowodów.

Weryfikacja lokalna z 2026-11-11: **80/80 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, w tym rzeczywisty ncdc ADC/ADCS. Nie wyłączano wykrywania wycieków; kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły kompilacja produkcyjna/typecheck WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych, lokalne odnośniki, składnię Pythona, pomoc US/PL i białe znaki Git. Są to wyniki lokalne; nie potwierdzają zdalnego CI, pełnej macierzy systemów/klientów ani obciążenia produkcyjnego.
