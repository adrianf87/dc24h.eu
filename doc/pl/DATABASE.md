# MariaDB — alpha-0.7.2

**alpha-0.7.11 Account Management:** [zaufana zmiana hasła, wyłączenie i reset](ACCOUNT_MANAGEMENT.md) łączą atomowy zapis konta/haseł ze świeżym potwierdzeniem, rewizją i usunięciem danych ADC. Wykonuj przy zatrzymanym hubie, następnie jawnie zapisz ADC/aktywuj konto według potrzeby i uruchom hub ponownie. Bez przeładowania na żywo, nowej migracji SQL i publicznego API zarządzania.

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) wdraża logowanie kont GPA/PAS, osobne hasło ADC szyfrowane AES-256-GCM i prywatny klucz poza bazą, zachowując scrypt. Wymagane są jawna migracja v6, aktywne konto i dokładna para nick/CID; domyślnie obowiązuje ADCS. Zmiany i blokady w katalogu startowym wymagają restartu. Publiczne przygotowanie kont, reset, rotacja i nadawanie ról pozostają kolejnymi etapami. Ten wpis zastępuje wcześniejsze opisy planowanego uwierzytelniania.

**Aktualny etap: alpha-0.7.6.** [Identity Mapping](IDENTITY_MAPPING.md) dodaje `dc24h_account_identities` przez jawną migrację v5. Konta, weryfikatory i migracje 1–4 pozostają bez zmian. Historyczne numery schematów poniżej opisują zakres wcześniejszych etapów.

**Aktualny etap — alpha-0.7.5:** [Rejestracja](REGISTRATION.md) atomowo tworzy konta registered/disabled i osobno przechowywane weryfikatory haseł OpenSSL scrypt z losową solą. Migracja v4 dodaje poświadczenia; walidacja Unicode, stały koszt KDF i rollback zapobiegają częściowym kontom. Uwierzytelnianie sesji i publiczna rejestracja API/WebAdmin pozostają planowane. CI zachowuje przypięty workflow Proxmox Ubuntu 24.04/22.04 bez scalania github-runner-v0.1.

**Wersjonowanie schematu — alpha-0.7.2:** [Migracje bazy danych](DATABASE_MIGRATIONS.md) opisują uporządkowany katalog, aktualizację v1, odczyt stanu i obsługę częściowych błędów.

**Core dostarczony w alpha-0.7.1:** [Database Core](DATABASE_CORE.md) dodaje typowane parametry, ograniczone wyniki na własność, metadane zapisu, egzekwowanie wątku właściciela, zredagowane błędy i transakcje RAII. Niezmienna migracja 1 i konfiguracja operatora poniżej pozostają; wymagane przebudowanie zależnych komponentów.

Zaimplementowano opcjonalne połączenie Connector/C, RAII, ograniczenia czasu, kodowanie `utf8mb4`, zapytania parametryzowane i jawną migrację rejestru wersji. Zapis metadanych kont i klucze roli/stanu udostępnia [Repozytorium kont](ACCOUNT_REPOSITORY.md). [Rejestracja](REGISTRATION.md) dodaje osobne weryfikatory scrypt z solą przy użyciu istniejącego OpenSSL oraz atomową transakcję konta i poświadczeń. Uwierzytelnianie, mapowanie uprawnień sesji, konfiguracja w bazie, bany, dane administracyjne i zapisywanie zdarzeń pozostają planowane. Jawne hasła użytkowników huba nie są zapisywane. WebAdmin będzie używał API C++, nie bezpośredniego SQL.

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

`--migrate-database` stosuje teraz uporządkowany kompilowany katalog do v4, zachowuje dokładną sumę v1 i dodaje kolumnę metadanych `state` i tabele kont/poświadczeń. `--database-status` odczytuje current/latest/pending/dirty bez DDL; zwykły start nigdy nie migruje. Poświadczenia inicjalizacji/aktualizacji wymagają dodatkowo ALTER. Przyszłe migracje po inicjalizacji zapisują `applying` przed SQL i odmawiają automatycznego odtwarzania po częściowym błędzie. Pełny kontrakt, manifest, format sum, uprawnienia i naprawa przez operatora: [Migracje bazy danych](DATABASE_MIGRATIONS.md).

## Kontrakt adaptera

`Connection` nie jest kopiowalne; działa synchronicznie i należy do jednego wątku. Uchwyty Connector/C są prywatne. Wartości są wiązane w prepared statements; nie łącz SQL z niezaufanymi identyfikatorami. `execute()` przyjmuje instrukcje bez wyniku; `scalar()` wymaga jednego wiersza/kolumny do 4096 bajtów, SQL NULL daje `nullopt`. SQL i pojedyncze parametry mają limit 64 KiB. Nie włączono multi-statements ani LOCAL INFILE. Zamknięcie instrukcji zwalnia zasoby również po błędzie przygotowania/wyniku. Brak ogólnego API pobierania nieograniczonych zbiorów wierszy.
