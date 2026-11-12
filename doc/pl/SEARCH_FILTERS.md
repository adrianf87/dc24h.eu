# Search Filters — alpha-0.5.3

**Aktualizacja — alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) dodaje ograniczony rejestr życia sesji/TO, lokalne generacje wyszukiwań, terminy ważności, anulowanie i jawne API aktywnych wyszukiwań Core. Nadal można przygotować plan bez rejestracji; odrzucone rzeczywiste SCH nie rezerwuje tokenów. Korelacja/routing wyników i administracja wyszukiwaniem pozostają kolejnymi etapami.

**Aktualizacja — alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) przekazuje pełny 192-bitowy hash pliku z SCH do niezmiennych zapytań/planów Core, opisów i filtrów. Zapytania z samym hashem lub łączone używają dokładnej równości i zachowują dodatkowe kryteria. Testowane są rzeczywiste zapytania TTH ncdc ADC/ADCS; routing zapytań/wyników pozostaje kolejnym etapem.

Etap oparty bezpośrednio na main `dd2a968469c3db21ada4e469afc85b7a3415eeab` po scaleniu alpha-0.5.2. Zachowano wcześniejsze Search Core i ADC SCH. Kod i komunikaty pozostają EN/US UTF-8; dokumentacja ma wersje US/PL.

## Zakres i API

`dc24h_search` zawiera teraz `search/filters.hpp` i `src/search/filters.cpp`. Udostępnia predykaty dla sprawdzonych opisów dostarczanych przez wywołującego. Hub nie ma indeksu plików, nie przegląda katalogów ani zasobów klientów. Moduł przygotowuje reguły nazw, rozszerzeń, rozmiarów i typów dla przyszłych adapterów zapytań/wyników; nie implementuje fanout SCH ani obsługi RES.

- `Query::create(QueryData)` sprawdza kryteria i zachowuje ich niezmienioną kopię.
- `Filter::create(QueryData)` zwraca `variant<Filter, QueryError>` i kompiluje wewnętrzne napisy porównawcze Unicode.
- `Item::create(ItemData)` zwraca `variant<Item, ItemError>` i przechowuje sprawdzony opis z napisem porównawczym.
- `filter.matches(item)` zwraca bool bez alokacji, wyjątków, I/O, logowania i mutacji. Żaden argument nie zachowuje drugiego.
- `SchRequest::to_filter()` zwraca `variant<Filter, QueryError, SchError>`; `to_core_query()` przekazuje do Core te same obsługiwane kryteria tekstowe/filtrów.

Poprawne wartości mają prywatne konstruktory. Kopie Filter współdzielą niezmienny skompilowany stan; Item posiada własne napisy i metadane. Zmiana/usunięcie wejścia nie zmienia żadnego obiektu. Przeniesienie Filter przekazuje jego stan; obiekt źródłowy zwraca false. Tworzenie Item i kompilacja mogą przekazać błąd alokacji bez częściowej publikacji i modyfikacji wejścia. Koordynacją nadal rządzą istniejące kontrole nadawcy, życia sesji, domeny i funkcji w Core; dopasowanie opisu nie nadaje uprawnień i nie potwierdza zawartości zdalnego klienta.

## Reguły dopasowania

