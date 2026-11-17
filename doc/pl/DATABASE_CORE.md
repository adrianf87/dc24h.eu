# Database Core — alpha-0.7.1

**alpha-0.7.13 Database Resilience:** [jawne odnowienie połączenia i niezależne limity czasu](DATABASE_RESILIENCE.md) dodają pojedynczą próbę reconnect w wątku właściciela, ponowną walidację pliku hasła oraz ochronę przed powtarzaniem SQL i zastąpieniem aktywnej transakcji. Schemat v6 i migawki uwierzytelniania pozostają. CI korzysta z runnerów GitHub-hosted Ubuntu 24.04; nie czekamy na jego zakończenie. Ten wpis zastępuje wcześniejsze opisy braku reconnect.

**Wersjonowanie schematu — alpha-0.7.2:** [Migracje bazy danych](DATABASE_MIGRATIONS.md) opisują uporządkowany katalog, aktualizację v1, odczyt stanu i obsługę częściowych błędów.

Etap oparty bezpośrednio na main `85a2d155270ff7f218ec336a7013fc65a6471275` po alpha-0.6.13. Zachowuje opcjonalne połączenie przez gniazdo i rejestr migracji. Dodaje synchroniczny rdzeń MariaDB dla C++20: typowane parametry, ograniczone wyniki na własność, metadane zapisu, typowane zredagowane błędy i transakcje zakresowe. Konta, uwierzytelnianie, role, bany i repozytoria aplikacji pozostają kolejnymi etapami; bez nowej migracji aplikacyjnej.

## Własność połączenia i transport

Budowanie wymaga MariaDB Connector/C 3.x (`libmariadb-dev` we wspieranych Debian/Ubuntu). Nagłówki/uchwyty biblioteki są prywatne w `src/database/connection.cpp`; interfejs publiczny to `dc24h/database/database.hpp`. Konfiguracja, prywatne hasło i niezmieniona migracja podstawowa pozostają w `database.cpp`.

`Connection` nie można kopiować/przenosić; zapisuje wątek utworzenia. Wywołanie z innego wątku kończy się błędem przed dostępem do Connector/C i zmiennego stanu właściciela. Przyszłe wątki bazy powinny tworzyć własne połączenia i zakończyć pracę przed zamykaniem procesu/obiektów statycznych. Etap nie dodaje puli, asynchronicznego SQL ani operacji bazy w reaktorze Network. Włączony start nadal łączy/sprawdza bazę przed nasłuchem, zachowuje połączenie do wyłączenia i sam nie stosuje migracji.

Lokalny statyczny obiekt C++ szereguje `mysql_library_init` przed pierwszymi `mysql_init` modułu i zwalnia bibliotekę przy zamykaniu procesu. Instrukcje/połączenia zwalniają własne zasoby. Współczesne funkcje thread-init/end Connector/C są przestarzałe; nie służą tu do kontroli własności ani synchronizacji.

Zachowano lokalne gniazdo Unix, `utf8mb4`, kontrolę prywatnego pliku hasła, oddzielne timeouty connect/read/write, wyłączone reconnect i LOCAL INFILE. Nie dodano TCP/TLS do bazy, odczytu konfiguracji hosta, haseł w argumentach, tworzenia schematów/użytkowników ani zmian uprawnień. Tymczasowe hasło aplikacji powstaje bezpośrednio w czyszczonym buforze; Connector/C może utrzymywać własne kopie do zamknięcia. Konfigurację operatora opisuje [MariaDB](DATABASE.md).

## Typowane wartości i wykonywanie

```cpp
using namespace dc24h::database;
Connection connection(options); // włączona, wcześniej przygotowana lokalna baza
const auto result = connection.query(
    "SELECT ? AS label, ? AS quantity, ? AS missing",
    {std::string("Zażółć 😺"), std::int64_t{-7}, nullptr});

// Przyszła tabela InnoDB aplikacji; rdzeń jej nie tworzy.
auto transaction = connection.transaction();
const auto inserted = connection.execute_prepared(
    "INSERT INTO application_table(label, payload) VALUES (?, ?)",
    {std::string("example"), Bytes{0, 255, 0}});
transaction.commit();
```

`Parameter` posiada SQL NULL (`nullptr`), liczbę całkowitą signed/unsigned 64-bit, tekst `std::string` albo binarne `Bytes`. Znak liczby jest wiązany jawnie z użyciem typów liczbowych Connector/C; zachowane są całe zakresy int64/uint64. Pusty tekst/blob, wewnętrzny NUL i SQL NULL są różne. Dane nie są wklejane do SQL, normalizowane, interpretowane jako składnia ani zmieniane pod względem wielkości liter. Tekst używa `utf8mb4`, dane binarne wiązania BLOB. Parametry zmiennoprzecinkowe i dekodowanie dziedzinowe pozostają przyszłe.

