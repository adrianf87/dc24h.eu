# Migracje bazy danych — alpha-0.7.2

**alpha-0.8.9 Dziennik audytu:** [Historia udanych działań konserwacyjnych](AUDIT_LOGS.md) jest zapisywana w schemacie v9. Aktywne konto z uprawnieniem audit.read wyświetla tę historię. Kick i wyciszenie na żywo, odmowy, hasła oraz dowolny tekst powodu pozostają poza zapisem. Zapis konfiguracji i administracja huba pozostają w planach.

**alpha-0.8.5 Ban Core:** [Trwałe blokady wejścia kont/CID/IP](BAN_CORE.md) dodają jawny schemat v8, dodawanie/cofanie z hasłem, egzekwowanie ze startu i wygaśnięcie. Admin Core ma dziesięć poleceń; users.ban/blocks.manage działają. Zastępuje wcześniejsze opisy v7/planowanych banów.

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) wdraża logowanie kont GPA/PAS, osobne hasło ADC szyfrowane AES-256-GCM i prywatny klucz poza bazą, zachowując scrypt. Wymagane są jawna migracja v6, aktywne konto i dokładna para nick/CID; domyślnie obowiązuje ADCS. Zmiany i blokady w katalogu startowym wymagają restartu. Publiczne przygotowanie kont, reset, rotacja i nadawanie ról pozostają kolejnymi etapami. Ten wpis zastępuje wcześniejsze opisy planowanego uwierzytelniania.

Aktualny katalog: v1 metadane, v2 stan, v3 [zapis kont](ACCOUNT_REPOSITORY.md), v4 [poświadczenia](REGISTRATION.md). Pusta baza stosuje 4 wersje; aktualizacja z v1 stosuje 3, z v2 dwie, a z v3 jedną. Poniżej opisano pierwotny etap alpha-0.7.2.

Podstawa: main `4f204b9f8901a5201c56abad957543344c177256` zawierający Database Core alpha-0.7.1. Etap wprowadza wersjonowanie schematu SQL przez uporządkowany katalog kompilowany, zachowuje pierwotną migrację bazową, aktualizuje metadane oraz dodaje odczyt stanu i wykrywanie przerwanych zmian. Bez tabel aplikacyjnych/kont/uwierzytelniania i zapisów z WebAdmin.

## Polecenia operatora

```sh
./build/dc24h --config config/dc24h.ini --check-database
./build/dc24h --config config/dc24h.ini --database-status
./build/dc24h --config config/dc24h.ini --migrate-database
```

Wymagane włączone ustawienia `[database]` i przygotowana lokalna baza/konto MariaDB zgodnie z [opisem bazy](DATABASE.md). Polecenia łączą się synchronicznie przez gniazdo Unix i kończą bez otwierania portów huba, uruchamiania Core i pliku logu. Wzajemnie wykluczają się z sobą, `--check-config`, pomocą i wersją. Domyślny język US; `--language pl` tłumaczy komunikaty. Hasła bazy nie należą do argumentów ani źródeł SQL.

`--check-database` tylko pinguje. `--database-status` odczytuje wyłącznie metadane migracji, bez DDL, zapisów i blokady doradczej. Pusta baza zwraca `version=0 latest=4 pending=4 dirty=no`; wcześniejsza v1 — `version=1 latest=4 pending=3 dirty=no`; aktualna — `version=4 latest=4 pending=0 dirty=no`. Dirty oznacza znaną migrację rozpoczętą bez potwierdzonego zakończenia. Niepoprawna/nieznana historia kończy się błędem zamiast zgadywania wersji. Status jest obserwacją, nie transakcyjną migawką podczas równoległego DDL.

`--migrate-database` stosuje oczekujący końcowy fragment katalogu i podaje liczbę nowych wersji: `Database migrations applied=4` dla pustej bazy, `applied=3` dla aktualizacji v1 i `applied=0` dla aktualnej bazy (prefiks komunikatu jest tłumaczony w PL). Kody zakończenia: 0 przy poprawnym odczycie/zastosowaniu (czyste oczekujące migracje nie powodują błędu statusu), 1 przy błędzie bazy/metadanych/dirty, 2 przy niepoprawnych argumentach/ustawieniach. Odczyt dirty zwraca 1. Zwykły start huba i `--check-config` nie sprawdzają ani nie zmieniają automatycznie historii; API `database.enabled` nadal opisuje konfigurację, nie schemat i zdrowie SQL.

