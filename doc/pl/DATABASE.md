# MariaDB — alpha-0.7.1

**Aktualny etap — alpha-0.7.1:** [Database Core](DATABASE_CORE.md) dodaje typowane parametry, ograniczone wyniki na własność, metadane zapisu, egzekwowanie wątku właściciela, zredagowane błędy i transakcje RAII. Niezmienna migracja 1 i konfiguracja operatora poniżej pozostają; wymagane przebudowanie zależnych komponentów.

Zaimplementowano opcjonalne połączenie Connector/C, RAII, ograniczenia czasu, kodowanie `utf8mb4`, zapytania parametryzowane i jawną migrację rejestru wersji. Konta użytkowników, uwierzytelnianie i hashowanie haseł, role, uprawnienia, konfiguracja w bazie, bany, dane administracyjne oraz zapisywanie zdarzeń pozostają planowane. Haseł użytkowników huba jeszcze nie przechowujemy; przyszły moduł powinien korzystać ze sprawdzonej implementacji Argon2id. WebAdmin będzie używał API C++, nie bezpośredniego SQL.

## Budowanie i konfiguracja

Zainstaluj `libmariadb-dev`; biblioteka wykonawcza Connector/C jest linkowana także przy wyłączonej bazie. Działający serwer MariaDB jest opcjonalny. Wyniki lokalnych testów: [TESTING.md](TESTING.md).

Ustawienia są wyłącznie w sekcji INI `[database]`. Przykład ma `enabled=off`; wtedy program nie odczytuje hasła, nie łączy się i nie migruje. Po włączeniu start łączy się i wykonuje ping przed otwarciem portów huba; błąd kończy program kodem 1. Połączenie istnieje do zakończenia programu. Brak automatycznego ponownego łączenia, puli, monitorowania w czasie pracy i operacji SQL w reaktorze Network. Limity connect/read/write dotyczą poszczególnych operacji Connector/C, nie łącznego czasu; DNS może trwać dłużej. Sygnały podczas synchronicznego startu są obsługiwane po zakończeniu zestawiania połączenia.

| Klucz | Wartość domyślna / zasada |
| --- | --- |
| `enabled` | `off`; `on` wymaga `password_file` |
| `socket` | `/run/mysqld/mysqld.sock`; ścieżka względna do katalogu INI |
| `name` | `dc24h`; 1..64 liter/cyfr ASCII lub znaków podkreślenia |
| `user` | `dc24h`; 1..128 bajtów bez znaków sterujących |
| `password_file` | brak wartości domyślnej; ścieżka względem INI |
| `timeout` | 5 sekund; zakres 1..60 dla connect/read/write |

Nieznane/powtórzone klucze i błędne kombinacje są odrzucane również przy wyłączonej bazie. Brak opcji podania hasła w INI, argumentach, środowisku i kodzie. `--check-config` sprawdza ustawienia i certyfikaty ADCS, ale nie czyta hasła bazy, nie łączy z SQL, nie zajmuje portów ani nie otwiera logu. Log nie może wskazywać hasła ani ich aliasów; zachowano też ochronę INI i plików ADCS.

## Hasło i transport

Utwórz bazę i osobne konto narzędziami administratora MariaDB. Nie uruchamiaj usługi na koncie SQL root. Konto wykonawcze powinno mieć `SELECT` w wybranym schemacie (jego wybór wymaga uprawnień); jawna migracja dodatkowo wymaga `CREATE` i `INSERT`. Dla przyszłych migracji stosuj osobne dane operatora. Program nie tworzy automatycznie bazy, użytkowników ani uprawnień.

Hasło umieść w pliku należącym do efektywnego UID procesu, bez praw grupy/innych, z jednym dowiązaniem twardym i długością do 4096 bajtów (zwykle tryb 0600 lub 0400). Odrzucane są dowiązania symboliczne ostatniego elementu ścieżki, katalogi, FIFO, puste pliki, NUL/znaki sterujące i zbyt duże pliki. Usuwane jest jedno końcowe LF lub CRLF; pozostałe bajty, w tym spacje, są zachowywane. Chroń również katalogi nadrzędne. Bufor odczytu i tymczasowe hasło aplikacji są zerowane; Connector/C może zachowywać własne kopie do zamknięcia połączenia. To hasło konta usługi MariaDB, nie użytkownika huba.

