# MariaDB — alpha-0.7.2

**Wersjonowanie schematu — alpha-0.7.2:** [Migracje bazy danych](DATABASE_MIGRATIONS.md) opisują uporządkowany katalog, aktualizację v1, odczyt stanu i obsługę częściowych błędów.

**Core dostarczony w alpha-0.7.1:** [Database Core](DATABASE_CORE.md) dodaje typowane parametry, ograniczone wyniki na własność, metadane zapisu, egzekwowanie wątku właściciela, zredagowane błędy i transakcje RAII. Niezmienna migracja 1 i konfiguracja operatora poniżej pozostają; wymagane przebudowanie zależnych komponentów.

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

Utwórz bazę i osobne konto narzędziami administratora MariaDB. Nie uruchamiaj usługi na koncie SQL root. Konto wykonawcze powinno mieć `SELECT` w wybranym schemacie (jego wybór wymaga uprawnień); jawna migracja dodatkowo wymaga `CREATE`, `ALTER` i `INSERT`. Dla przyszłych migracji stosuj osobne dane operatora. Program nie tworzy automatycznie bazy, użytkowników ani uprawnień.

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

## Jawne wersjonowanie schematu

`--migrate-database` stosuje teraz uporządkowany kompilowany katalog do v2, zachowuje dokładną sumę v1 i dodaje kolumnę metadanych `state`. `--database-status` odczytuje current/latest/pending/dirty bez DDL; zwykły start nigdy nie migruje. Poświadczenia inicjalizacji/aktualizacji wymagają dodatkowo ALTER. Przyszłe migracje po inicjalizacji zapisują `applying` przed SQL i odmawiają automatycznego odtwarzania po częściowym błędzie. Pełny kontrakt, manifest, format sum, uprawnienia i naprawa przez operatora: [Migracje bazy danych](DATABASE_MIGRATIONS.md).

## Kontrakt adaptera

`Connection` nie jest kopiowalne; działa synchronicznie i należy do jednego wątku. Uchwyty Connector/C są prywatne. Wartości są wiązane w prepared statements; nie łącz SQL z niezaufanymi identyfikatorami. `execute()` przyjmuje instrukcje bez wyniku; `scalar()` wymaga jednego wiersza/kolumny do 4096 bajtów, SQL NULL daje `nullopt`. SQL i pojedyncze parametry mają limit 64 KiB. Nie włączono multi-statements ani LOCAL INFILE. Zamknięcie instrukcji zwalnia zasoby również po błędzie przygotowania/wyniku. Brak ogólnego API pobierania nieograniczonych zbiorów wierszy.
