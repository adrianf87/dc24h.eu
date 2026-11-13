# Search Tokens — alpha-0.5.5

**Bieżący etap — alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

**Etap dostarczony w alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) dodaje limity liczby i bajtów zapytań oraz wysyłanych/odbieranych wyników na czas życia połączenia, ograniczone powiadomienia, politykę gości/globalną, zaufane uprawnienia sesji i konfigurowalne limity aktywnych wyszukiwań. Kontrola poprzedza routing/rejestrację tokenu. Opcjonalny wymagany token uzupełnia kontrolę właściciela/generacji; bezpośrednie UDP klientów jest poza kontrolą huba.

**Aktualizacja — alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) przenosi sprawdzone I4/I6/U4/U6 do publicznego modelu Users i dodaje metadane dróg odpowiedzi TCP/UDP dla każdej rodziny do Search Core oraz niezmiennych planów. Pasywne DRES nadal przechodzi przez hub, a aktywne URES bezpośrednio między klientami. Testowane są zmiany INF, własność adresów i rzeczywiste wysyłanie/odbieranie UDP przez ncdc.

**Aktualizacja — alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) przekazuje sprawdzone DRES wyłącznie do gotowego celu przez ADC/ADCS. Plan wiąże czas życia sesji oraz — gdy podano TO — generację aktywnego wyszukiwania właściciela celu. Nadal obsługiwane są wyniki bez tokenu. Współdzielone ograniczone kolejki zachowują oryginalne pola bez rozsyłania publicznego, echa do nadawcy i treści w administracji.

**Aktualizacja — alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) przekazuje sprawdzone SCH przez rzeczywiste ADC/ADCS: broadcast B/F i D/E związane z życiem celu współdzielą jeden niezmienny pakiet z oddzielnymi ograniczonymi kolejkami. Podane TO jest rejestrowane w Search Core; brak TO nadal jest poprawny. Obowiązują kontrole źródła/stanu/funkcji, jednokrotny dispatch i sprzątanie tokenów. RES/routing wyników pozostaje kolejnym etapem.

Etap oparty bezpośrednio na main `c9b020c0842550862a86988bdcf9b6cb97e6b6b9` po scaleniu alpha-0.5.4. Zachowano wcześniejsze Search Core, SCH, filtry i TTH. Kod/diagnostyka pozostają EN/US UTF-8, z dokumentacją US/PL.

## Rejestr i tożsamość

`search/tokens.hpp` i `src/search/tokens.cpp` udostępniają rejestr `Tokens` dla wątku właściciela. Korzysta on z SessionManager, który musi żyć dłużej od rejestru. Klucz to dokładne **ID życia sesji + zdekodowane TO**, a nie SID, nick, CID, fd ani sam token. Dwaj gotowi właściciele mogą niezależnie użyć takiego samego tokena. Duplikat aktywnego tokena tego samego właściciela jest odrzucany i nie odświeża terminu.

TO zachowuje politykę Query/SCH: niepuste gotowe NFC UTF-8, do 128 bajtów, bez znaków sterujących/formatujących i separatorów wierszy. Spacje, Unicode, wielkość liter i odwrotne ukośniki pozostają nieprzezroczyste i niezmienione. Rejestr nie przycina, nie normalizuje, nie zmienia wielkości liter, nie interpretuje tokena jako ścieżki/polecenia i nie używa jako hasła. Puste, zbyt duże i błędne tokeny są odrzucane; lookup porównuje dokładne bajty bez alokacji i ponownej walidacji Unicode.

Każda przyjęta rejestracja otrzymuje niezerową lokalną generację 64-bitową. ID rosną, błędy alokacji ich nie zużywają, a wyczerpanie nie powoduje zawijania/ponownego użycia. `TokenHandle` ma własny token, migawkę generacji/właściciela/terminu i nieprzezroczyste odwołanie do domeny rejestru. Kopie wskazują tę samą rejestrację; przeniesienie przekazuje domenę. Uchwyt można bezpiecznie odczytać po zniszczeniu rejestru, ale stary/obcy uchwyt nie anuluje wpisu nowego rejestru nawet pod tym samym adresem i z identycznym ID liczbowym. Token ani ID nie jest poświadczeniem uwierzytelniania ani globalnie unikalnym identyfikatorem.

`Tokens::activate(owner, token, now)` zwraca `variant<TokenHandle, TokenError>` i wymaga aktualnie gotowego właściciela. `find(owner, token, now)` zwraca opcjonalną skalarną migawkę `ActiveSearch` (ID, właściciel, termin), nie treść zapytania. `active(handle, now)` sprawdza domenę, bieżącego właściciela, termin i generację. `cancel(handle)` usuwa tylko tę zapisaną generację; ponowne/stare/obce anulowanie zwraca false. Anulowanie jest sprzątaniem: może usunąć wygasły wpis jeszcze nieuprzątnięty. `forget(owner)` usuwa wszystkie zapisane wpisy tego życia sesji.