SQL jest zaufanym kodem aplikacji, nie językiem zapytań klienta ADC/przeglądarki. Placeholder wiąże wartość, nie identyfikator. Struktura SQL i identyfikatory muszą pochodzić z zaufanego kodu; bez konkatenacji niezaufanych danych. Multi-statements pozostaje wyłączone. `execute_prepared` wymaga instrukcji bez zbioru wyników i zwraca `Execution{affected_rows, insert_id}` danej instrukcji. `query` zwraca jeden zbiór; dodatkowe zbiory są odrzucane, a pozostałe wyniki anulowane. Dotychczasowe `execute` i `scalar` zachowują parametry tekstowe i kontrolę kształtu; `scalar` nadal wymaga jednego wiersza/kolumny do 4096 bajtów, z `nullopt` dla SQL NULL.

## Limity wejścia i wyników

| Zasób | Limit |
| --- | --- |
| SQL | Niepusty, do 65536 bajtów, bez wewnętrznego NUL |
| Parametry | Do 256; tekst/blob do 65536 bajtów; 1048576 wiązanych bajtów łącznie |
| Wynik domyślny | 256 wierszy, 32 kolumny, 65536 bajtów/komórkę, 1048576 bajtów treści komórek |
| Maksymalne `QueryLimits` | 1024 wiersze, 64 kolumny, 65536 bajtów/komórkę, 4194304 bajtów treści łącznie |
| Nazwa kolumny | Do 1024 bajtów; metadane dodatkowo ogranicza liczba kolumn |

Limit wierszy/komórki/sumy może wynosić zero przy sprawdzaniu pustych wyników lub wartości NULL/pustych; liczba kolumn musi być 1..64. Niepoprawne limity są odrzucane przed przygotowaniem SQL. Przechowywanie liczb w parametrach wlicza się do sumy bajtów. Narzut obiektów wyniku ograniczają dodatkowo maksima wierszy i kolumn.

`Result` posiada nazwy kolumn i wiersze (`vector<optional<string>>`). Wyniki liczbowe mają tekstową reprezentację Connector/C; bajty blob/tekstu są zachowane dokładnie, także NUL. Wynik pozostaje ważny po kolejnych zapytaniach. Odczyt pobiera najpierw długość pola, sprawdza limity przed utworzeniem własnego napisu i pobiera dokładne bajty przez `mysql_stmt_fetch_column`. Nie używa `mysql_stmt_store_result` i nie udostępnia pożyczonego kursora/wskaźnika. Przekroczenie limitu zgłasza typowany błąd i zamyka instrukcję, bez częściowego wyniku.

Limity dotyczą zwracanych danych C++ i struktur wiązań, nie planu wykonania serwera, buforów pakietów Connector/C ani łącznego czasu. Stosuj zaufane ograniczone schematy/zapytania oraz SQL `LIMIT`; timeouty operacji nie tworzą usługi anulowania zapytań. Błąd wyniku/kształtu nie dowodzi braku skutków SQL: automatycznie zatwierdzony `INSERT ... RETURNING` może zakończyć zapis przed walidacją wyniku. Gdy sprawdzenie wyniku ma decydować o atomowości zapisu, użyj jawnej transakcji.

## Kontrakt transakcji

`Connection::transaction()` wykonuje stałe `START TRANSACTION` i zwraca przenoszalną konstrukcją `Transaction`. Zagnieżdżenie jest odrzucane przed SQL. Jawne `commit`/`rollback` kończą zakres; powtórne wywołanie i zakończony dawny zakres nie zatwierdzą ani nie wycofają nowego. Destrukcja aktywnego zakresu wycofuje zmiany, także podczas wyjątku. Słabe powiązanie czasu życia pozwala bezpiecznie przetrwać zamknięcie Connection: samo zamknięcie już wycofało zapis, późniejszy commit/rollback zgłasza błąd lokalny.

Po błędzie SQL/wyniku/alokacji w aktywnym zakresie możliwy jest wyłącznie rollback; dalszy SQL i commit są blokowane do wycofania. Timeout blokady (1205) i deadlock (1213) mają kategorię konfliktu; bez automatycznego powtarzania instrukcji/transakcji. Jawne wycofanie przywraca użycie, jeśli transport/stan są poprawne. Błąd sprzątania wycofuje połączenie z użycia. Destrukcja Transaction w obcym wątku atomowo żąda wycofania połączenia; właściciel zamknie je przed następną operacją, bez SQL w obcym wątku.

