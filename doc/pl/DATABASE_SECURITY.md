# Bezpieczeństwo bazy danych — alpha-0.9.8

Obsługa awarii MariaDB i kopia zapasowa. Gałąź powstaje z main `2a2737b`. Nazwa gałęzi to `alpha-0.9.8`.

Hub już trzyma jedną sesję gniazda Unix, wycofuje ją po utracie potwierdzenia i łączy ponownie tylko na żądanie właściciela. Ten etap dodaje jedną jawną kopię. To nie jest harmonogram, replika ani klucz INI.

## Migawka

`--backup-database PLIK` łączy się dotychczasowymi ustawieniami, wykonuje ping i bierze blokadę doradczą migracji. Schemat musi już być czystą wersją 9. Polecenie uruchamia `START TRANSACTION WITH CONSISTENT SNAPSHOT`, odczytuje siedem tabel katalogu i wycofuje migawkę. Nie zatwierdza zmian i nie zmienia wierszy.

Plik powstaje z `O_EXCL` i `O_NOFOLLOW`, w trybie `0600`, należy do użytkownika huba i ma jedno dowiązanie. Dowiązanie symboliczne, istniejąca ścieżka albo tryb inny niż `0600` są odrzucane. Jeśli migawka się nie zakończy, także przy utraconym potwierdzeniu wycofania, niepełny plik jest usuwany. Awaria procesu może zostawić niepełny plik; przywracanie go odrzuca, bo końcówka SHA-256 już nie pasuje.

Plik zawiera wiersze tabel, w tym weryfikatory haseł i zapieczętowane sekrety ADC. Nie zawiera hasła MariaDB, ścieżki gniazda ani tekstu błędu serwera. Traktuj ten plik jak plik hasła. Standardowe wyjście podaje tylko `Zapisano kopię bazy danych, bajty=` i długość.

Każda tabela ma limit 4096 wierszy. Plik ma limit 32 MiB. Komórka ma limit 65536 bajtów. Większa baza jest odrzucana i plik nie zostaje. Kopia nie łączy się ponownie i nie powtarza SQL.

## Przywracanie

`--restore-database PLIK` czyta ten prywatny plik i sprawdza końcówkę przed jakimkolwiek zapisem. Żywy schemat musi być tą samą czystą wersją 9, a wiersze migracji muszą zgadzać się z plikiem. Każda tabela aplikacji musi być pusta. Zgodna historia migracji nie jest przepisywana.

Wiersze są wstawiane w jednej zwykłej transakcji z wiązanymi parametrami. Plik nie jest wykonywany jako SQL. Utracone potwierdzenie zatwierdzenia pozostaje `outcome_unknown` i nie jest powtarzane; operator sprawdza tabele przed kolejną próbą. Wyjście z zakresu wycofuje przywracanie, które nie doszło do zatwierdzenia.

## Bez zmian

Nie ma harmonogramu kopii, kopii poza hostem ani repliki. Uprawnienia katalogu danych MariaDB zostają przy operatorze. To nie zamyka zagrożenia szczątkowego T14. Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, strony WebAdmin, klucza INI ani ustawienia Apache. Start nadal nie migruje. `Require local` zostaje.

`database.process` sprawdza czystą migawkę, tryb tylko dla właściciela, odmowę dowiązania symbolicznego, istniejącej ścieżki, pliku czytelnego dla grupy i uszkodzonej końcówki, podróż binarną poświadczenia oraz odmowę, gdy schemat nie jest bieżący albo baza jest wyłączona.
