# Pomiary — alpha-0.9.12

Pomiary wydajności alpha-0.8. Gałąź powstaje z main `76a093f`. Nazwa gałęzi to `alpha-0.9.12`.

Pomiar działa w procesie. Wywołuje istniejące funkcje uprawnień i moderacji na stałej sekwencji wejść i zapisuje, ile decyzji te funkcje przyjmują oraz ile pełnych milisekund sekwencja trwała. Nie ma nowej trasy, klucza INI, migracji schematu, ustawienia Apache, flagi CLI, strony WebAdmin ani pola statusu. Plik wykonywalny huba nie łączy tej biblioteki i nie uruchamia jej przy starcie.

## Praca

`permissions` wywołuje `allows` 1024 razy. Rola i uprawnienie pochodzą z istniejących katalogów, po kolei, z zawinięciem. Polityka to domyślna pusta maska VIP.

`moderation` wywołuje `moderate` 1024 razy. Operacja idzie po swoim katalogu. Aktor jest pięć kroków dalej w katalogu ról, a rola celu idzie po tym katalogu od początku. Podmiot celu jest tym, którego operacja już wymaga. Podmioty konta i CID są oznaczone jako powiązane z kontem. Aktor nie jest celem i nie jest traktowany jako twórca zapisu bana. Odmowy zostają w próbce. Nie stają się komunikatami klienta ani wierszami audytu.

`accepted` to liczba decyzji, które obecne funkcje dopuszczają. To nie jest nowe uprawnienie. `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Kick i mute w przeglądarce nadal nie są wysyłane.

`elapsed_ms` to czas zegara monotonicznego tylko wokół tej sekwencji, w pełnych milisekundach. Zero oznacza, że sekwencja skończyła się w ciągu jednej milisekundy. To nie jest CPU hosta, tempo ani zapisana historia.

## Granice

`benchmark.unit` sprawdza, że obie sekwencje zachowują liczby operacji, że liczby przyjęć zgadzają się z drugim przebiegiem tych samych wejść, że przyjęta jest co najmniej jedna decyzja i że każda sekwencja kończy się w pięć sekund. Sufit pięciu sekund chroni przed zawieszeniem. To nie jest deklaracja pojemności, cel usługi ani test obciążenia.

Próbka nie otwiera gniazda, nie czyta pliku konfiguracji, nie zapisuje pliku wyniku i nie rozmawia z MariaDB. Schemat v9 pozostaje bez zmian. `Require local` zostaje. Status i health nadal wymagają tylko bearer. To nie zamyka zagrożenia szczątkowego.
