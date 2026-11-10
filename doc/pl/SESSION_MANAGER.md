# Session Manager — alpha-0.3.2

Etap bazuje na main `ca00266992dd866bdd30ff68e911c981c6665a62`, zawierającym Session Core. `users::SessionManager` w niezależnej bibliotece dc24h_users jest jedynym właścicielem modeli zarejestrowanych sesji. LoginHub posiada jeden manager, a jego obiekty protokołu pożyczają modele. Własność TCP/TLS, indeksy SID/CID/nick, kolejki, walidacja i zachowanie protokołu pozostają w dotychczasowych modułach.

## Rejestracja i czas życia

Manager ma jawny limit 1–65535 wpisów. LoginHub wykorzystuje istniejący limit połączeń. Rejestr obejmuje wszystkie zarejestrowane transporty: ADCS oczekujące na TLS, negocjowanie ADC, przyjętą tożsamość oczekującą na wysłanie powitania, gotowych klientów i zamykane sesje. Zamykane wpisy zajmują miejsce do sprzątania. To inna liczba niż liczba publicznych użytkowników lub gotowych klientów.

`add` przyjmuje niezerowy SessionId, transport i przekazany czas przyjęcia. Sukces zwraca pożyczony wskaźnik Session; typowane błędy rozróżniają stare/niemalejąco niepoprawne ID i brak miejsca. ID muszą rosnąć przez cały czas życia managera, także po usunięciu wpisów. Odrzucenie z powodu pojemności nie zatwierdza ID ani wpisu; Network nadal korzysta z sekwencji bez ponownego użycia. Wyjątek alokacji nie pozostawia częściowego wpisu ani nie przesuwa najwyższego ID. Rejestracja nie rozpoczyna negocjacji — to decyzja adaptera.

Uporządkowana mapa posiada modele przez unique_ptr. Dodanie lub usunięcie innego wpisu nie unieważnia referencji. Unieważnia ją usunięcie danego modelu lub zniszczenie managera. Dostęp odbywa się wyłącznie w wątku reaktora; nie ma blokad, callbacków, uchwytów między wątkami ani trwałych identyfikatorów. Managera nie można kopiować ani przenosić.

`find` zwraca null dla brakującego ID; odczyt const udostępnia niezmienny model. `remove` zwraca missing lub not_closed zamiast usuwać żywą sesję. Właściciel transportu/protokołu najpierw usuwa indeksy, anuluje wyjście, rozłącza protokół i finalizuje model. LoginHub następnie niszczy obiekt protokołu i usuwa zamknięty model, bez historii usuniętych wpisów. Ponowne disconnect w hubie jest nieszkodliwe. Błąd konstrukcji obiektu protokołu zamyka/usuwa nowo zarejestrowany model, zachowując zużycie ID. Destrukcja zwalnia pamięć bez operacji na gniazdach i ukrytych callbacków.

## Odczyt

`snapshot` zwraca samodzielną kopię lub nullopt. `page(after, limit)` zwraca kopie w rosnącym porządku SessionId, ściśle po wyłącznym kursorze; zero rozpoczyna listę. Limit wynosi 1–256, domyślnie 64; błędny limit powoduje invalid_argument przed kopiowaniem. Pusta strona oznacza bieżący koniec. Ostatnie ID jest kursorem kolejnej strony. Luki i usunięty kursor są obsługiwane, a UINT64_MAX nie zawija. Każda strona odczytuje bieżący stan w wątku właściciela; wiele stron nie stanowi zamrożonej transakcji, więc dodawanie/usuwanie wpływa na kolejne wyniki.

`counts` wylicza sumę i sześć faz z rzeczywistych modeli w O(n), bez powielonych liczników przejść. Samodzielny manager pokazuje zamknięte wpisy do jawnego usunięcia; hub usuwa je w tym samym wywołaniu sprzątania. Rejestracja, wyszukiwanie i usuwanie mają O(log n); strona O(log n + limit), poza kopiowaniem ograniczonej tożsamości. Nie dodano nieograniczonej kopii całego rejestru. Liczba wpisów jest ograniczona, a modele przechowują wyłącznie dotychczasową ograniczoną tożsamość publiczną i cykl życia.

`LoginHub::sessions()` udostępnia wewnętrzny odczyt const. Nie jest to endpoint HTTP ani funkcja WebAdmin. Nie potrzeba nowej konfiguracji, komunikatów, tłumaczeń, danych logowania, schematu SQL, zależności ani pól JSON API. Istniejące katalogi runtime US/PL pozostają aktualne. Konta, role, bany, model profilu użytkownika, trwałość, rejestry rozproszone i zdalna administracja są osobnymi etapami.

## Weryfikacja

`users.manager` sprawdza pojemność i ID, usuwanie brakujących/żywych/zamkniętych wpisów, liczenie faz, stabilność adresów, własność stron, kursory z lukami, maksymalny kursor/ID, błędne limity, 2000 czasów życia i integrację z rzeczywistymi modelami LoginHub. Testy adaptera obejmują wycofanie rejestracji po błędzie konstrukcji protokołu, stare ID po takim błędzie i pusty rejestr po sprzątaniu ADC/ADCS. Dotychczasowe testy transportu i rzeczywistego ncdc sprawdzają zintegrowany rejestr.

Polecenia budowania i pełnych testów opisano w [testowaniu](TESTING.md). [Session Core](SESSION_CORE.md) opisuje przejścia, zegary, ograniczenia tożsamości i sprzątanie. Testy lokalne nie dowodzą działania zdalnego CI, innych systemów ani obciążenia produkcyjnego.
