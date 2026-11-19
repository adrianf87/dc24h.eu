# Odporność bazy danych — alpha-0.7.13

**alpha-0.9.13 Profilowanie:** [Wykrywanie wąskich gardeł alpha-0.8](PROFILING.md) szereguje stały zestaw decyzji uprawnień, moderacji i ról w procesie. To nie jest profiler próbkujący ani deklaracja pojemności. Schemat v9 pozostaje bez zmian.
**alpha-0.9.12 Pomiary:** [Pomiary wydajności alpha-0.8](BENCHMARK.md) mierzy stały zestaw decyzji uprawnień i moderacji w procesie. To nie jest deklaracja pojemności ani test obciążenia. Schemat v9 pozostaje bez zmian.
**alpha-0.9.11 Monitorowanie:** [CPU, pamięć, sesje i opóźnienia](MONITORING.md) publikuje jedną próbkę procesu i liczniki sesji na istniejącym statusie z bearer. To nie jest monitor hosta ani zapisana historia. Schemat v9 pozostaje bez zmian.
**alpha-0.9.10 Logowanie:** [Bezpieczne logi i rejestrowanie zdarzeń](LOGGING_SECURITY.md) utrzymuje plik logu jako prywatny dla właściciela i zapisuje stałe skutki bezpieczeństwa bez danych klienta. To nie jest audyt produkcyjny. Schemat v9 pozostaje bez zmian.
**alpha-0.9.9 Bezpieczeństwo systemu:** [Utwardzenie usługi Debian i Ubuntu](SYSTEM_SECURITY.md) dodaje jeden przykład systemd, który uruchamia hub jako zablokowanego użytkownika systemowego bez nowych uprawnień i ze ścisłym systemem plików. Budowanie nie instaluje ani nie uruchamia tej usługi. Schemat v9 pozostaje bez zmian.
**alpha-0.9.8 Bezpieczeństwo bazy danych:** [Kopia MariaDB i obsługa awarii](DATABASE_SECURITY.md) zapisuje jedną spójną kopię tylko dla właściciela, gdy operator o to poprosi, i odtwarza ją wyłącznie do pustych tabel tego samego schematu. Utracone potwierdzenie nie jest powtarzane. Kopie poza hostem zostają przy operatorze. Schemat v9 pozostaje bez zmian.
**alpha-0.7.15 Configuration:** [startowe ustawienia bazy i logowania](DATABASE_AUTH_CONFIGURATION.md) dodają wspólne dziedziczenie timeoutów, zakresy także przy wyłączonym logowaniu, bezpieczny podgląd --show-configuration i stronę Konfiguracja EN/PL tylko do odczytu. Migawki nie zawierają poświadczeń ani prywatnych ścieżek; zmiany wymagają sprawdzenia i restartu. CI sprawdza cztery logiczne CPU na runner i używa nproc bez stałego limitu workerów. Nie czekamy na zdalne CI.

Etap powstaje bezpośrednio z main `eb6b44e2` (integracja alpha-0.7.12).
Zachowuje schemat v6, konta, formaty haseł i istniejący katalog uwierzytelniania
ADC/ADCS ładowany przy starcie. Pełny SHA podstawy zapisano w CHANGELOG.md.

## Limity czasu

Połączenia z bazą nadal korzystają z lokalnego gniazda Unix. Dotychczasowe
`timeout` w sekcji `[database]` pozostaje domyślnym limitem łączenia, odczytu
i zapisu. W tej samej sekcji można podać niezależne wartości:

```ini
[database]
enabled = on
socket = /run/mysqld/mysqld.sock
name = dc24h
user = dc24h
password_file = /etc/dc24h/database.password
timeout = 5
connect_timeout = 3
read_timeout = 5
write_timeout = 5
```

Każda jawna wartość INI musi być liczbą całkowitą od 1 do 60 sekund. Duplikaty,
zero, liczby ujemne i nieznane ustawienia są odrzucane również przy wyłączonej
bazie. Pominięcie wartości oznacza dziedziczenie `timeout`. Tylko w strukturze
C++ `Options` zero oznacza dziedziczenie. Walidacja konfiguracji nie otwiera
pliku hasła i nie łączy się z MariaDB.

Są to limity łączenia i operacji na pakietach Connector/C, a nie łączny limit
czasu zapytania, transakcji lub zamykania. Sprzątanie zapytań i duże wyniki
mogą wymagać wielu operacji wejścia/wyjścia. Nadal trzeba kontrolować zapytania
i limity blokad serwera. Timeout może mieć ten sam kod co utrata połączenia;
nie wolno wnioskować, że SQL został anulowany albo wycofany.

## Jawne odnowienie połączenia

`Connection::reconnect()` działa synchronicznie w wątku tworzącym połączenie.
Zamyka poprzednią sesję i wykonuje dokładnie jedną próbę nowego połączenia,
korzystając z pierwotnej kopii ustawień. Ponownie odczytuje plik hasła oraz
sprawdza jego właściciela, uprawnienia i zawartość. Aplikacja nie przechowuje
hasła do ponawiania; Connector/C może zachować własne dane do zamknięcia sesji.