Po anulowaniu/wygaśnięciu ten sam właściciel może zarejestrować ponownie token przewodowy z nową wewnętrzną generacją. Stary uchwyt nie usunie ani nie wyśle ponownie zastępczego wyszukiwania. **Samo ponownie użyte TO nie odróżnia spóźnionej odpowiedzi na wcześniejsze zapytanie.** Etap nie przyjmuje wyników RES; przyszły adapter musi określić politykę tokenów przewodowych/generacji i kontrole nadawcy/odbiorcy przed deklaracją bezpiecznej korelacji zdalnej. Wewnętrzna generacja sama nie rozwiązuje tej niejednoznaczności protokołu.

## Limity, czas i sprzątanie

| Lokalna polityka biblioteki | Domyślnie | Dopuszczalne granice |
| --- | --- | --- |
| Wszystkie zapisane wyszukiwania | 4096 | 1–65535 |
| Zapisane wyszukiwania właściciela | 8 | 1–64, nie więcej niż limit ogólny |
| Czas życia | 60 sekund | 1–3600 sekund |
| Token | Istniejący limit 128 bajtów | Dokładne zdekodowane TO gotowego NFC |

Limity konstruktora są polityką aplikacji, nie nowymi polami ADC, limiterem sieciowym, ustawieniami INI ani deklaracją interoperacyjności. `first_id` może ustalić początek lokalnego strumienia ID nowej domeny; zero jest błędne, a UINT64_MAX jest ostatnią dostępną generacją.

Wywołujący dostarcza `users::TimePoint` zegara monotonicznego; rejestr nie odczytuje zegara i nie uruchamia wątku/timera. Obserwowany czas jest ograniczany monotonicznie, więc próbki wsteczne nie wydłużają terminów ani nie odtwarzają wygasłych wpisów. Wygaśnięcie następuje przy `now >= expires`. Dodawanie czasu nasyca się na maksimum zegara bez przepełnień; nowy wpis bez dodatniego pozostałego czasu życia jest odrzucany. Duplikaty, odmowy przyjęcia i lookup wyników nie wydłużają terminu.

Uporządkowany indeks właściciela/tokena umożliwia ograniczone zliczanie wpisów właściciela i lookup bez alokacji. Indeks terminów wskazuje stabilne klucze mapy, więc wygaśnięcie usuwa tylko należne wpisy. Oba indeksy są zatwierdzane razem; błąd alokacji usuwa częściowe wstawienie i nie zużywa generacji. Uchwyt jest całkowicie przydzielony przed publikacją, a późniejsze przeniesienia nie alokują. Awarie biblioteki Unicode/alokatora C są osobne od kampanii alokacji C++.

Aktywacja i lookup usuwają wygasłe wpisy. Lookup brakującego/niegotowego właściciela usuwa jego wpisy. `prune(now)` dodatkowo przegląda zapisane pozycje i usuwa brakujących/zamkniętych właścicieli; wywołujący powinien używać go w cyklu utrzymaniowym. Przy pełnym rejestrze przyjęcie sprząta rozłączonych właścicieli przed odmową pojemności. Nie podłączono automatycznego hooka SessionManager ani timera Network. `size()` oznacza zapisane pozycje, także czekające na sprzątnięcie po zamknięciu właściciela. `find`, `active`, `cancel`, `forget` i `prune` nie alokują i są noexcept. Wygaśnięcia ogranicza ogólny limit, zliczanie właściciela jego limit, a jawne sprzątanie rozłączonych przegląda najwyżej zapisany ogół.

## Cykl życia Search Core

Dotychczasowe `prepare()` / `dispatch(Plan, sink)` nadal przygotowują/wykonują koordynację bez rezerwacji TO. Nowe `Core::start(sender, source, data, now, selection)` najpierw sprawdza gotowość i własność źródła, wymaga jawnego tokena, przygotowuje sprawdzone zapytanie/plan odbiorców i rejestruje token. Zwraca `variant<ActivePlan, Error, QueryError, TokenError>`. Błędne zapytanie, selektory, brak tokena, duplikat/limit i awaria alokacji nie publikują częściowego aktywnego planu; wcześniejsze aktywne wpisy i modele użytkowników pozostają nienaruszone (sprzątanie należnych wygaśnięć może nadal nastąpić).

`ActivePlan` posiada Plan i TokenHandle. `dispatch(activePlan, sink, now)` sprawdza domenę Core, nadawcę i bieżącą rejestrację, a następnie istniejące życie odbiorców/funkcje. Przed każdym odbiorcą ponownie sprawdza aktywność tokena. Zakończenie/anulowanie wewnątrz zaufanego sink anuluje pozostałych odbiorców; nie cofa wcześniejszego przyjęcia do kolejki. Dispatch nie alokuje po utworzeniu Sink. Wyjątek sink nadal jest przekazywany po możliwym częściowym dostarczeniu, bez automatycznego ponowienia ani zakończenia.

