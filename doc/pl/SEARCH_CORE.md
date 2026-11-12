# Search Core — alpha-0.5.1

**Bieżący etap — alpha-0.5.7:** [Feature Routing](FEATURE_ROUTING.md) kompiluje selektory FSCH raz i korzysta z aktualnego publicznego INF SU: wszystkie wymagane, żadna wykluczająca. Negocjacja SUP jest niezależna. Zmiany możliwości, udział źródła, nieznane rewizje i limity sprawdzono przez ADC/ADCS i rzeczywisty ncdc. RES/routing wyników pozostaje kolejnym etapem.

**Aktualizacja — alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) przekazuje sprawdzone SCH przez rzeczywiste ADC/ADCS: broadcast B/F i D/E związane z życiem celu współdzielą jeden niezmienny pakiet z oddzielnymi ograniczonymi kolejkami. Podane TO jest rejestrowane w Search Core; brak TO nadal jest poprawny. Obowiązują kontrole źródła/stanu/funkcji, jednokrotny dispatch i sprzątanie tokenów. RES/routing wyników pozostaje kolejnym etapem.

**Aktualizacja — alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) dodaje ograniczony rejestr życia sesji/TO, lokalne generacje wyszukiwań, terminy ważności, anulowanie i jawne API aktywnych wyszukiwań Core. Nadal można przygotować plan bez rejestracji; odrzucone rzeczywiste SCH nie rezerwuje tokenów. Korelacja/routing wyników i administracja wyszukiwaniem pozostają kolejnymi etapami.

**Aktualizacja — alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) przekazuje pełny 192-bitowy hash pliku z SCH do niezmiennych zapytań/planów Core, opisów i filtrów. Zapytania z samym hashem lub łączone używają dokładnej równości i zachowują dodatkowe kryteria. Testowane są rzeczywiste zapytania TTH ncdc ADC/ADCS; routing zapytań/wyników pozostaje kolejnym etapem.

**Aktualizacja — alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) dodaje skompilowane filtry nazw/ścieżek Unicode bez rozróżniania wielkości liter, alternatywy rozszerzeń, domknięte/dokładne limity bajtów i typ plik/katalog dla sprawdzonych opisów. Zapytania z samymi filtrami i nieprzezroczyste tokeny NFC przechodzą do Search Core. Dopasowanie nie alokuje pamięci; routing/wyniki, indeksy i administracja wyszukiwaniem pozostają kolejnymi etapami.

**Kolejny etap — alpha-0.5.2:** [ADC SCH](ADC_SCH.md) parsuje teraz odbierane zapytania i sprawdza ich pola. Bieżąca odpowiedź to `125 Search routing not implemented`; routing i wymiana wyników są osobnymi etapami. Opis implementacji i weryfikacji alpha-0.5.1 poniżej zachowuje historyczny zakres.

Gałąź utworzono bezpośrednio z main `c6b34ea849ef808a0bfee4a27e0c8ae38cff4a6b` po scaleniu alpha-0.4.12 WebAdmin. Kod, typowane błędy i diagnostyka testów pozostają EN/US UTF-8; dokumentacja ma wersje US i PL.

## Dostarczony zakres

`dc24h_search` jest osobną biblioteką C++20 w `include/dc24h/search/` i `src/search/`, budowaną przez CMake z istniejącymi modułami Users i Text. Zapewnia sprawdzany model zapytania tekstowego i silnik koordynacji po stronie huba: waliduje gotowego nadawcę, wybiera gotowe ID życia sesji, przygotowuje własny niezmienny plan i próbuje przekazania do kolejek przez zaufany sink wywołującego. Nie przechowuje indeksu plików huba i nie przegląda lokalnych/udostępnionych plików. Wyszukiwanie własnych udostępnień należy do klientów.

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html) rozdziela zapytania SCH, wyniki RES i routing według funkcji. Ten etap dostarcza podległy model ogólny, nie kodek ADC ani sieciowy handler wyszukiwania. `QueryData` jest wewnętrznym kontraktem C++; opisane limity i walidacja są polityką aplikacji, a nie opisem wszystkich poprawnych zapytań ADC. Wartości enum nie są wartościami liczbowymi protokołu.

Od alpha-0.5.6 produkcyjny LoginHub używa [Search Routing](SEARCH_ROUTING.md) i Search Core do przekazywania poprawnych SCH po kontroli źródła/stanu/gotowości. Podane TO używa przyjęcia aktywnego wyszukiwania; brak TO używa przygotowania bez rejestracji. Obsługiwane są B/F i D/E związane z życiem celu. Poprawny routing nie wysyła potwierdzenia sukcesu/odmowy; błędy semantyczne, duplikaty/limity i brak celu dostają stałe komunikaty. RES pozostaje nieobsługiwane. Korelacja wyników, odpowiedzi UDP, osobny limiter/historia i administracja wyszukiwaniem są kolejnymi etapami.

## Niezmienne kryteria