Używaj osobnych poświadczeń operatora. Obecna ścieżka instalacji/aktualizacji wymaga SELECT, CREATE, ALTER i INSERT w wybranym schemacie. UPDATE będzie dodatkowo potrzebny do zakończenia przyszłych migracji z dziennikiem; dalsze uprawnienia wynikają z przyszłego SQL. Konto SELECT-only odczytuje aktualny/starszy schemat i wykonuje no-op aktualnej bazy, ale nie tworzy ani nie aktualizuje metadanych. Runner nie tworzy kont, baz ani grantów. Planuj migracje przy zatrzymanej aplikacji i z kopią zarządzaną przez operatora; bez koordynatora zmian na żywo, kopii, cofania i polecenia naprawy.

## Katalog i dokładne sumy kontrolne

`database/migrations/manifest.json` jawnie określa kolejne całkowite wersje od 1, nazwy i uporządkowane pliki SQL. CMake sprawdza tożsamość/kolejność/nazwy/rozmiary, odczytuje dokładne bajty, liczy SHA-256 i osadza niezmienne instrukcje oraz metadane. Śledzi manifest/wymienione źródła na potrzeby rekonfiguracji i instaluje te źródła z manifestem w `share/dc24h/migrations`. Niewymienione SQL, duplikaty plików, wyjście poza katalog, symlinki, brak pliku i kolizje delimitera literału C++ przerywają konfigurację. Runtime ponownie sprawdza katalog przed odczytem historii.

| Wersja | Nazwa | SQL | Zachowanie |
| --- | --- | --- | --- |
| 1 | `schema_migrations` | `0001_schema_migrations.sql` | Pierwotny niezmieniony idempotentny rejestr InnoDB |
| 2 | `migration_state` | `0002_migration_state.sql` | Idempotentnie dodaje kolumnę ASCII `state`, domyślnie `applied` |

Pozostają cztery pierwotne pola: nieujemny `version` jako klucz główny, ASCII z binarną kolacją `name` i SHA-256 `checksum` oraz niepusty znacznik czasu `applied_at`. Wersja 2 dodaje `state VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'applied'`. Tożsamość, suma i czas v1 pozostają nietknięte. Zastosowane wpisy muszą odpowiadać bajtowo katalogowi i tworzyć ciągły początek listy. Luki, nieznane/nowsze wersje, zmiany nazw/sum, niepoprawne stany i niezgodna struktura metadanych przerywają pracę przed DDL. Kontrola obejmuje silnik, kolejność/typy/nullowalność/kolacje ASCII kolumn i klucz główny wersji; nie jest pełnym audytem wszystkich domyślnych wartości, indeksów, triggerów i schematu aplikacji.

Dla jednego pliku suma to SHA-256 jego dokładnych bajtów, zgodnie z konwencją v1. Dla wielu plików haszowana jest uporządkowana konkatenacja dziesiętnej długości każdego pliku, dwukropka i jego bajtów: `len1:bytes1len2:bytes2...`. Długości liczą bajty UTF-8. Wymagany tekst LF bez NUL/CR; konfiguracja odrzuca CRLF, ponieważ kompilator C++ normalizuje je wewnątrz literału. Zaakceptowane bajty nie są normalizowane; bez dzielenia tokenów SQL i średników. Cudzysłowy, średniki, `@name@` i zapis podobny do CMake pozostają bajtami SQL. Każdy plik jest jedną instrukcją przygotowywaną przez serwer; sterownik odrzuca wiele instrukcji w jednym pliku. Nie umieszczaj poświadczeń i SQL wybieranego przez klienta w katalogu. Zaufany SQL migracji nie może zmieniać wybranej bazy, ustawień transakcji sesji ani samego rejestru migracji.

Rozszerzenie polega na dodaniu N+1 z nową nazwą i jawnymi plikami (`0003_name.sql` lub kolejno `0003_name_01.sql`, `0003_name_02.sql` itd.), przeglądzie, przebudowie i testach aktualizacji/awarii na bazie testowej. Nie edytuj zastosowanego SQL ani jego zapisanej sumy. Limity: 2..128 wpisów, 1..32 pliki na wpis, 1..65536 bajtów na plik; nazwy z małych liter ASCII, cyfr i podkreśleń, zaczynające się literą. Runtime używa kompilowanego katalogu i nie odczytuje skryptów z katalogu roboczego ani zainstalowanych danych. Bez CLI dowolnego skryptu, wyboru docelowej wersji i migracji w dół.

## Blokady, inicjalizacja i częściowe błędy

