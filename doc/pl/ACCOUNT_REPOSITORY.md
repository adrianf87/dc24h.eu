# Repozytorium kont — alpha-0.7.4

**alpha-0.7.11 Account Management:** [zaufana zmiana hasła, wyłączenie i reset](ACCOUNT_MANAGEMENT.md) łączą atomowy zapis konta/haseł ze świeżym potwierdzeniem, rewizją i usunięciem danych ADC. Wykonuj przy zatrzymanym hubie, następnie jawnie zapisz ADC/aktywuj konto według potrzeby i uruchom hub ponownie. Bez przeładowania na żywo, nowej migracji SQL i publicznego API zarządzania.

Poniżej zachowano historyczny zakres alpha-0.7.4. [Rejestracja](REGISTRATION.md) łączy teraz zapis konta i poświadczeń w jednej prywatnej transakcji; publiczne operacje repozytorium nadal zarządzają własną transakcją.

Podstawa: bezpośrednio main `5947add6c5b6e318841c612c93b9cb5cf5eae061`. Zachowano istniejące etapy alpha-0.7.1 Database Core, alpha-0.7.2 Migracje i alpha-0.7.3 Model konta.

## Zakres i API

Biblioteka C++20 `dc24h_account_repository` łączy zweryfikowany [Model konta](ACCOUNT_MODEL.md) z [Database Core](DATABASE_CORE.md). Interfejs: `include/dc24h/accounts/repository.hpp`. Cel CMake udostępnia obie zależne biblioteki. Repozytorium pożycza połączenie, które musi istnieć dłużej i pozostać na swoim wątku. Wywołania są synchroniczne i powinny działać poza reaktorem Network. Konstruktor nie wykonuje SQL. Każda operacja sprawdza aktualną, czystą historię migracji oraz silnik, kolumny i indeksy ID/nazwy. Brakujący/oczekujący/brudny schemat jest odrzucany; migracje są wyłącznie jawne. Błędy historii/metadanych mogą również propagować jako `database::MigrationError`.

| Operacja | Kontrakt |
| --- | --- |
| `insert(NewAccount)` | Weryfikuje nazwę, rolę/stan i jawne daty UTC przed SQL. MariaDB nadaje dodatnie ID signed BIGINT; po commit zwraca własny `StoredAccount {account, revision=1}`. Domyślnie registered, disabled, daty utworzenia/zmiany równe epoce, brak logowania. |
| `find_by_id(AccountId)` | Opcjonalna własna migawka; nieistniejący rekord daje `nullopt`. |
| `find_by_name(AccountName)` | Dokładne **bajty** UTF-8 NFC. `Alice` i `alice` mogą współistnieć; podobne wizualnie znaki pozostają różnymi nazwami. Nazwa po przeniesieniu jest odrzucana. |
| `update(expected, replacement)` | Blokuje i weryfikuje bieżący rekord, porównuje całą oczekiwaną migawkę i rewizję, zapisuje metadane z revision+1. Brak lub zmiana rekordu daje `stale`. ID/data utworzenia są stałe; data zmiany i istniejącego logowania nie mogą maleć, a istniejącego logowania nie można usunąć. Zapis identycznych danych również zwiększa rewizję. |

Daty podaje wywołujący, również przyszłe do roku 9999; repozytorium nie czyta zegara i nie potwierdza faktycznego logowania. Odczyt i zapis odrzucają istniejącą transakcję wywołującego. Każdy zapis posiada własną transakcję RAII. Wynik powstaje przed commit, a przenoszenie nie alokuje. Błędy walidacji, duplikatu, nieaktualności, odczytu lub alokacji przed commit powodują rollback przy wyjściu. ID mogą mieć luki i nie identyfikują sesji/połączeń.

`RepositoryErrorKind` rozróżnia `invalid_account`, `duplicate`, `stale`, `invalid_record`, `schema`; komunikaty EN/US nie zawierają danych wejściowych. Duplikat 1062 jest mapowany tylko przy pewnym błędzie ograniczenia. Inne błędy Database Core zachowują kategorię, operację, kod i `outcome_unknown`. Utrata potwierdzenia zapisu/commit może oznaczać wykonany zapis: należy ponownie połączyć się, sprawdzić stan i uzgodnić wynik, bez automatycznego ponawiania. Konflikt wymaga ponownego odczytu i decyzji wywołującego.