`Query::create(const QueryData&)` zwraca `variant<Query, QueryError>`. Sukces tworzy niezmienioną własną kopię; zmiana lub zniszczenie wejścia jej nie modyfikuje. `Query::data()` udostępnia wyłącznie stałą referencję. Model nie przycina, nie naprawia normalizacji, nie zmienia wielkości liter, nie wykonuje wildcard/regex, dopasowań plików ani SQL. Terminy, kolejność/powtórzenia rozszerzeń i opcjonalny token są zachowywane dosłownie dla przyszłego adaptera/klienta. Błąd to typowane enum bez cytowania wejścia. Błędy alokacji są przekazywane do wywołującego.

| Wejście | Reguła aplikacji |
| --- | --- |
| `include` | Opcjonalne od alpha-0.5.3; łącznie wymagane co najmniej jedno kryterium nazwy/rozszerzenia/rozmiaru/typu/TTH |
| `include` + `exclude` | Łącznie do 32 terminów, każdy niepusty, nie z samych spacji ASCII i do 256 bajtów UTF-8 |
| Tekst terminów | Wymagane gotowe NFC UTF-8; Text odrzuca znaki sterujące/formatujące i separatory wierszy/akapitów; zachowuje spacje wewnętrzne i brzegowe |
| `extensions` | Do 16 niepustych wartości, każda do 32 bajtów; ta sama walidacja tekstu, bez początkowej kropki, ukośnika, odwrotnego ukośnika i spacji ASCII |
| `min_bytes`, `max_bytes`, `exact_bytes` | Opcjonalne nieujemne liczby całkowite ze znakiem 64-bit; minimum nie przekracza maksimum, exact mieści się w podanych granicach; zero i INT64_MAX są poprawne |
| `type` | Sprawdzany enum `ItemType::any`, `file` lub `directory` |
| `token` | Opcjonalny; jeśli podano, 1–128 bajtów gotowego NFC UTF-8 ze spacjami/Unicode, bez interpretacji (alpha-0.5.3) |
| Łączny tekst wejścia | Do 4096 bajtów terminów, rozszerzeń i tokena |

Liczby pól, rozmiary poszczególnych wartości, liczby/enum i suma bajtów są sprawdzane przed pracą UTF-8/NFC i kopiowaniem. Sumowanie jest ograniczone wcześniejszymi limitami. Wejście ma już własną pamięć; przyszły dekoder musi osobno ograniczać ramki/parametry przed zbudowaniem wejścia. Od alpha-0.5.3 include może być puste przy obecności innego rozpoznanego kryterium; brak kryteriów i sam token zwracają `no_criteria`. Od alpha-0.5.4 opcjonalny stały 24-bajtowy hash `tth` także stanowi kryterium, w tym przy samym hashu; jest zachowany w kopiach i dispatch planu bez zależności od protokołu. `QueryError` rozróżnia brak kryteriów/nadmiar terminów, błędny tekst/rozszerzenie/rozmiar/typ/token i przekroczenie sumy bajtów. Publiczny konstruktor nie pozwala pominąć sprawdzenia danych.

## Wybór sesji i własność planu

`search::Core` pożycza stały SessionManager, który musi żyć dłużej od silnika i jego wywołań; cała praca pozostaje w wątku właściciela/reaktora. `prepare(sender, source, data, selection)` sprawdza najpierw istnienie lokalnego ID życia, gotowość z tożsamością i własność dokładnego klucza źródła. Sesje oczekujące na końcowe powitanie oraz zamykane nie wyszukują. Błąd źródła/stanu poprzedza kopiowanie i fanout. Wynik to `variant<Plan, Error, QueryError>`.

Opcjonalny `Selection` zawiera wymagane i wykluczone funkcje. Łącznie dopuszcza do 32 selektorów. Nazwa ma cztery znaki: pierwsze trzy wielkie litery ASCII, ostatni wielka litera lub cyfra. Powtórzenia i sprzeczne wymaganie/wykluczenie są odrzucane zamiast zmieniane. Bieżący publiczny Profile musi zawierać wszystkie wymagane funkcje i żadnej wykluczonej. To wybór możliwości, nie uwierzytelnienie/uprawnienie ani dowód osiągalności sieciowej. Wybór nie ufa deklaracjom udziału, liczby plików, slotów, ról ani IP.

Bez selektorów wybierani są wszyscy gotowi użytkownicy, także nadawca. ID rosną; zbieranie używa ograniczonych stron SessionManager po 256 pozycji i może objąć maksymalnie 65535 sesji rejestru. Z selektorami nadawca jest uwzględniany tylko po ich spełnieniu. Poprawny plan z zerową liczbą odbiorców jest dozwolony. Sesje pending/negotiating/identified/closing/closed są pomijane.

