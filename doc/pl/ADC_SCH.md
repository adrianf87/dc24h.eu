# ADC SCH — alpha-0.5.2

**Aktualizacja — alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) dodaje skompilowane filtry nazw/ścieżek Unicode bez rozróżniania wielkości liter, alternatywy rozszerzeń, domknięte/dokładne limity bajtów i typ plik/katalog dla sprawdzonych opisów. Zapytania z samymi filtrami i nieprzezroczyste tokeny NFC przechodzą do Search Core. Dopasowanie nie alokuje pamięci; routing/wyniki, indeksy i administracja wyszukiwaniem pozostają kolejnymi etapami.

Gałąź utworzono bezpośrednio z main `0fd969bebfe399388f17fc14632cc72cb3f6ee3d` po scaleniu Search Core. Kod i diagnostyka protokołu pozostają EN/US UTF-8; dokumentacja ma wersje US/PL. Etap dodaje typowane parsowanie i rzeczywisty odbiór; rozsyłanie, wyniki, limity/historia wyszukiwań i administracja pozostają osobnymi etapami.

## Parser i własność zapytania

`adc/sch.hpp` i `src/adc/sch.cpp` dodają `parse_sch(body)`, `validate_sch(message)` i niezmienny `SchRequest`. Pierwsza funkcja przyjmuje ciało bez LF i zwraca `variant<SchRequest, ParseError, SchError>`; błędy ramki, escape, UTF-8/NFC, nagłówka i ogólnych parametrów zachowują ParseError. Walidacja ręcznie zbudowanego Message zwraca `variant<SchRequest, SchError>` i sprawdza spójność przez ograniczoną wspólną serializację/ponowne parsowanie, także pól nieznanych. Żadna operacja nie nadaje uprawnienia do wysłania ani nie kieruje pakietu.

SchRequest ma własny sprawdzony Message, typowane kryteria, Selection Search Core i opcjonalny 24-bajtowy korzeń Tiger. `message()` zachowuje kolejność parametrów, powtórzenia pól wielokrotnych/nieznanych, zapis liczb i selektory; zwykła serializacja odtwarza je ze standardowym escape ADC. Referencje do wejścia nie przeżywają kopii/przeniesienia ani zmian wywołującego. Nie dodano osobnego buildera/serializatora SCH.

Profil huba obejmuje B/F/D/E, poprawny SID źródła, cel dokładnie dla D/E i niepuste selektory dokładnie dla F. H/I/C/U są poza tym profilem; nie dodano odbioru zapytań UDP. Ogólny parser zachowuje inne zakresy. Także lokalne obiekty podlegają kontroli command/kind/positional count, źródła/celu, nagłówków i limitów parametrów.

## Typowane pola i polityka aplikacji