W zakresie dozwolone są zwykłe SELECT/INSERT/UPDATE/DELETE/REPLACE/WITH, bez rozróżniania liter słowa kluczowego i z dopuszczalnymi początkowymi białymi znakami ASCII. Początkowe komentarze, DDL, SET, procedury i ręczne komendy transakcji są blokowane przed prepare. To ochrona atomowości, nie pełny parser SQL ani piaskownica niezaufanych zapytań. Używaj kontrolowanych tabel InnoDB: silnik nietransakcyjny i skutki zewnętrzne nie podlegają rollbackowi biblioteki. Migracje są blokowane w transakcji przed zajęciem blokady doradczej.

Flagi autocommit/transakcji serwera są sprawdzane przez `mariadb_get_infov`. Ręcznie otwarta transakcja bez właściciela lub wyłączenie autocommit zamyka połączenie, także po błędzie wyniku. Po wycofaniu przez serwer właściciel nadal musi zakończyć swój nieudany zakres RAII. Bez resetu, reconnect, powtórzeń, savepointów i cichego zastępowania transakcji.

## Błędy i niepewny wynik

`database::Error` udostępnia `ErrorKind`, `Operation`, opcjonalny liczbowy kod biblioteki/serwera i `outcome_unknown`. Kategorie obejmują inicjalizację, poświadczenia, połączenie, instrukcję, ograniczenie danych, konflikt, kształt wyniku, limity zasobów, stan transakcji i obcy wątek. `what()` ma stały tekst EN/US z tłumaczeniem PL w aplikacji; bez SQL, wartości parametrów, haseł, ścieżek i surowych błędów serwera. Błąd alokacji może nadal propagować standardowy wyjątek C++.

Błąd transportu/protokołu wycofuje połączenie z użycia po zamknięciu instrukcji. Kolejne wywołania kończą się lokalnie, bez reconnect i odtwarzania zapisów. Brak potwierdzenia execute/fetch/commit jest zachowawczo oznaczany jako niepewny: zapis mógł już zostać zatwierdzony i rdzeń nie rozstrzygnie tego przez zerwane połączenie. Flaga dotyczy niepewnego potwierdzenia, a nie ogólnej gwarancji braku skutków przy wartości false. Idempotencja/uzgadnianie wyniku i świadome ponowienie na nowym połączeniu należą do przyszłych usług.

## Weryfikacja i dalszy zakres

`database.core` sprawdza limity i metadane błędów bez serwera. Izolowany harness MariaDB zachowuje `database.unit`/`database.process` i dodaje `database.core.process` oraz `database.ncdc.process`. Rzeczywiste testy obejmują równoległą inicjalizację połączeń, Unicode/tekst przypominający injection, dane binarne/NUL, skrajne liczby, limity wejścia/wierszy/kolumn/komórek/sumy, metadane zapisu, własność wyników, commit/rollback/wyjątki/przenoszenie/wygasłą własność, blokady zagnieżdżenia/DDL/sterowania, uprawnienia tylko do odczytu, błędy SQL/wyniku, dodatkowe zbiory wyników, timeout blokady, deterministyczny deadlock, zabite połączenia, wycofanie i niepewny commit. Przejście 64 pozycji awarii alokacji C++ sprawdza blokadę zatwierdzenia i powrót do działania.

Włączony hub z bazą jest dodatkowo sprawdzany prawdziwym ncdc przez login/reconnect/chat po ADC i ADCS; wcześniejsze testy protokołu/frontendu pozostają. Testy tworzą tylko prywatne tymczasowe katalogi danych i plików SQL (inicjalizacja i działanie), gniazda, użytkowników, tabele i procedurę, używają `--no-defaults --skip-networking` i usuwają je po zakończeniu. Bez zmian konfiguracji MariaDB hosta, kont, schematów i zewnętrznych runnerów. Końcowe wyniki zapisano w [historii zmian](../../CHANGELOG.md); bez oczekiwania na zdalny Check status.

Etap nie dodaje schematu kont/aplikacji, uwierzytelniania użytkowników, hashowania haseł, ORM/repozytoriów, puli/workerów, monitorowania zdrowia SQL, zapisu WebAdmin, zależności ani wdrożenia produkcyjnego. Flaga API `database.enabled` nadal oznacza konfigurację startową, nie aktualne zdrowie SQL. Zmiana publicznego C++ wymaga przebudowania zależnych komponentów.

Źródła: [wiązanie parametrów](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_bind_param), [odczyt kolumny](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_fetch_column), [anulowanie/zamknięcie instrukcji](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_close), [flagi stanu serwera](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mariadb_get_infov), [transakcje](https://mariadb.com/docs/server/reference/sql-statements/transactions/start-transaction).
