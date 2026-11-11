# TTH Search — alpha-0.5.4

Etap oparty bezpośrednio na main `edb702ada4fea04d7854a18370a052fc96035ea7` po scaleniu alpha-0.5.3. Zachowano wcześniejsze Search Core, SCH i filtry. Kod i komunikaty pozostają EN/US UTF-8, z dokumentacją US/PL.

## Dostarczone zachowanie

`search::TthRoot` w `search/tth.hpp` jest stałą 24-bajtową wartością publicznego hasha pliku bez zależności od ADC, gniazd i dostawcy kryptografii. `QueryData::tth` i `ItemData::tth` są opcjonalnymi hashami. Każda wartość 192-bitowa jest reprezentowalna; brak hasha różni się od samych zer. Hash jest metadanymi od wywołującego, nie dowodem istnienia pliku ani posiadania jego treści.

`Query::create()` akceptuje TTH jako jedyne kryterium, bez tworzenia dodatniego terminu nazwy. Przy zapytaniach łączonych nadal sprawdza tekst, rozszerzenia, rozmiary, typ i token. Sam hash ma stałe 24 bajty; istniejący 4096-bajtowy limit sumy tekstu, liczby pól i długości nazwy pozostają. Sam hash nie pozwala na błędne ani zbyt duże pola dodatkowe.

`Filter::create()` zachowuje hash w niezmiennym skompilowanym stanie. `matches(item)` wymaga dokładnej równości wszystkich 192 bitów i opisu pliku ze znanym hashem. Brak lub inny hash oznacza brak dopasowania; katalogi nie spełniają zapytania TTH. `Item::create()` odrzuca hash pliku przypisany do katalogu przez `ItemError::invalid_hash`, przed przetwarzaniem tekstu. Plik może nie mieć hasha przy zwykłym dopasowaniu nazw/filtrów.

Dopasowanie samego hasha jest niezależne od poprawnej nazwy/rozszerzenia i znanego/nieznanego rozmiaru opisu pliku. Dodatkowe AN/NO/EX/GE/LE/EQ/TY pozostają warunkami AND: równość hasha nie pomija żadnego z nich. TR połączone z TY katalog jest akceptowane jako predykat niemożliwy do spełnienia, zgodnie z wcześniejszą jawną polityką EX/TY katalog. Nie jest po cichu zmieniane na zapytanie o plik. TO nadal stanowi nieprzezroczyste metadane korelacji i nie wpływa na dopasowanie.

Dopasowanie nie alokuje, nie wykonuje I/O, normalizacji ani obliczania hashy i jest noexcept. Tworzenie zachowuje wcześniejsze limity Unicode i kontrakt awarii alokacji. Query, Item, Filter i plany Core posiadają/kopiują hashe; zmiana pamięci wywołującego nie zmienia przygotowanego zapytania, opisu ani dispatch. Core nadal sprawdza gotowość nadawcy/źródło, życie sesji, domenę routingu i selektory funkcji. TTH nie jest nowym mechanizmem tożsamości/uwierzytelniania.

## Reprezentacja ADC i projekcja