Ten etap obsługuje wyłącznie lokalne gniazdo Unix; protokół jest jawnie wymuszony, bez przejścia na TCP. Chroń gniazdo i jego katalogi uprawnieniami systemu. TCP/TLS bazy jest odłożone: Connector/C 3.4 zmienia weryfikację nazwy dla lokalnych/prywatnych adresów, a ścisła przenośna weryfikacja wymaga osobno przetestowanego adaptera. Transport ADCS jest niezależny. Komunikaty pomijają hasła, SQL, ścieżki i surowe błędy serwera.

```ini
[database]
enabled = on
socket = /run/mysqld/mysqld.sock
name = dc24h
user = dc24h
password_file = /etc/dc24h/database.password
timeout = 5
```

Pusty socket i klucze dotyczące TCP są odrzucane. Nie udostępniaj lokalnego gniazda bazy przez niezabezpieczony tunel.

## Jawna migracja podstawowa

```sh
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini --check-database
./build/dc24h --config config/dc24h.ini --migrate-database
```

Sprawdzenie bazy i migracja łączą się z SQL, ale nie uruchamiają Core, nasłuchu ani logu z konfiguracji. Kody zakończenia: 0 sukces, 1 błąd bazy, 2 błędne argumenty. Te trzy operacje wzajemnie się wykluczają; operacje bazy wymagają jej włączenia. Sprawdzenie wykonuje tylko ping; zwykły start nie zmienia schematu.

Migracja 1 tworzy tabelę InnoDB `dc24h_schema_migrations`: klucz główny `version` (INT UNSIGNED), `name` ASCII, `checksum` ASCII SHA-256 i znacznik czasu `applied_at`. Źródło to `database/migrations/0001_schema_migrations.sql`; CMake wbudowuje dokładne bajty i SHA-256, śledzi zmiany i instaluje SQL w `share/dc24h/migrations`. Program nie ładuje dowolnych plików ani nie rozdziela SQL średnikami. Nazwa migracji to `schema_migrations`.

Runner uzyskuje bez oczekiwania blokadę połączenia `GET_LOCK`: `dc24h:` i pierwsze 52 znaki szesnastkowe SHA-256 nazwy bazy. Chroni to przed równoległymi współpracującymi runnerami dc24h tego samego schematu na tym samym serwerze. Brak blokady przerywa operację; RAII zwalnia ją również po błędach. Tabela powstaje idempotentnie; wpis wersji 1 jest zapisywany dopiero po udanym DDL. Powtórne uruchomienie sprawdza wersję/nazwę/sumę i zwraca `applied=0`. Obce wersje, dodatkowe rekordy lub zmienione sumy kończą się błędem. Nie edytuj zastosowanych migracji. Weryfikacja tożsamości wpisów nie jest pełnym audytem zgodności struktury tabel.

DDL MariaDB może niejawnie zatwierdzać transakcję; brak obietnicy rollbacku. Migrację podstawową można ponowić po przerwaniu pomiędzy utworzeniem tabeli a zapisem wersji. Przyszłe migracje nieidempotentne wymagają jawnego zapisu i naprawy częściowych błędów. Przed migracją wykonaj kopię bazy i sprawdź SQL/uprawnienia. Brak automatyzacji kopii i downgrade. Źródła: [opcje Connector/C](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mysql_optionsv), [niejawne zatwierdzenia DDL](https://mariadb.com/docs/server/reference/sql-statements/transactions/sql-statements-that-cause-an-implicit-commit).

## Kontrakt adaptera

`Connection` nie jest kopiowalne; działa synchronicznie i należy do jednego wątku. Uchwyty Connector/C są prywatne. Wartości są wiązane w prepared statements; nie łącz SQL z niezaufanymi identyfikatorami. `execute()` przyjmuje instrukcje bez wyniku; `scalar()` wymaga jednego wiersza/kolumny do 4096 bajtów, SQL NULL daje `nullopt`. SQL i pojedyncze parametry mają limit 64 KiB. Nie włączono multi-statements ani LOCAL INFILE. Zamknięcie instrukcji zwalnia zasoby również po błędzie przygotowania/wyniku. Brak ogólnego API pobierania nieograniczonych zbiorów wierszy.
