# Trwały zapis w MariaDB w wersji Alpha 0.1

Na Debianie lub Ubuntu zainstaluj serwer bazy danych i pakiet deweloperski biblioteki klienckiej.

```sh
sudo apt-get install mariadb-server mariadb-client libmariadb-dev
```

Hub łączy się z bazą przez lokalne gniazdo uniksowe. Na instancji MariaDB przeznaczonej wyłącznie dla huba dodaj poniższe ustawienie do `/etc/mysql/mariadb.conf.d/60-dc24h-local.cnf` i uruchom ponownie MariaDB, aby wyłączyć nasłuch TCP.

```ini
[mariadbd]
skip-networking
```

## Baza danych i konta

W sesji administratora MariaDB utwórz bazę danych i trzy lokalne konta. Zastąp tekst w nawiasach ostrych osobno wygenerowanymi hasłami. Możesz uruchomić klienta poleceniem `sudo env MYSQL_HISTFILE=/dev/null mariadb --protocol=socket`, aby polecenia tworzenia kont nie trafiły do historii klienta.

```sql
CREATE DATABASE dc24h CHARACTER SET utf8mb4;
CREATE USER 'dc24h_migrate'@'localhost' IDENTIFIED BY '<hasło migracji>';
CREATE USER 'dc24h_hub'@'localhost' IDENTIFIED BY '<hasło huba>';
CREATE USER 'dc24h_admin'@'localhost' IDENTIFIED BY '<hasło operatora>';
GRANT CREATE, SELECT, INSERT ON dc24h.* TO 'dc24h_migrate'@'localhost';
```

Ustaw w powłoce zmienne połączenia dla konta migracyjnego. Wskaż w `DC24H_DB_PASSWORD_FILE` zwykły plik dostępny wyłącznie dla jego właściciela, np. z trybem `0600` lub `0400`. Zapisz w nim jedno hasło. Jeżeli MariaDB korzysta z innego gniazda niż `/run/mysqld/mysqld.sock`, ustaw `DC24H_DB_SOCKET`.

```sh
export DC24H_DB_NAME=dc24h
export DC24H_DB_USER=dc24h_migrate
export DC24H_DB_PASSWORD_FILE=/ścieżka/do/prywatnego/hasła-migracji
./build/debug/dc24h-hub --db migrate
./build/debug/dc24h-hub --db status
```

Po wykonaniu migracji nadaj hubowi prawo odczytu, a operatorowi uprawnienia potrzebne do obsługi poleceń administracyjnych. Uprawnienie `INSERT` konta migracyjnego można teraz ograniczyć do tabeli historii migracji.

```sql
REVOKE INSERT ON dc24h.* FROM 'dc24h_migrate'@'localhost';
GRANT INSERT ON dc24h.schema_migrations TO 'dc24h_migrate'@'localhost';
GRANT SELECT ON dc24h.schema_migrations TO 'dc24h_hub'@'localhost', 'dc24h_admin'@'localhost';
GRANT SELECT ON dc24h.hub_settings TO 'dc24h_hub'@'localhost';
GRANT SELECT ON dc24h.bans TO 'dc24h_hub'@'localhost';
GRANT SELECT, INSERT, UPDATE ON dc24h.hub_settings TO 'dc24h_admin'@'localhost';
GRANT SELECT, INSERT, DELETE ON dc24h.bans TO 'dc24h_admin'@'localhost';
GRANT SELECT, INSERT ON dc24h.audit_events TO 'dc24h_admin'@'localhost';
```

Uruchamiaj hub na osobnym koncie systemowym z prawem odczytu pliku hasła huba i plików TLS. Po utworzeniu konta systemowego `dc24h` możesz użyć polecenia z katalogu, do którego to konto ma dostęp.

```sh
sudo -u dc24h -- env \
  DC24H_DB_NAME=dc24h DC24H_DB_USER=dc24h_hub \
  DC24H_DB_PASSWORD_FILE=/etc/dc24h/secrets/hub-db \
  /usr/local/bin/dc24h-hub --serve --mariadb \
  --tls-cert /etc/dc24h/tls/fullchain.pem --tls-key /etc/dc24h/tls/key.pem
```

## Migracje

