# Profilowanie — alpha-0.9.13

Wykrywanie wąskich gardeł alpha-0.8. Gałąź powstaje z main `ac87fa2`. Nazwa gałęzi to `alpha-0.9.13`.

Profil działa w procesie. Mierzy trzy stałe etapy i nazywa najwolniejszy. Nie ma nowej trasy, klucza INI, migracji schematu, ustawienia Apache, flagi CLI, strony WebAdmin ani pola statusu. Plik wykonywalny huba nie łączy tej biblioteki i nie uruchamia jej przy starcie. To nie jest profiler próbkujący, wykres płomieni ani ślad żywego huba.

## Etapy

`permissions` wywołuje `allows` 256 razy. Rola i uprawnienie pochodzą z istniejących katalogów, po kolei, z zawinięciem. Polityka to domyślna pusta maska VIP.

`moderation` wywołuje `moderate` 512 razy. Operacja idzie po swoim katalogu. Aktor jest pięć kroków dalej w katalogu ról, a rola celu idzie po tym katalogu od początku. Podmiot celu jest tym, którego operacja już wymaga. Podmioty konta i CID są oznaczone jako powiązane z kontem. Aktor nie jest celem i nie jest traktowany jako twórca zapisu bana.

`decisions` wywołuje 1024 istniejące predykaty ról, na zmianę `can_mute`, `can_revoke`, `can_manage` i `can_assign`. `can_revoke` jest sprawdzeniem rangi kicka. Żadne z tych wywołań nie wysyła kicka, wyciszenia, bana ani zmiany roli.

`accepted` to liczba decyzji, które obecne funkcje dopuszczają. To nie jest nowe uprawnienie i ta liczba nie wybiera wąskiego gardła. `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Kick i mute w przeglądarce nadal nie są wysyłane.

## Szeregowanie

Wąskim gardłem jest etap z największym `elapsed_ms`. Przy równym czasie wygrywa większa liczba operacji, więc próbka skończona w ciągu jednej milisekundy nadal ma stabilną kolejność. Pozostały remis zostawia wcześniejszy etap. Zero oznacza, że etap skończył się w ciągu jednej milisekundy. Kolejność nie jest CPU hosta, procentem ani zapisaną historią.

## Granice

`profile.unit` sprawdza reguły szeregowania wprost, w tym przypadek, w którym większy czas pokonuje większą liczbę operacji, oraz przypadek, w którym liczba przyjęć nie zmienia kolejności. Potem uruchamia stałe etapy, wymaga zgodności liczb przyjęć z drugim przebiegiem, wymaga co najmniej jednego przyjęcia i wymaga zakończenia każdego etapu w pięć sekund. Sufit pięciu sekund chroni przed zawieszeniem. To nie jest deklaracja pojemności, cel usługi ani test obciążenia.

Profil nie otwiera gniazda, nie czyta pliku konfiguracji, nie zapisuje pliku wyniku i nie rozmawia z MariaDB. Schemat v9 pozostaje bez zmian. `Require local` zostaje. Status i health nadal wymagają tylko bearer. To nie zamyka zagrożenia szczątkowego.
