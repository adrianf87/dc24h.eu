# Przegląd konfiguracji — beta-0.1.2

Nazwa gałęzi to `chatgpt-beta-0.1.2`. Hub, API i WebAdmin zgłaszają `beta-0.1.2`. Lokalny interfejs administracyjny czyta cztery niesekretne ustawienia, rozróżnia zapisane od aktywnych w procesie administracyjnym oraz waliduje propozycje na podstawie rewizji zawartości, bez zapisu. Aktywna konfiguracja pracującego huba jest niedostępna w procesie administracyjnym. Zobacz [Przegląd konfiguracji](CONFIGURATION_REVIEW.md). Pełne zarządzanie konfiguracją i hubem pozostaje niezaimplementowane. Schemat v9 pozostaje bez zmian.

## Poprzedni etap

# Fundament zarządzania — beta-0.1.1

Kanał wydania to Beta. beta-0.1.1 powstaje z main `075081f`. Nazwa gałęzi to `beta-0.1.1`.

Hub, API i WebAdmin zgłaszają `beta-0.1.1`. Numer projektu CMake i numer pakietu WebAdmin pozostają `0.1.0`. To krotka pakietu. Operator widzi `beta-0.1.1`. Parser statusu przyjmuje `beta-N.N`, `beta-N.N.N` i dotychczasową formę `alpha-N.N.N`. Każdy inny tekst wersji odrzuca.

Zachowanie huba, bajty ADC, zbiory odbiorców i decyzje bezpieczeństwa dostarczone do alpha-0.9.17 zostają. Schemat v9 pozostaje bez zmian. To rozwojowa Beta. To nie jest wydanie produkcyjne. Żadne zagrożenie szczątkowe nie jest zamknięte.

`configuration.manage` i `hub.manage` pozostają niezaimplementowane. Katalog i kontrakt autoryzacji opisuje [Fundament zarządzania](MANAGEMENT_FOUNDATION.md). Ten etap nie odczytuje ani nie zapisuje konfiguracji i nie uruchamia, nie zatrzymuje ani nie restartuje huba.

`beta.baseline` sprawdza ten napis wersji, dwanaście tras utrzymaniowych oraz to, że `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Test katalogu nadal wymaga schematu v9 i `audit_events`. Plik wykonywalny huba nie dostaje biblioteki dla tego sprawdzenia.

## Wcześniejsza linia — beta-0.1

Gałąź powstaje z main `423a958`. Nazwa gałęzi to `beta-0.1`. Hub, API i WebAdmin zgłaszały `beta-0.1`. Tamten etap nie dodał trasy.

## Granice

Kick i mute w przeglądarce pozostają niewysyłane. Status i health zostają tylko z bearer. `Require local` zostaje. Publiczna uwierzytelniona brama i zdalny dostęp administratora pozostają osobną pracą.

Check status działa na publicznych runnerach GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04` i `ubuntu-26.04`. Każdy runner ma w specyfikacji cztery logiczne CPU i 16 GB pamięci. Każde zadanie sprawdza, że `nproc` równa się 4, i używa workerów `nproc`. Pull requesty do `main` uruchamiają te same sprawdzenia. Lokalna walidacja używa ośmiu wątków CPU. Nie czekamy na zakończenie zdalnego CI. Scalenie do `main` czeka, aż wymagane kontrole się powiodą.