Znaczenia odpowiadają [ADC SCH](https://adc.sourceforge.io/ADC.html): wielokrotne AN/NO i rozszerzenia EX; włączne granice GE/LE oraz dokładne EQ; TY 1 dla pliku i 2 dla katalogu; opcjonalny token TO. [TR rozszerzenia TIGR](https://adc.sourceforge.io/ADC-EXT.html) przenosi korzeń drzewa. Nieznane pola zachowywane są w oryginalnym Message i nie stają się kryteriami Core. Nie są wykonywane dopasowania, wildcard, odczyty plików, SQL ani obliczanie hashy.

| Pole | Reprezentacja i lokalna polityka |
| --- | --- |
| AN / NO | Własne odkodowane ciągi NFC UTF-8, po 1–256 bajtów, nie z samych spacji ASCII, łącznie do 32 terminów |
| EX | Własny tekst po 1–32 bajty, do 16 wartości, bez początkowej kropki, ukośnika, odwrotnego ukośnika i spacji ASCII |
| GE / LE / EQ | Opcjonalne nieujemne liczby całkowite ze znakiem 64-bit; bez przepełnienia/ułamków/plusa i sprzecznych granic; zero i INT64_MAX są dozwolone |
| TY | Opcjonalna wartość dziesiętna 1 lub 2; brak oznacza `ItemType::any`; alias `01` jest odczytywany liczbowo, a oryginalny zapis zachowany |
| TO | Opcjonalny niepusty odkodowany tekst NFC, 1–128 bajtów UTF-8; spacje i Unicode zachowane, kontrolki/formatowanie/separatory wierszy odrzucane |
| TR | Opcjonalny kanoniczny nieuzupełniony Base32 korzenia Tiger: 39 wielkich symboli i zerowe niewykorzystane bity; także zapytania hash-only |
| Selektory F | Łącznie do 32; pierwsze trzy znaki wielkie litery ASCII, ostatni wielka litera/cyfra; powtórzone i sprzeczne nazwy odrzucane |
| Payload | Do 4096 odkodowanych bajtów pełnych ciągów parametrów, w tym dwuznakowych nazw, nieznanych pól i powtórzeń |

GE, LE, EQ, TY, TO i TR są pojedyncze; ponowienie pola jest błędem także przy tej samej wartości. GE nie przekracza LE, a EQ mieści się w podanych granicach. Znany tekst korzysta ze ściślejszej polityki Text: bez kontrolek, formatowania i separatorów wierszy/akapitów; bez przycinania, zmiany wielkości liter i naprawy normalizacji. To limity aplikacji, a nie opis wszystkich poprawnych klientów ADC. Nadal obowiązuje ogólny limit 16384 bajtów zakodowanego ciała i 128 parametrów; bajty nagłówka selektorów są oddzielne od budżetu parametrów.

Dopuszczono same filtry, wykluczenia lub hash. Puste zapytania, same nieznane pola i sam token zwracają `SchError::no_criteria`; odbiornik je pomija bez odpowiedzi. To jawny profil ograniczony, który nie zamienia braku kryteriów w nieograniczone wyszukiwanie. Puste znane wartości, błędne liczby/hashe, powtórzenia pojedynczych pól i sprzeczne rozmiary dają typowane błędy semantyczne. Nieznane pola są sprawdzane składniowo, wliczane do budżetów, zachowane i nie są ujawniane w logach/API/UI. Ich semantyka nie jest zaimplementowana.

## Powiązanie z Search Core

`criteria()` i `selection()` udostępniają stałe dane. `to_core_query()` zwraca `variant<search::Query, search::QueryError, SchError>` i stosuje sprawdzony profil Search Core. Od alpha-0.5.3 akceptowane są same filtry i nieprzezroczyste tokeny NFC ze spacjami/Unicode, bez tworzenia ani przepisywania pól. `to_filter()` kompiluje te same kryteria do [Search Filters](SEARCH_FILTERS.md). Zapytanie z TR zwraca `unsupported_hash_query` w obu projekcjach, więc hash nigdy nie jest po cichu usuwany. Dane nie są przepisywane dla wymuszenia akceptacji. Metoda jest sprawdzaną projekcją, a nie planem dostarczenia ani uprawnieniem.

Biblioteka ADC łączy się teraz z dc24h_search bez zależności odwrotnej. Odbiornik sprawdza SCH, ale nie tworzy/nie wykonuje fanout Core i nie odkłada zapytań do późniejszego odtwarzania. Przyszła integracja musi uwzględnić granice projekcji i zachować odpowiednią semantykę rozszerzeń.

## Rzeczywisty odbiór ADC/ADCS

LoginHub obsługuje SCH po walidacji stanu/źródła ClientSession i gotowości końcowego powitania. Zamykanie nadal pomija wejście. Przed gotowością obowiązuje dotychczasowy fatalny błąd stanu; fałszywy SID daje fatalny błąd źródła również przy błędnych polach. Zakresy H/I/C/U nadal kontroluje istniejąca warstwa stanów.

| Wejście | Zachowanie |
| --- | --- |
| Poprawne SCH tego profilu | Odwracalne `ISTA 125 Search\srouting\snot\simplemented FC<FOURCC>` |
| Semantyczny błąd SCH | Odwracalne `ISTA 100 Invalid\ssearch\squery FC<FOURCC>` |
| Puste/same nieznane/sam token | Bez odpowiedzi i przekazania |
| Surowy błąd ogólnego parsera | Dotychczasowa ograniczona obsługa błędnych ramek przed odbiornikiem SCH |
| Błędne źródło/stan | Dotychczasowe fatalne `240` / `244` przed analizą pól |

Opis i FOURCC są stałe; nie cytują kryteriów, tokena, korzenia, nazwy pliku ani pól dowolnych. Odmowa zachowuje gotowość i uporządkowane kolejki; kolejne zapytania i czat mogą działać. Ani poprawne, ani błędne zapytanie nie trafia do innego klienta. RES pozostaje dotychczasowo nieobsługiwane. SCH nie zwiększa liczników czatu, nie dodaje pól API i nie ujawnia tekstu WebAdmin. Dane wyszukiwania są przejściowe, bez historii/bazy zapytań.

Błąd alokacji parsera/diagnostyki jest przechwytywany dla danego klienta i wymusza tylko jego natychmiastowe zamknięcie internal-error. Inne sesje i kolejki pozostają dostępne. Nadal obowiązują ogólne limity ramek/bezpieczeństwa/wyjścia. Nie dodano limitera wyszukiwań, potwierdzenia sukcesu, zegara, rejestru żądań ani korelacji wyników. Język operatora nie tłumaczy opisów protokołu.

## Weryfikacja i odtworzenie

Weryfikacja lokalna **alpha-0.5.2** z 2026-11-11: wszystkie **94/94 zestawy CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym typowane SCH, rzeczywisty fragmentowany odbiór ADC/ADCS, zapytania ncdc, MariaDB i Apache HTTPS. Końcowe kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły budowanie produkcyjne/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, lokalne linki, przykładowy INI i białe znaki. Wyniki są lokalne; zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne są niezweryfikowane.

`adc.sch` sprawdza typowane pola BASE, Unicode/spacje/escape, aliasy/granice liczb, kanoniczne TR, nieznane pola, własność kopii/przeniesień, odmowy projekcji Core, duplikaty, dokładne limity bajtów/parametrów/terminów/tokena/selektorów, rozdzielenie błędów składni/semantyki, wrogie obiekty i rollback/izolację błędów alokacji. Kampania alokacji C++ nie deklaruje dowolnych awarii alokatora C/biblioteki Unicode.

`adc.sch.process` używa rzeczywistych ADC/ADCS, fragmentacji i uporządkowanych barier INF do sprawdzenia rodzin pól, błędów, ciszy bez kryteriów, odzyskania po błędzie składni, braku fanout, pierwszeństwa źródła/stanu i prywatności API/logów/tokenów. `search.core.process` dostosowano do nowej odpowiedzi. Rzeczywisty ncdc ADC/ADCS `/search` potwierdza `Search routing not implemented`, co dowodzi poprawnego parsowania odbieranego SCH, a nie gotowej wymiany wyników.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug -R 'adc.sch|search.core|adc.ncdc|adcs.ncdc' --output-on-failure -j 4
ctest --test-dir build-debug --output-on-failure -j 4
```

Zobacz [Search Core](SEARCH_CORE.md), [TESTING.md](TESTING.md) i [CHANGELOG.md](../../CHANGELOG.md). Pełne rozsyłanie/wyniki, zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne wymagają osobnej pracy i dowodów.