`complete(activePlan)` anuluje tę generację. Udany dispatch pozostawia wyszukiwanie aktywne na czas odpowiedzi; przyjęcie do kolejki nie oznacza ukończenia. `find_search(owner, token, now)` udostępnia lokalną aktywną tożsamość, a `prune_searches(now)` sprząta. Plany/uchwyty wywołującego nie anulują wpisu przy zniszczeniu, więc wymagane jest jawne zakończenie albo ograniczone wygaśnięcie. Śledzenie tokenów nie dodaje automatycznej ochrony przed wielokrotnym dispatch/replay; zaufane adaptery nadal odpowiadają za jednorazowe planowanie, walidację wyników i kolejki. Dispatch zwykłego Plan pozostaje dostępny i nie sprawdza tego rejestru.

## Granica rzeczywistego ADC/ADCS

[Specyfikacja ADC SCH](https://adc.sourceforge.io/ADC.html) definiuje opcjonalne TO. Od alpha-0.5.6 [Search Routing](SEARCH_ROUTING.md) wywołuje Core::start dla poprawnych zapytań z TO, a brak TO obsługuje bez rejestracji i tworzenia sztucznego tokena. Odmowy duplikatu/limitu nie są rozsyłane, dispatch bez przyjętych kopii/pustej selekcji kończy token, a disconnect usuwa wpisy właściciela. Poprawny routing nie wysyła potwierdzenia. RES pozostaje nieobsługiwane; generacje nie rozwiązują niejednoznaczności spóźnionych wyników z ponownie użytym TO.

Nie dodano routingu wyników, odpowiedzi UDP, historii bazy, sterowania wyszukiwaniem API/UI, komunikatu operatora ani konfiguracji. Klucze/metadane rejestru pozostają tymczasowe; kryteria istnieją tylko w Query/Plan należących do wywołującego. Token, ID wyszukiwania, treść, nazwa pliku i TTH nie są eksportowane do logów, liczników czatu, magazynu, API ani WebAdmin. Zachowano istniejące `languages/us` i `languages/pl`. Metadane serwera/WebAdmin przechodzą razem na alpha-0.5.5.

## Weryfikacja

Weryfikacja lokalna **alpha-0.5.5** z 2026-11-12: wszystkie **100/100 zestawów CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym cykl życia/awarie tokenów, rzeczywiste ADC/ADCS odmowy/ponowne użycie/prywatność, ncdc, MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń. Przeszły budowanie produkcyjne/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych/linki dokumentacji, składnię zmienionych skryptów Python, przykładowy INI i białe znaki. Zapisane wyniki są lokalne; zdalne CI jest śledzone w GitHub Actions. Pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.

`search.tokens` sprawdza nieprzezroczyste tokeny case/Unicode i dokładne limity bajtów, niezależnych właścicieli, pierwszeństwo stanu/źródła, duplikaty bez odświeżenia, granice wygaśnięć/czas wsteczny/skrajny, limity ogólne/właściciela, disconnect/ponowne SID, kopie/przeniesienia/stare/obce/ten sam adres uchwytów, wyczerpanie ID, sprzątanie/dispatch bez alokacji, projekcję SCH/TTH, zakończenie/anulowanie w trakcie dispatch Core oraz transakcyjny rollback alokacji obu indeksów i startu Core. Wcześniejsze testy Core/filtrów/TTH/parsera pozostają włączone.

`search.tokens.process` używa rzeczywistego ADC/ADCS do powtarzanych zapytań z tym samym tokenem ponad domyślny limit właściciela, drugiego właściciela, błędnych duplikatów TO, TTH, braku/samego tokena, disconnect/reconnect i nieobsługiwanego RES. Uporządkowane bariery INF/czat potwierdzają brak fanout; kontrole API/logów potwierdzają prywatność tokena/treści i niezmienione liczniki czatu dla wyszukiwań. Istniejące rzeczywiste testy ncdc nazw/filtrów i TTH pozostają na obu transportach; brak opcjonalnego TO nadal jest poprawny. Testy transportu potwierdzają granicę odrzuconego wyszukiwania, nie udane aktywne zdalne wyszukiwanie/wyniki.

Powtórz standardowe Debug/Release/ASan i `ctest --test-dir build-debug -R 'search\.|adc.sch|ncdc' --output-on-failure -j 4`, następnie całą serię. Zobacz [TESTING.md](TESTING.md), [Search Core](SEARCH_CORE.md), [ADC SCH](ADC_SCH.md), [ROADMAP.md](ROADMAP.md) i [CHANGELOG.md](../../CHANGELOG.md). Gałąź zachowuje historię i nie scala się automatycznie do main.