Plan ma własny sprawdzony Query, źródło, Selection i migawkę ID życia odbiorców; nie kopiuje profili, nicków, CID, PID, IP ani sekretów. Późniejsze dołączenia nie trafiają do planu. Usuniętego odbiorcy nie zastępuje nowe połączenie z ponownie użytym SID. Przechowywany nieprzezroczysty token domeny wiąże plan z konkretnym Core, także po zniszczeniu i utworzeniu nowego silnika pod tym samym adresem pamięci. Kopia planu zachowuje tę własność. Przygotowanie nie zmienia rejestru/kolejek, a błąd alokacji nie publikuje częściowego planu.

## Kontrakt dispatch

`dispatch(plan, sink)` sprawdza domenę, aktualną tożsamość/gotowość nadawcy i niepusty sink przed próbami odbiorców. Przed każdym sink ponownie sprawdza nadawcę, ID życia, gotowość i funkcje odbiorcy. Utrata funkcji oznacza niedostępność; uzyskanie funkcji nie dodaje ID nieobecnego w planie. Sama zmiana nicka nie unieważnia stabilnego SID.

| Pole raportu | Znaczenie |
| --- | --- |
| `queued` | Sink zwrócił true; przyjęcie przez kolejkę wywołującego, nie dostarczenie/odczyt |
| `unavailable` | Brak ID życia, brak gotowości lub niepasujące bieżące selektory |
| `refused` | Sink zwrócił false; ograniczenia i izolacja kolejki należą do wywołującego |
| `canceled` | Pozostałe ID pominięte po utracie własności/gotowości nadawcy w trakcie dispatch |

Suma raportu równa się liczbie odbiorców planu. Początkowe błędy źródła/domeny/sink zwracają `Error` bez wywołania sink. Zamknięcie nadawcy w sink przerywa pozostałe rozsyłanie i raportuje anulowanie; błąd pojedynczego odbiorcy nie blokuje pozostałych. Sam dispatch nie alokuje po zbudowaniu Sink; silnik nie czyta zegarów/plików/gniazd/loggera/bazy.

Sink jest zaufany, synchroniczny, nie może rekurencyjnie wywoływać dispatch i nie powinien rzucać wyjątków. Może zamykać/usuwać sesje; kolejna iteracja sprawdza życie ponownie. Rzucony wyjątek wychodzi po możliwym częściowym przyjęciu, bez ponowienia. Plan nie zapewnia ochrony przed powtórzeniem ani trwałości: dopuszczanie, limity tempa, jednorazowe planowanie, serializacja, limity bajtów kolejek i ich wyjątki należą do przyszłego adaptera. Core nie przechowuje zmiennego iteratora przez callback.

## Weryfikacja i kolejne etapy

Weryfikacja lokalna **alpha-0.5.1** z 2026-11-11: wszystkie **92/92 zestawy CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym Search Core, mieszane granice etapu ADC/ADCS, rzeczywiste zapytania ncdc, tymczasowa MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły budowanie produkcyjne/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, lokalne linki, przykładowy INI i białe znaki. Podczas testów poprawiono harness ncdc, uwzględniając szyfrowany ruch proxy i nawigację klawiaturą w karcie wyników. Wyniki są lokalne; nie potwierdzają zdalnego CI, pełnej macierzy systemów/klientów ani obciążenia produkcyjnego.

`search.core` sprawdza Unicode/NFC/kontrolki, limity tekstów/liczb/tokena/rozmiarów i dokładne granice bajtów, brak naprawy danych, typowane błędy, pierwszeństwo stanu/źródła, własność danych, wiele stron odbiorców, pusty plan, selektory/aktualizacje funkcji, późniejsze dołączenia, ponowne SID, kopie/obce domeny/ponowny adres Core, brak sink, anulowanie przy zamknięciu nadawcy i izolację odmów. Kampania alokacji C++ sprawdza brak zmian po błędzie i dispatch bez alokacji; nie symuluje dowolnych awarii alokatora C w normalizacji Unicode. Wyjątki sink są przekazywane bez ponowień.

`search.core.process` sprawdza rzeczywiste mieszane sesje ADC/ADCS: odmowę SCH/RES, brak przekazania przez uporządkowaną barierę INF, odmowę przed gotowością/ze złym źródłem, działający czat i pominięcie treści/tokenów w API/logach. Rzeczywisty ncdc ADC/ADCS wysyła `/search -hub SearchCoreSecret`, otrzymuje dotychczasową odmowę i kończy normalnie. To test granicy etapu, nie dowód gotowego wyszukiwania i wyników end-to-end.

Kolejne etapy mogą dodać sprawdzany parser/serializację SCH, wspólne niezmienne ramki, politykę transportu/zakresów, admission i walidację/korelację wyników. Nie oznaczono ich tu jako ukończonych. Zobacz [TESTING.md](TESTING.md), [ROADMAP.md](ROADMAP.md) i [CHANGELOG.md](../../CHANGELOG.md).

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug -R 'search.core|adc.ncdc|adcs.ncdc' --output-on-failure -j 4
ctest --test-dir build-debug --output-on-failure -j 4
```