Sukces przywraca `utf8mb4`, autocommit, lokalne gniazdo oraz wyłączone LOCAL INFILE
i automatyczny reconnect Connector/C. Tabele tymczasowe, blokady doradcze,
zmienne i ręczne ustawienia sesji zostają utracone. Repozytoria wskazujące ten sam
obiekt `Connection` mogą wykonywać nowe, świadomie zlecone operacje. Słabe
powiązania starych transakcji wygasają i nie mogą zakończyć nowej transakcji.

Nieudany reconnect pozostawia stare połączenie wycofane z użycia. Zwykłe
`query`, `execute` i `ping` odmawiają lokalnie. Po usunięciu przyczyny można
ponownie jawnie wywołać `reconnect()`. Komunikaty nie ujawniają danych logowania,
a błędy alokacji zachowują typ `std::bad_alloc`. Wywołanie z obcego wątku jest
odrzucane przed zmianą sesji.

Reconnect odmawia przy aktywnym zakresie `Transaction`, również po błędzie
połączenia lub oznaczeniu zakresu jako wymagającego rollback. Najpierw należy
go wycofać lub zniszczyć. Nieudana próba COMMIT kończy zakres, ale może zwrócić
`outcome_unknown=true`. Zniszczenie zakresu w obcym wątku żąda wycofania starej
sesji; późniejszy reconnect w wątku właściciela bezpiecznie ją usuwa. Zakończone,
przeniesione i wygasłe zakresy nie mogą zakończyć nowej transakcji.

```cpp
try {
    auto tx = connection.transaction();
    // Zaufane operacje zapisu aplikacji.
    tx.commit();
} catch (const dc24h::database::Error& error) {
    if (error.kind() != dc24h::database::ErrorKind::connection) {
        throw;
    }
    // Zakres transakcji już zniszczono. Przed zleceniem nowej operacji trzeba
    // rozstrzygnąć niepewny wynik według kluczy biznesowych aplikacji.
    connection.reconnect(); // Jedna próba; transakcja nie jest powtarzana.
}
```

Nie ma automatycznego powtarzania SQL, transakcji, DDL ani migracji, pętli prób,
reconnect w tle, puli połączeń ani SQL w reaktorze sieci. Konflikty i naruszenia
ograniczeń zachowują istniejące typy błędów. Kody awarii odczytu, zapisu i kompresji
wycofują połączenie. Stałe komunikaty nie zawierają SQL, parametrów, haseł,
prywatnych ścieżek ani surowych odpowiedzi błędów serwera.

## Działanie podczas awarii

Przy włączonej bazie start łączy się z MariaDB, wykonuje ping i wczytuje sprawdzony
katalog kont przed otwarciem portów. Niedostępna baza blokuje start bez listenerów.
Działający hub używa niezmiennej migawki kont w pamięci, więc późniejsza awaria
SQL nie blokuje reaktora, nie rozłącza użytkowników i nie zmienia ról. Nowe
logowania korzystają z tej samej migawki. Zmiany kont, haseł i blokad nadal wymagają
opisanych prac przy zatrzymanym hubie oraz restartu.

Pole API `database.enabled` opisuje konfigurację startową, nie bieżący stan bazy.
Ten etap nie dodaje sondy SQL w tle ani przeładowania uwierzytelniania na żywo.
Polecenia CLI tworzą nowe połączenia i podczas awarii kończą się ograniczonymi,
bezpiecznymi komunikatami. Powrót MariaDB nie odświeża automatycznie katalogu kont.

## Weryfikacja

`database.resilience` sprawdza granice ustawień czasu. `database.resilience.process`
używa wyłącznie tymczasowej MariaDB i lokalnego gniazda. Obejmuje zerwanie sesji
bezczynnej i transakcyjnej, niepewny COMMIT, odmowę przy otwartym zakresie,
izolację starych transakcji, reset sesji, zwolnienie blokad, obcy wątek, zmieniony
lub niezabezpieczony plik hasła, błędy alokacji, domyślny i osobny timeout odczytu,
zablokowany zapis dużego pakietu, utratę potwierdzenia faktycznie zatwierdzonej
transakcji, gniazdo bez odpowiedzi powitalnej, odmowę reconnect przy wyłączonej bazie oraz
odzyskanie po rzeczywistym zatrzymaniu i uruchomieniu MariaDB. Hub pozostaje
dostępny przez API i ADC podczas awarii. Istniejące testy bazy i ncdc sprawdzają
logowanie gości i kont, ponowne połączenie i czat przez ADC/ADCS.
Testy nie zmieniają bazy systemowej i nie uruchamiają migracji produkcyjnych.

[Opcje czasu i reconnect Connector/C](https://mariadb.com/docs/connectors/mariadb-connector-c/api-functions/mysql_optionsv)
opisują zachowanie gniazd. Pozostałe zasady: [Database Core](DATABASE_CORE.md),
[konfiguracja bazy](DATABASE.md), [testowanie](TESTING.md) i [CI](GITHUB_CI.md).
