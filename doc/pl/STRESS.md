# Testy przeciążenia — alpha-0.9.16

Przeciążenie i długotrwała praca. Gałąź powstaje z main `cdc509a`. Nazwa gałęzi to `alpha-0.9.16`.

Próba działa w procesie. Przez 24 rundy powtarza istniejący limiter ramek, limiter klientów, budżet logowania, strażnika czatu i tabelę sesji. Nie ma nowej trasy, klucza INI, migracji schematu, ustawienia Apache, flagi CLI, strony WebAdmin ani pola statusu. Plik wykonywalny huba nie łączy tej biblioteki i nie uruchamia jej przy starcie. To nie jest deklaracja pojemności, test obciążenia ani test DDoS.

## Rundy

Każda runda używa syntetycznego zegara, który porusza się tylko do przodu. Próba nie śpi i nie otwiera gniazda.

- Limiter ramek przyjmuje 4 ramki w jednej sekundzie i odmawia następnej. Kolejna runda zaczyna się sekundę później, więc okno otwiera się ponownie.
- Limiter klientów przyjmuje 2 połączenia dla jednego stałego klucza, odmawia następnego, zwalnia te dwa, przyjmuje jedno przyjęcie pozostałe w budżecie minuty i odmawia następnego. Potem przesuwa zegar o minutę i usuwa bezczynny klucz. Pusty klucz jest odmówiony i służy tylko do tego czyszczenia. Klucz nie jest połączeniem i nie trafia do logu.
- Świeży budżet logowania przyjmuje 4 ramki, a potem odmawia dwa razy. Druga odmowa pozostaje odmową, bo nieudana negocjacja pozostaje nieudana.
- Strażnik czatu przyjmuje serię 2 wiadomości po jednym bajcie i odmawia następnej. Dwie sekundy później ten sam strażnik może przyjąć serię ponownie.
- Tabela sesji przyjmuje 2 sesje, odmawia następnej istniejącym błędem pojemności, zamyka obie i usuwa je.

Jeden limiter ramek, jeden limiter klientów, jeden strażnik czatu i jedna tabela sesji żyją przez wszystkie 24 rundy. Po ostatniej rundzie tabela klientów jest pusta, żadne logowanie nie jest oczekujące i żadna sesja nie jest otwarta. Utworzone sesje są równe zakończonym sesjom.

## Granice

`stress.unit` sprawdza te liczniki, sprawdza, że druga próba zapisuje te same liczniki, sprawdza, że każdy limiter odmówił pracy, i sprawdza, że próba kończy się w ciągu pięciu sekund. Pięciosekundowy sufit jest zabezpieczeniem przed zawieszeniem. To nie jest deklaracja pojemności, cel usługi ani test obciążenia. Zero milisekund oznacza, że próba skończyła się w ciągu jednej milisekundy.

Próba nie czyta pliku konfiguracji, nie zapisuje pliku wyniku i nie rozmawia z MariaDB. Schemat v9 pozostaje bez zmian. `Require local` zostaje. Status i health zostają tylko z bearer. `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Kick i mute w przeglądarce pozostają bez wysyłki. To nie zamyka zagrożenia szczątkowego. Check status w GitHub nadal buduje tylko Debug i Release.