[Specyfikacja ADC, SCH i nazwy plików](https://adc.sourceforge.io/ADC.html) definiuje porównanie pełnej nazwy bez rozróżniania wielkości liter, wszystkie AN, wykluczenie przez dowolny NO, alternatywy EX, operatory bajtowe i typ plik/katalog. Implementacja przyjmuje następującą jawną lokalną politykę porównań i opisów:

| Kryterium | Zachowanie |
| --- | --- |
| AN (`include`) | Każdy termin musi wystąpić jako dosłowny fragment nazwy wraz z wirtualną ścieżką zasobu |
| NO (`exclude`) | Dopasowanie dowolnego terminu w tej nazwie wyklucza element |
| EX (`extensions`) | Plik musi kończyć się co najmniej jednym rozszerzeniem poprzedzonym kropką w ostatnim członie oddzielonym `/`; katalogi i człony nadrzędne nie spełniają EX |
| GE / LE | Domknięte dolne/górne granice rozmiaru w bajtach |
| EQ | Dokładna liczba bajtów przecinana z GE/LE |
| TY | Plik, katalog albo dowolny przy braku; konkretny opis nie może mieć typu `any` |
| TO | Nieprzezroczyste metadane korelacji; nigdy warunek dopasowania |

Nazwy/rozszerzenia używają istniejącej biblioteki utf8proc: stabilnego pełnego Unicode case folding oraz kompozycji NFC na kopiach wewnętrznych. `ŻÓŁW` pasuje do `żółw`, `STRASSE` do `Straße`, a postacie greckiej sigmy porównują się tak samo. Akcenty pozostają: `zolw` nie pasuje do `żółw`. Oryginalne kryteria, token i zapis opisu nie są zmieniane. Nie ma konwersji zależnej od locale, przycinania spacji, dzielenia terminów ani wykonywania wildcard/regex. Spacje, interpunkcja, `*` i `?` są dosłowne.

Wartości EX nie zawierają początkowej kropki. Dopasowanie wymaga niepustej podstawy nazwy, granicy kropki i całego sufiksu: `iso` pasuje do `disk.ISO`, ale nie do `notiso`, `disk.iso.bak`, `/dir.iso/file` ani samej ukrytej nazwy `.iso`. Obsługiwane są dosłowne sufiksy złożone, np. `tar.gz`; nie przechodzą przez `/`. Wiele EX to alternatywy OR łączone z pozostałymi kryteriami przez AND. EX z TY katalog jest poprawnym predykatem niepasującym do żadnego opisu. Reguły granic/złożonych sufiksów/ukrytych nazw są lokalną polityką, a nie dodatkowymi polami ADC czy domyślnymi grupami rozszerzeń.

`ItemData` zawiera zdekodowaną wirtualną `name` ze ścieżką zasobu, konkretny ItemType oraz opcjonalne nieujemne 64-bitowe `bytes`. Nazwa ma 1–4096 bajtów UTF-8, nie składa się wyłącznie ze spacji ASCII i podlega istniejącej polityce Text gotowego NFC (bez znaków sterujących/formatujących oraz separatorów wierszy/akapitów). Nazwa pliku nie może kończyć się `/`. Nazwa katalogu może zawierać końcowy `/`. To sprawdzenie opisu, nie pełny parser listy plików/RES ADC ani walidator ścieżek systemu operacyjnego. Moduł nigdy nie otwiera nazwy, nie rozwiązuje przejść katalogów ani dowiązań.

Rozmiar zero jest rzeczywistą wartością; nieznany to `nullopt`. Każdy filtr rozmiaru odrzuca nieznany rozmiar, także katalogu. Podany rozmiar katalogu jest porównywany jak pliku; nie oblicza się ani nie zgaduje rozmiaru rekurencyjnego. Bez kryteriów rozmiaru brak wartości jest dopuszczalny. Zero i INT64_MAX są porównywane bez odejmowania i przepełnień. Błędne/ujemne rozmiary i sfałszowane typy są odrzucane przed pracą z tekstem.

## Integracja Query i SCH

Wymagane jest co najmniej jedno rozpoznane kryterium: AN, NO, EX, GE/LE/EQ, konkretny TY albo (od alpha-0.5.4) hash TTH. Dodatnie AN są teraz opcjonalne, więc same wykluczenia, rozszerzenia, rozmiary lub typ przechodzą do Core bez tworzenia sztucznych terminów. Puste QueryData i sam token zwracają `QueryError::no_criteria` zamiast starego `missing_term`. Dopuszcza to szerokie jawne kryteria, np. GE0/TY1; limity tempa i przyjęcia routingu pozostają przyszłym etapem, nie właściwością dopasowania.

Zachowano limity: 32 łączne AN/NO, 256 bajtów terminu, 16 rozszerzeń do 32 bajtów, 128 bajtów tokena i 4096 łącznych bajtów tekstu. Walidacja rozszerzeń spójnie odrzuca początkową kropkę w SCH i Query. Tokeny przyjmują nieprzezroczysty gotowy tekst NFC ze spacjami/Unicode zgodnie z SCH; błędny tekst jest odrzucany, a sam TO nadal nie stanowi kryterium. Limity obowiązują przed normalizacją/kopiowaniem. Wewnętrzne napisy po case folding mogą się wydłużyć, ale pochodzą tylko z ograniczonych wejść. Pracę dopasowania ograniczają limity nazwy, terminów i rozszerzeń; kompilacja/przygotowanie opisu alokują, powtarzane dopasowanie nie.

Od alpha-0.5.4 zapytania z TR przekazują pełny hash do Core i Filter, także przy połączeniu TR z tekstem/filtrami. Sam hash wymaga dokładnej równości hasha pliku; pozostałe filtry są dodatkowymi warunkami AND. Zobacz [TTH Search](TTH_SEARCH.md). Hash Tiger nie może zniknąć ani stać się filtrem tekstowym. Sprawdzone pola SCH, kolejność, nieznane parametry i zapis liczb pozostają w oryginalnym Message. Nieznane pola zachowują politykę ADC ignorowania i zachowania; moduł nie interpretuje ani nie ogłasza grup ASCH/SEGA.

Rzeczywisty odbiór ADC/ADCS zachowuje zachowanie alpha-0.5.2: stan/źródło/gotowość są sprawdzane przed SCH; poprawne zapytania otrzymują odwracalne `125 Search routing not implemented`, błędy semantyczne `100 Invalid search query`, a brak kryteriów jest pomijany. Odbiornik nie uruchamia dopasowania opisów i nie tworzy fanout. Treść zapytań/tokenów/nazw plików nie trafia do diagnostyki, liczników czatu, logów, bazy, API ani WebAdmin. Nie ma nowej migracji SQL, ustawienia, zależności ani kontrolki UI; metadane wydania przechodzą na alpha-0.5.3.

## Weryfikacja

Weryfikacja lokalna **alpha-0.5.3** z 2026-11-11: wszystkie **95/95 zestawów CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym rzeczywisty ncdc ADC/ADCS, MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń. Przeszły budowanie produkcyjne/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych/linki dokumentacji, składnię zmienionych skryptów Python, przykładowy INI i białe znaki. Wyniki są lokalne; nie potwierdzają zdalnego CI, pełnej macierzy systemów/klientów ani obciążenia produkcyjnego.

`search.filters` sprawdza AN/NO w pełnej ścieżce, ekspansję Unicode/sigmę/akcenty, dosłowne spacje/interpunkcję, OR/granice kropek/członów/złożonych sufiksów EX, izolację katalogów, rozmiary domknięte/dokładne/zero/INT64_MAX, brak rozmiaru, konkretne typy, błędne teksty/typy/rozmiary, dokładne limity, własność kopii/przeniesień, projekcję SCH, przygotowanie samych filtrów w Core, rollback błędów alokacji C++ i dopasowanie bez alokacji. Nie symuluje dowolnych awarii alokatora C wewnątrz utf8proc.

`adc.sch.process` rozszerza rzeczywiste fragmentowane B/F/D/E SCH przez ADC/ADCS o każdą rodzinę samodzielnych filtrów i rozszerzenia/tokeny Unicode, zachowując powrót po błędzie, brak fanout, pierwszeństwo stanu/źródła i prywatność API/logów. Rzeczywisty ncdc ADC/ADCS wykonuje `/search -hub -ge 1K -t archive SearchCoreSecret`; przechwycony tekst ADC potwierdza GE1024 i EX, a TLS pozostaje zaszyfrowany. Obie ścieżki potwierdzają parsowanie odbioru i jawną granicę routingu, nie wymianę wyników. ncdc używa jednego warunku rozmiaru na zapytanie; łączne GE/LE/EQ są testowane bezpośrednio i przez surowe ADC/ADCS.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure -j 4
```

Powtórz dla Release i ASan/UBSan zgodnie z [TESTING.md](TESTING.md). Zobacz [Search Core](SEARCH_CORE.md), [ADC SCH](ADC_SCH.md), [ROADMAP.md](ROADMAP.md) i [CHANGELOG.md](../../CHANGELOG.md). Routing zapytań/wyników, indeks plików, limity/historia wyszukiwania, zdalne CI, pełna macierz systemów i obciążenie produkcyjne wymagają osobnej pracy i dowodów.