Oficjalne [rozszerzenie TIGR](https://adc.sourceforge.io/ADC-EXT.html) definiuje TR jako hash drzewa Tiger zapisany w Base32. Etap korzysta z istniejącego sprawdzonego kodeka Tiger Base32 ADC: dokładnie 39 wielkich znaków A–Z/2–7 bez dopełnienia i z zerowymi nieużywanymi bitami końcowymi. Małe litery, dopełnienie, błędne długości/znaki, prefiksy `TTH/` i teksty magnet/URN nie są po cichu poprawiane. To istniejący kanoniczny profil aplikacji; nie dodano drugiego kodeka, zależności ani dostawcy hashy.

`parse_sch()` / `validate_sch()` umieszczają zdekodowane TR bezpośrednio w `criteria().tth`. `SchRequest::root()` czyta tę samą pamięć, eliminując osobne kopie typowanego hasha. `to_core_query()` i `to_filter()` przekazują teraz sam hash i zapytania łączone bez utraty TR. Stare `SchError::unsupported_hash_query` pozostaje dla zgodności kodu, ale nie jest już zwracane. Typy zwracane przez projekcje pozostają bez zmian.

Oryginalny Message zachowuje sprawdzone pola, kolejność, zapis liczb i nieznane parametry dla istniejącej serializacji. Powtórzone TR jest odrzucane także przy identycznej wartości. Nadal obowiązują wszystkie limity parametrów/ramki/zdekodowanych bajtów, Unicode, selektory, reguły skalarów i stanu/źródła. Parser przewodowy pozostaje w ADC; Search otrzymuje tylko binarny hash i zachowuje niezależność od protokołu.

| Wejście | Wynik |
| --- | --- |
| `BSCH AAAB TR<kanoniczny-hash> TOtoken` | Własne dane zapytania z samym hashem; Core i Filter zachowują hash/token |
| Równy hash pliku i spełnione dodatkowe pola | Dopasowanie predykatu |
| Brak/inny hash pliku albo katalog | Brak dopasowania |
| Równy hash i niespełniony filtr nazwy/rozszerzenia/rozmiaru/typu | Brak dopasowania |
| Błędne lub powtórzone TR | Dotychczasowy błąd semantyczny `invalid_hash` / `duplicate_field` |

## Granica rzeczywistego transportu

Rzeczywisty odbiór ADC/ADCS nadal sprawdza stan/źródło/gotowość przed parserem SCH. Poprawne zapytanie TTH nadal otrzymuje odwracalne `125 Search routing not implemented`; błędna semantyka otrzymuje `100 Invalid search query`. Etap implementuje kryteria TTH, projekcję i dopasowanie dostarczanych opisów. Nie przekazuje SCH, nie przyjmuje/koreluje RES, nie wysyła odpowiedzi UDP ani nie publikuje wyników. Testy surowego TCP/TLS i rzeczywisty ncdc potwierdzają sparsowany odbiór TTH i tę jawną granicę, nie zakończoną wymianę zdalnych wyników.

Nie dodano indeksu plików huba, przeglądania zasobów, obliczania drzewa Tiger, weryfikacji treści pliku, rejestru zapytań, limitów/historii wyszukiwania ani konfiguracji/API/UI. Istniejące `adc::tiger192` hashuje ciąg bajtów dla kodu związanego z tożsamością; nie wolno zastępować nim hasha drzewa Merkle całego pliku. Testy odróżniają TTH pustego pliku (Tiger bajtu prefiksu liścia 0x00) od Tiger pustego ciągu bajtów; nie deklaruje się ogólnego generatora TTH.

Hashe, terminy, nieprzezroczyste tokeny i nazwy pozostają tymczasowe i nie trafiają do diagnostyki, logów, statystyk czatu, bazy, API ani WebAdmin. Nie ma migracji SQL ani nowego komunikatu operatora; zachowano istniejące `languages/us` i `languages/pl`. Metadane serwera i WebAdmin przechodzą razem na alpha-0.5.4.

## Weryfikacja

Weryfikacja lokalna **alpha-0.5.4** z 2026-11-11: wszystkie **98/98 zestawów CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym ncdc ADC/ADCS z samym hashem, wcześniejsze regresje filtrów/czatu/logowania, MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń. Przeszły budowanie produkcyjne/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych/linki dokumentacji, składnię zmienionych skryptów Python, przykładowy INI i białe znaki. Wyniki są lokalne; zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.

`search.tth` obejmuje wektor pustego pliku, każdą zmianę pojedynczego bitu w 192 bitach, zero/brak/same jedynki, zmianę nazwy i brak rozmiaru, katalogi, połączone kryteria, dokładne limity tekstu, własność/kopie/przeniesienia, round trip B/F/D/E SCH, odmowę niekanonicznych danych, duplikaty, plany/dispatch Core związane ze źródłem, rollback awarii alokacji C++ oraz dopasowanie/dispatch bez alokacji. Pozostają aktywne istniejące testy `search.filters`, `search.core`, `adc.sch` i kodeka tożsamości. Wstrzykiwanie awarii nie deklaruje dowolnych błędów alokatora C/dostawcy kryptografii.

`adc.sch.process` zawiera zapytania z samym kanonicznym hashem i łączone, małe litery/dopełnienie/duplikaty/niekanoniczne hashe oraz granice prywatności/powrotu po błędzie/braku fanout przez rzeczywiste ADC/ADCS. Nowe `adc.tth.ncdc` i `adcs.tth.ncdc` używają istniejącego pełnego testu logowania/czatu/reconnect z `/search -hub -tth LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ`. Przechwycenie tekstu sprawdza SCH z TR bez sztucznych AN/EX i stałą odpowiedź; TLS zachowuje hash zaszyfrowany. Istniejące wyszukiwania ncdc według nazw/rozszerzeń/rozmiarów pozostają osobnymi testami.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug -R 'search\.|adc.sch|tth.ncdc' --output-on-failure -j 4
ctest --test-dir build-debug --output-on-failure -j 4
```

Powtórz Release i ASan/UBSan zgodnie z [TESTING.md](TESTING.md). Zobacz [Search Core](SEARCH_CORE.md), [ADC SCH](ADC_SCH.md), [Search Filters](SEARCH_FILTERS.md), [ROADMAP.md](ROADMAP.md) i [CHANGELOG.md](../../CHANGELOG.md). Numerowana gałąź zachowuje historię i nie scala się automatycznie do main. Rzeczywisty routing/wyniki i walidacja obciążenia produkcyjnego/pełnej macierzy wymagają dalszych etapów.