Zachowano nazwę blokady: `dc24h:` plus pierwsze 52 cyfry szesnastkowe SHA-256 `DATABASE()`. Zerowy czas oczekiwania, połączeniowy `GET_LOCK` porządkuje współpracujące runnery na tym samym serwerze/schemacie; zajętość i rekurencyjne posiadanie kończą się natychmiastowym błędem. Własność jest sprawdzana przed instrukcjami i po wykonaniu. Sprzątanie zwalnia blokadę po sukcesie/błędzie; utrata połączenia zwalnia ją na serwerze. Narzędzia administratora i aplikacje ignorujące blokadę nie są nią synchronizowane.

Tylko niezmienne migracje inicjalizacyjne 1 i 2 można automatycznie wznowić: v1 `CREATE TABLE IF NOT EXISTS` i v2 `ADD COLUMN IF NOT EXISTS` mogą się powtórzyć po przerwaniu między DDL a zapisem tożsamości. Poprawnie zbudowany pusty stary rejestr może zakończyć v1; pusty rozszerzony rejestr jest odrzucany jako potencjalnie usunięta historia. Poprawna v1 z istniejącą już kolumną state może zakończyć lukę v2. Wyjątki nie dopuszczają nieznanej historii i niezgodnych kolumn. Brak uprawnień pozostawia zastosowany początek bez zmian.

Od wersji 3 ogólny runner najpierw zapisuje automatycznie zatwierdzaną tożsamość `applying`, potem wykonuje po kolei jawne instrukcje SQL. Dopiero po pełnym sukcesie ustawia `applied` i `applied_at=CURRENT_TIMESTAMP`; wcześniej czas oznacza początek próby, nie jej zakończenie. Błędy SQL/alokacji i brak odpowiedzi mogą pozostawić częściowe skutki i trwały wpis `applying`. Odczyt zwraca dirty, a migracja odmawia kontynuacji i odtwarzania instrukcji. Dostarczony katalog obejmuje tabelę kont w v3; niezależne katalogi testowe sprawdzają także trwałe częściowe błędy wersjami 3/4.

MariaDB DDL może niejawnie zatwierdzać, więc objęcie instrukcji transakcją nie zapewnia atomowego cofania migracji. Migracja/status wewnątrz transakcji Database Core jest odrzucana przed SQL/blokadą. Błąd transportu zamyka połączenie bez reconnect/replay; brak potwierdzenia zakończenia wymaga nowego połączenia i odczytu. Wyjątek nie dowodzi braku skutków SQL. Źródła: [niejawne zatwierdzanie MariaDB](https://mariadb.com/docs/server/reference/sql-statements/transactions/sql-statements-that-cause-an-implicit-commit), [GET_LOCK](https://mariadb.com/docs/server/reference/sql-functions/secondary-functions/miscellaneous-functions/get_lock), [ALTER TABLE](https://mariadb.com/docs/server/reference/sql-statements/data-definition/alter/alter-table).

Po błędzie zachowaj bazę/historię i sprawdź dokładną wersję oraz rzeczywisty schemat narzędziami operatora. Przed wznowieniem przywróć kopię lub uzgodnij częściowe skutki i zapis tożsamości według zatwierdzonej procedury naprawy. CLI nie usuwa automatycznie dirty, nie podmienia sum, nie oznacza ręcznie jako zastosowane i nie cofa. Starszy program odrzuca nowszy rejestr. Komunikaty są stałym tekstem EN/US lub PL bez SQL, prywatnych ścieżek, wartości i surowych błędów MariaDB.

## Weryfikacja

Testy kompilują rzeczywiste katalogi CMake: dokładne hasze jednego/wielu plików, UTF-8, literały SQL, błędna kolejność/ścieżki, brak/niewymienione/zdublowane źródła, symlinki oraz granice rozmiaru/delimitera oraz odrzucanie NUL/CRLF przed normalizacją przez kompilator. Testy natywne sprawdzają publiczne metadane i niepoprawne katalogi. Testy tymczasowej MariaDB obejmują pustą instalację/no-op, aktualizację ze starej wersji i zachowanie sumy/czasu v1, przerwaną inicjalizację, uszkodzenia historii/struktury, odczyt read-only/brak uprawnień, blokady i rekurencję, transakcję zewnętrzną, wieloplikowy SQL, trwałe częściowe błędy, zabite połączenia i odmowę odtwarzania. Baza hosta nie jest używana ani zmieniana.

Włączony hub nadal przechodzi prawdziwy login/reconnect/chat ncdc po ADC i ADCS z prywatną bazą testową. Zachowano testy protokołu, API, Apache i WebAdmin. Faktyczne lokalne wyniki: [Testowanie](TESTING.md) i [historia zmian](../../CHANGELOG.md). CI GitHub pozostaje wyłącznie na hostowanym Ubuntu 22.04/24.04; bez oczekiwania na zdalny Check status.
