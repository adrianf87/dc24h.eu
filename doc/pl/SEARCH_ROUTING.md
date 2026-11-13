# Search Routing — alpha-0.5.6

**Bieżący etap — alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

**Etap dostarczony w alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

**Etap dostarczony w alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) dodaje limity liczby i bajtów zapytań oraz wysyłanych/odbieranych wyników na czas życia połączenia, ograniczone powiadomienia, politykę gości/globalną, zaufane uprawnienia sesji i konfigurowalne limity aktywnych wyszukiwań. Kontrola poprzedza routing/rejestrację tokenu. Opcjonalny wymagany token uzupełnia kontrolę właściciela/generacji; bezpośrednie UDP klientów jest poza kontrolą huba.

**Aktualizacja — alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) przenosi sprawdzone I4/I6/U4/U6 do publicznego modelu Users i dodaje metadane dróg odpowiedzi TCP/UDP dla każdej rodziny do Search Core oraz niezmiennych planów. Pasywne DRES nadal przechodzi przez hub, a aktywne URES bezpośrednio między klientami. Testowane są zmiany INF, własność adresów i rzeczywiste wysyłanie/odbieranie UDP przez ncdc.

**Aktualizacja — alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) przekazuje sprawdzone DRES wyłącznie do gotowego celu przez ADC/ADCS. Plan wiąże czas życia sesji oraz — gdy podano TO — generację aktywnego wyszukiwania właściciela celu. Nadal obsługiwane są wyniki bez tokenu. Współdzielone ograniczone kolejki zachowują oryginalne pola bez rozsyłania publicznego, echa do nadawcy i treści w administracji.

**Aktualizacja — alpha-0.5.8:** [ADC RES](ADC_RES.md) dodaje typowany parser DRES z własnymi danymi i rzeczywistą walidację ADC/ADCS nazw, rozmiarów, slotów, tokenów i kanonicznych korzeni TTH po kontroli sesji/źródła. Błędy semantyczne są odwracalne i nie ujawniają treści; poprawne wyniki otrzymują jawną informację o oczekującym routingu. Korelacja tokenów, przekazywanie wyników i UDP pozostają kolejnymi etapami.

**Aktualizacja — alpha-0.5.7:** [Feature Routing](FEATURE_ROUTING.md) kompiluje selektory FSCH raz i korzysta z aktualnego publicznego INF SU: wszystkie wymagane, żadna wykluczająca. Negocjacja SUP jest niezależna. Zmiany możliwości, udział źródła, nieznane rewizje i limity sprawdzono przez ADC/ADCS i rzeczywisty ncdc. RES/routing wyników pozostaje kolejnym etapem.

Etap oparty bezpośrednio na main `a5824cbc2a5d4ab1c8d6ddf99e5af87bfafe30cb` po scaleniu alpha-0.5.5. Zachowano Search Core, SCH, filtry, TTH i tokeny. Kod/diagnostyka protokołu pozostają EN/US UTF-8, z dokumentacją US/PL.

## Rzeczywista dostawa zapytań

`adc/search_routing.hpp` i `src/adc/search_routing.cpp` dodają SearchRouter, RoutedSearch i niezmienny współdzielony SearchFrame. LoginHub posiada teraz Search Core oraz SearchRouter i przekazuje sprawdzone SCH przez istniejące ograniczone kolejki TCP/TLS. Poprawne wyszukiwanie nie otrzymuje już `Search routing not implemented`; odbiór przez wybranych klientów potwierdzają mieszane surowe ADC/ADCS i rzeczywisty ncdc. Nie ma potwierdzenia sukcesu ani tworzenia sztucznych wyników.