Przy wdrożeniu lub aktualizacji uruchom `--db migrate` na koncie migracyjnym, a potem sprawdź `--db status`. Etap E5 wymaga wersji 3. Blokada przypisana do bazy danych dopuszcza tylko jedną migrację naraz. Tworzenie tabel można powtórzyć. Przed zapisaniem numeru wersji migrator sprawdza silnik InnoDB i dostępność wymaganych kolumn. MariaDB zatwierdza polecenia DDL niejawnie, więc po przerwaniu migrator może sprawdzić już utworzoną tabelę i zapisać jej wersję. Ponowna migracja zachowuje istniejące wiersze. Nieznana lub niespójna historia migracji wymaga sprawdzenia przez operatora. [Blokady w MariaDB](https://mariadb.com/docs/server/reference/sql-functions/secondary-functions/miscellaneous-functions/get_lock), [Niejawne zatwierdzanie w MariaDB](https://mariadb.com/docs/server/reference/sql-statements/transactions/sql-statements-that-cause-an-implicit-commit)

| Tabela | Zawartość |
| --- | --- |
| `schema_migrations` | Wersja, nazwa i czas wykonania UTC |
| `hub_settings` | Nazwa ustawienia, wartość i czas aktualizacji UTC |
| `bans` | ID, CID lub IP, powód, czas utworzenia UTC, opcjonalny czas wygaśnięcia i wykonawca |
| `audit_events` | ID, typ zdarzenia, czas UTC, opcjonalny wykonawca i ID obiektu oraz szczegóły o ustalonej treści |

## Ustawienia

Polecenia administracyjne uruchamiaj z `DC24H_DB_USER=dc24h_admin` i prywatnym plikiem hasła tego konta.

```sh
./build/debug/dc24h-hub --db settings set hub-name 'Przykładowy hub' --actor operator.1
./build/debug/dc24h-hub --db settings set hub-description 'Lokalny hub ADC i ADCS'
./build/debug/dc24h-hub --db settings set max-clients 256
./build/debug/dc24h-hub --db settings list
```

Po uruchomieniu z `--serve --mariadb` hub odczytuje ustawienia przed startem wątków sieciowych. Podana jawnie opcja uruchomienia ma pierwszeństwo przed wartością z bazy. Dla brakującego lub nieprawidłowego wpisu obowiązuje pierwotna wartość domyślna, chyba że podano odpowiednią opcję. Adresy nasłuchu, porty i ścieżki plików TLS ustawia się dotychczasowymi opcjami `--serve`.

| Klucz ustawienia i odpowiadająca mu opcja uruchomienia | Dopuszczalne wartości |
| --- | --- |
| `hub-name` | 1–128 bajtów tekstu UTF-8 w postaci NFC |
| `hub-description` | 0–255 bajtów tekstu UTF-8 w postaci NFC |
| `max-clients`, `input-bytes`, `pending-bytes` | Dodatnie liczby całkowite do 1048575 klientów lub 1048576 bajtów |
| `max-nick-codepoints`, `max-message-bytes` | Dodatnie liczby całkowite do 256 punktów kodowych lub 4096 bajtów |
| `setup-timeout-ms`, `tls-timeout-ms`, `identify-timeout-ms`, `idle-timeout-ms`, `shutdown-timeout-ms` | 1–86400000 milisekund |

Zmiany ustawień obowiązują po ponownym uruchomieniu huba. Bez `--mariadb` hub korzysta z opcji uruchomienia i wartości domyślnych oraz pomija sprawdzanie banów z bazy.

## Bany i audyt

```sh
./build/debug/dc24h-hub --db bans add ip 192.0.2.25 3600 'Powtarzający się spam' --actor operator.1
./build/debug/dc24h-hub --db bans add ip 2001:db8::10 permanent 'Nadużycie'
./build/debug/dc24h-hub --db bans list
./build/debug/dc24h-hub --db bans remove 1 --actor operator.1
./build/debug/dc24h-hub --db audit list
```

Wybierz `cid` dla identyfikatora ADC w postaci kanonicznej lub `ip` dla pojedynczego adresu IPv4 albo IPv6. Adres IPv6 mapujący IPv4 jest równoważny postaci IPv4. Czas trwania podaj w dodatnich sekundach lub użyj `permanent`. Wygasły ban pozostaje na liście i przestaje blokować logowanie. Listy banów i audytu zwracają do 100 wierszy. Aby pobrać kolejną stronę, podaj ostatni identyfikator liczbowy jako `AFTER_ID`.

Oba nasłuchy sprawdzają zweryfikowany CID i adres drugiego końca gniazda przed dodaniem użytkownika do listy obecności. Ban bezterminowy ma pierwszeństwo przed czasowym. W przypadku bana czasowego klient otrzymuje liczbę pozostałych sekund. Zmiany banów dotyczą nowych logowań. Połączeni użytkownicy zachowują dostęp do końca sesji. [Protokół ADC](https://adc.sourceforge.io/ADC.html)

Udana zmiana i jej zdarzenie audytowe są zatwierdzane w jednej transakcji. Zdarzenie zawiera nazwę ustawienia lub ID bana oraz szczegóły o ustalonej treści. Błędy wykryte podczas operacji administracyjnych są zapisywane, jeśli baza przyjmuje zapis. Opcjonalny identyfikator wykonawcy może zawierać do 64 liter ASCII, cyfr, kropek, łączników, podkreśleń lub znaków `@`. Używaj jawnych identyfikatorów operatorów. Wartości ustawień, powody banów, dane sesji klienta i poświadczenia nie trafiają do szczegółów audytu.

## Kopia zapasowa i odtwarzanie

Użyj konta z uprawnieniem `SELECT` do czterech tabel. Klient poprosi o hasło. Przechowuj zrzuty w prywatnej lokalizacji, ponieważ zawierają identyfikatory użytkowników i powody banów. Migracje planuj poza oknem wykonywania kopii.

```sh
umask 077
mariadb-dump --protocol=socket --user=dc24h_backup --password \
  --single-transaction --skip-lock-tables --no-tablespaces --skip-triggers \
  --skip-add-drop-table --skip-add-locks --skip-disable-keys \
  dc24h > dc24h-backup.sql
```

W sesji administratora utwórz nową bazę poleceniem `CREATE DATABASE dc24h_restore CHARACTER SET utf8mb4;`. Nadaj tymczasowemu kontu odtwarzania uprawnienia `CREATE` i `INSERT` w tej bazie, a następnie zaimportuj zrzut.

```sh
mariadb --protocol=socket --user=dc24h_restore --password dc24h_restore < dc24h-backup.sql
```

Nadaj uprawnienia do tabel odtworzonej bazy, uruchom `--db status` i sprawdź zapisane ustawienia, bany oraz audyt, zanim przełączysz hub na tę bazę. Jeżeli kopia zawiera starszą wersję schematu, wykonaj migracje. Po weryfikacji usuń konto odtwarzania.

## Rozwiązywanie problemów

- Przy błędach połączenia lub dostępu sprawdź adres gniazda oraz uprawnienia konta użytego do danego polecenia. Hub podaje nazwę operacji i kod błędu MariaDB.
- Przy brakującym lub niepełnym schemacie uruchom `--db migrate` na koncie migracyjnym i ponów operację. Przed powtórzeniem nieudanej migracji usuń przyczynę niezgodności istniejącej tabeli.
- Gdy odczyt ustawień przy uruchomieniu się nie powiedzie, hub stosuje jawne opcje oraz wartości domyślne do końca działania procesu. Po odzyskaniu dostępu zrestartuj hub, by załadować zapisane ustawienia.
- Hub z włączoną obsługą MariaDB odrzuca nowe logowania tymczasowym błędem, gdy baza jest niedostępna, sprawdzenie trwa zbyt długo albo kolejka kontroli banów jest pełna. Dotychczasowe sesje ADC i ADCS mogą dalej działać. Po krótkiej przerwie kolejne logowanie może wznowić połączenie z bazą. Wątek zapytań używa jednego połączenia i kolejki na 32 sprawdzenia. Operacje wejścia i wyjścia biblioteki klienckiej mają limity dwóch sekund, a MariaDB stosuje sekundowe limity wykonania zapytania i oczekiwania na blokadę.
- Jeżeli połączenie zostało utracone w trakcie zatwierdzania operacji administracyjnej, przed ponowieniem sprawdź odpowiednią listę oraz wpisy audytu.

Aby wykonać odizolowane testy integracyjne, zainstaluj `openssl` i użyj poleceń `cmake --preset debug -DDC24H_TEST_MARIADB=ON`, `cmake --build --preset debug` i `ctest --preset debug -L mariadb`. Testy uruchamiają osobną instancję MariaDB w katalogu tymczasowym.