## Schemat i aktualizacja

Migracja `0003_accounts.sql` tworzy `dc24h_accounts` InnoDB: klucz signed BIGINT AUTO_INCREMENT, pełny unikalny `VARBINARY(64)` nazwy, binarne klucze roli/stanu, signed BIGINT sekund UTC, opcjonalne ostatnie logowanie i dodatnią rewizję signed BIGINT. Nazwy nie podlegają równoważności językowej, wielkości liter ani dopełnienia. CHECK pilnują długości, roli/stanu, dat i rewizji; pełna walidacja UTF-8/NFC oraz ID działa także po odczycie w C++. CHECK na ID AUTO_INCREMENT nie jest przenośny w MariaDB; ID wygenerowane i zaimportowane sprawdza adapter. Nie ma poświadczeń. Rewizję 9223372036854775807 można odczytać, lecz nie zwiększyć.

Wykonaj kopię zapasową i zatrzymaj zapisy przed jawną migracją z poświadczeniami operatora:

```sh
./build/dc24h --config config/dc24h.ini --database-status
./build/dc24h --config config/dc24h.ini --migrate-database
./build/dc24h --config config/dc24h.ini --database-status
```

Pusta baza stosuje 3 wersje, v1 stosuje 2, a v2 jedną. Stan docelowy: `version=3 latest=3 pending=0 dirty=no`. Bajty zastosowanych migracji 1/2 pozostają niezmienione. v3 zapisuje trwałe `applying` przed DDL; częściowy błąd wymaga naprawy przez operatora, bez automatycznego odtwarzania i założenia `CREATE IF NOT EXISTS`. Patrz [Migracje](DATABASE_MIGRATIONS.md).

Migracje wymagają dotychczasowych SELECT/INSERT/UPDATE/CREATE/ALTER; konto wykonawcze repozytorium potrzebuje SELECT/INSERT/UPDATE tabeli kont i SELECT metadanych migracji. Konto tylko do odczytu nie zapisuje. Oddziel poświadczenia migracji i wykonania; hasła nie trafiają do repozytorium, testy aplikacji nie potrzebują sudo, a przygotowanie klonów używa ograniczonego helpera. Bezpośredni SQL administratora, triggery, równoczesne DDL, zmiany trybu sesji i usunięte/ponownie użyte ID omijają kontrakt adaptera. Sprawdzenie schematu nie jest pełnym audytem triggerów, CHECK, wartości domyślnych i zewnętrznych zapisów; konto wykonawcze nie powinno mieć tych uprawnień.

## Weryfikacja i kolejne etapy

`accounts.repository.process` uruchamia prywatną MariaDB bez sieci i sprawdza odczyt/zapis, null/epokę/maksymalne daty, nazwy Unicode przypominające SQL, dokładną unikalność, duplikaty, konflikty rewizji, zapis bez zmian, stałe metadane, osobne połączenia i procesy, równoczesnych autorów, uszkodzone rekordy, zmiany silnika/indeksu, brudną historię, prawa tylko do odczytu i kolejne błędy alokacji aż do poprawnego insert/update. Pozostają aktywne testy migracji, Core i rzeczywisty ncdc ADC/ADCS:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Rejestracja kont, haszowanie/weryfikacja haseł, powiązanie uwierzytelnionej sesji, nadawanie uprawnień roli, zapis przez API/WebAdmin, usuwanie/listowanie oraz pula połączeń pozostają kolejnymi etapami. Zapis aktywnego administratora nie nadaje uprawnień sesji gościa. Testy nie zmieniają usługi produkcyjnej ani bazy hosta. Wyniki: [Testowanie](TESTING.md) i [Changelog](../../CHANGELOG.md).

Źródła MariaDB: [VARBINARY](https://mariadb.com/docs/server/reference/data-types/string-data-types/varbinary), [ograniczenia](https://mariadb.com/docs/server/reference/sql-statements/data-definition/constraint), [liczba zmienionych wierszy](https://mariadb.com/docs/connectors/mariadb-connector-c/api-prepared-statement-functions/mysql_stmt_affected_rows), [niejawny commit DDL](https://mariadb.com/docs/server/reference/sql-statements/transactions/sql-statements-that-cause-an-implicit-commit).