[Reguły typów komunikatów ADC](https://adc.sourceforge.io/ADC.html) określają odbiorców:

| Zakres | Odbiorcy |
| --- | --- |
| B | Wszyscy aktualnie gotowi użytkownicy, także źródło |
| F | Gotowi użytkownicy, których publiczne INF SU zawiera wszystkie `+` i żadne `-`; źródło tylko przy spełnieniu warunków |
| D | Tylko aktualnie gotowy cel |
| E | Cel i źródło, pojedyncza kopia przy tym samym życiu sesji |

B/F zachowują deterministyczną kolejność migawki SessionManager. D/E najpierw wybierają życie celu, potem źródło przy E. Źródło, odbiorcy i cel są ID życia sesji, nie fd ani ponownie używalnymi SID. Core dodaje niezależny od protokołu argument `Audience`/`Scope` do prepare/start i rozwiązuje klucze gotowych celów. Brak/niegotowość celu jest odrzucana przed rejestracją/kolejką; zapytanie kierowane nigdy nie przechodzi w broadcast. Zamkniętego celu nie zastąpi nowa sesja z tym samym SID.

Wybór pomija anonimowych, negocjujących, zidentyfikowanych czekających na koniec powitania, zamykanych i zamkniętych. Dispatch ponownie sprawdza źródło oraz gotowość/życie/bieżące funkcje każdego odbiorcy. Późniejsze dołączenia nie są dodawane; utrata funkcji oznacza niedostępność zaplanowanego odbiorcy. Zamknięcie źródła zatrzymuje resztę fanout. Zachowano granicę gotowości końcowego własnego INF zamiast uznawania samego NORMAL za gotowość.

## Sprawdzone dane i wspólne kolejki

SearchRouter najpierw sprawdza gotowość/własność źródła, następnie pełną dotychczasową walidację nagłówka/semantyki SCH. AN/NO/EX, zapis liczb, rozmiary, TY, nieprzezroczyste TO, kanoniczne TR, nieznane pola/kolejność i selektory pozostają w oryginalnym Message. Ogólna serializacja buduje wspólne kodowanie wyjściowe; walidacja zachowuje dotychczasową ograniczoną serializację i ponowne parsowanie. Nie zastępuje się pól znormalizowanym widokiem Filter; klienci szukają we własnych zasobach. Nieznane pola nadal mają kontrolę składni/rozmiaru i są przekazywane zgodnie z rozszerzalnością ADC. Pochodzą z SCH klienta, nie z serwerowej pamięci tożsamości/profili.

Jeden niezmienny Message/pakiet jest współdzielony przez wybrane kolejki. Każdy odbiorca ma oddzielny offset TCP/TLS i pełne logiczne obciążenie bajtowe. Częściowe wysyłanie, przypięte ponowienia TLS, dopisywanie innemu klientowi i rozłączenie źródła nie zmieniają ani nie zwalniają frontu żywej kolejki. Obowiązują limity klienta/ogółu; wolny/odmawiający odbiorca jest zamykany osobno, a zdrowi mogą działać dalej. Błąd alokacji kolejki jest obsługiwany dla tego odbiorcy; błąd przygotowania/diagnostyki zamyka tylko źródło i usuwa jego tokeny. Akceptacja oznacza kolejkę, nie odbiór/przeczytanie.

Kopie RoutedSearch współdzielą stan jednej próby dispatch. Pusty sink/plan obcego Core jest odrzucany bez zużycia poprawnej próby; po rozpoczęciu dispatch także kopia publikacji nie wykona go ponownie. Obiekty po przeniesieniu bez pakietu są odrzucane. Wyjątek sink jest przekazywany po możliwej częściowej dostawie i zużywa próbę; zaufany wywołujący musi odpowiednio jawnie anulować i nie ponawiać całego planu. Zwykłe niskopoziomowe API Core nadal istnieje dla zaufanych modułów; gwarancja jednej próby dotyczy adaptera routingu ADC.

Przygotowanie alokuje sprawdzony pakiet i stan próby przed publikacją aktywnego tokena. Błąd alokacji nie pozostawia nowego TO bez gotowej publikacji. Sink Core jest budowany przez reference wrapper i dispatch nie alokuje po przygotowaniu Sink wywołującego; alokacje kolejek należą do transportu. Migawki odbiorców ogranicza limit sesji i istniejące ograniczone strony. Wspólne kodowanie nie usuwa pełnego logicznego rozliczania odbiorców i nie jest gwarancją pojemności produkcyjnej.

## Aktywne TO i sprzątanie

Poprawne zapytanie z TO używa Core::start i rejestru życia sesji/tokena z alpha-0.5.5. Domyślnie aktywne limity to 4096 ogółem, 8 właściciela i 60 sekund życia. `LoginLimits::search_tokens` pozwala zaufanemu konstruktorowi programu nadpisać limity; nie dodano INI/CLI. Duplikaty aktywnych tokenów i błędy pojemności/wyczerpania są odrzucane odwracalnie bez fanout. Nazwy, TTH i dodatkowe kryteria pozostają w zapytaniu niezmienione.

Brak TO jest poprawny i przekazywany bez zmian przez przygotowanie bez rejestracji; hub nie tworzy sztucznego tokena klienta. Poprawna pusta selekcja funkcji nie daje kopii ani komunikatu sukcesu. Po dispatch token pozostaje tylko gdy choć jedna kolejka przyjęła kopię; brak przyjęcia/odmowy/nieaktualny dispatch kończy wpis. Częściowa dostawa nie jest automatycznie ponawiana.

Udany routing z tokenem pozostawia aktywne okno odpowiedzi do wygaśnięcia lub sprzątnięcia właściciela. Wygaśnięcie jest leniwe przez rejestrację/lookup; disconnect usuwa właściciela przed zwolnieniem modelu/SID. Przy pełnym rejestrze przyjęcie nadal odzyskuje miejsca rozłączonych. Nie dodano automatycznego timera Network ani skanowania całego rejestru przy każdym zapytaniu bez tokena. Pakiet w kolejce może przeżyć źródło, pozostając przed jego uporządkowanym QUI; stary właściciel nie rozpocznie nowego dispatch.

Wewnętrzne generacje chronią przed anulowaniem zastępczych wyszukiwań przez stare uchwyty. TO pozostaje oryginalne, więc jego ponowne użycie nadal samo nie odróżnia spóźnionego starego wyniku. Wydanie nie przyjmuje/koreluje RES i nie deklaruje rozwiązania tej niejednoznaczności. Parser/routing wyników, UDP, pełna wymiana wyników, indeks plików huba, osobny limiter/historia i administracja wyszukiwaniem pozostają kolejnymi etapami. Nadal obowiązują ogólne limity ramek/IP/przyjęcia i wyjścia; limity aktywnych tokenów nie są limiterem tempa zapytań.

## Diagnostyka i prywatność

| Wejście/wynik | Zachowanie protokołu |
| --- | --- |
| Poprawne routowalne SCH | Wybrani klienci otrzymują sprawdzone SCH; bez potwierdzenia ISTA sukcesu/odmowy |
| Brak rozpoznanego kryterium, także sam token | Pominięcie bez rejestracji i przekazania |
| Błędna semantyka/selektory | Odwracalne `100 Invalid search query` ze stałym FOURCC |
| Brak celu D/E | Odwracalne `140 Search recipient unavailable`, bez zastępczego broadcastu/echo |
| Aktywne TO tego właściciela | Odwracalne `100 Search token already active` |
| Limit/wyczerpanie aktywnych wyszukiwań | Odwracalne `100 Active search limit reached` |
| Złe źródło / żądanie przed gotowością | Dotychczasowe fatalne 240 / 244 przed routingiem |
| RES | Dotychczasowe jawne `125 Command not implemented`; bez przekazywania wyników |

Zamierzeni odbiorcy B/F/D/E otrzymują teraz tekst, hashe, tokeny i nieznane pola zapytania potrzebne do wyszukiwania. Te dane nie są cytowane w diagnostyce ani eksportowane do logów, statystyk czatu, bazy, API lub WebAdmin. Nie dodano migracji SQL, endpointu, ustawienia runtime, komunikatu operatora ani kontrolki wyszukiwania. Zachowano istniejące US/PL operatora/UI; metadane przechodzą razem na alpha-0.5.6.

## Weryfikacja

Weryfikacja lokalna **alpha-0.5.6** z 2026-11-12: wszystkie **102/102 zestawy CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym zakresy/życie/tokeny/kolejki/awarie routingu, rzeczywisty mieszany fanout ADC/ADCS, odbiór zapytań ncdc nazw/filtrów/TTH przez drugiego klienta, MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń. Przeszły budowanie produkcyjne/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych/linki dokumentacji, składnię zmienionych skryptów Python, przykładowy INI i białe znaki. Zapisane wyniki są lokalne; zdalne CI jest śledzone w GitHub Actions. Pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.

`search.routing` sprawdza dokładne B/F/D/E, własny cel bez duplikatu, bieżące funkcje, pomijanie niegotowych, życie celu/ponowne SID, zachowanie nieznanych pól/Unicode/zapisu liczb/TTH, wspólny pakiet, 270 odbiorców, brak replay kopii, obcy/pusty sink, wygaśnięcie/duplikaty/pusty wybór, wyjątki sink, rollback alokacji i dispatch bez alokacji. Rzeczywisty LoginHub sprawdza wspólny przypięty front, osobne offsety/pełne obciążenie, sprzątanie źródła, izolację odmowy wolnego celu i brak statystyk czatu.

`search.routing.process` używa fragmentowanych mieszanych ADC/ADCS z gotowymi i oczekującymi klientami, jawnymi SU, dodatnimi/ujemnymi/pustymi selektorami, direct/echo/własnym celem, duplikatami/limitami/niezależnymi właścicielami TO, disconnect/reconnect, brakującym celem, pierwszeństwem źródła/stanu, nieobsługiwanym RES i prywatnością logów/API. Wcześniejsze testy procesowe SCH/Core/tokenów sprawdzają teraz właściwe kopie i brak kopii dla odmów zamiast starej odmowy poprawnego wyszukiwania.

Rzeczywiste ncdc nazw/filtrów i TTH nadal działają przez ADC oraz ADCS. Surowy obserwator ogłasza TCP4 i sprawdza odbiór SCH właściwego źródła; tekst ADC potwierdza pola/echo, a ADCS zachowuje szyfrowanie. Testy nie deklarują wyników ani transferów plików. Powtórz standardowe Debug/Release/ASan, `ctest --test-dir build-debug -R 'search\.|adc.sch|ncdc' --output-on-failure -j 4`, a potem pełną serię. Zobacz [TESTING.md](TESTING.md), [Search Tokens](SEARCH_TOKENS.md), [ADC SCH](ADC_SCH.md), [ROADMAP.md](ROADMAP.md) i [CHANGELOG.md](../../CHANGELOG.md). Gałąź nie scala się automatycznie.
