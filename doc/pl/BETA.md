# Linia beta — beta-0.1

Kanał wydania to Beta. Gałąź powstaje z main `423a958`. Nazwa gałęzi to `beta-0.1`.

Hub, API i WebAdmin zgłaszają `beta-0.1`. Numer projektu CMake i numer pakietu WebAdmin to `0.1.0`. To krotka pakietu. Operator widzi `beta-0.1`. Parser statusu przyjmuje tę formę beta, `beta-N.N.N` i dotychczasową formę `alpha-N.N.N`. Każdy inny tekst wersji odrzuca.

Zachowanie huba, bajty ADC, zbiory odbiorców i decyzje bezpieczeństwa zostają takie, jak w alpha-0.9.17. Schemat v9 pozostaje bez zmian. To rozwojowa Beta. To nie jest wydanie produkcyjne.

`beta.baseline` sprawdza ten napis wersji, dwanaście tras utrzymaniowych oraz to, że `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Test katalogu nadal wymaga schematu v9 i `audit_events`. Plik wykonywalny huba nie dostaje biblioteki dla tego sprawdzenia.

## Granice

To nie dodaje trasy, klucza INI, flagi CLI, pola statusu, strony WebAdmin, ustawienia Apache ani migracji schematu. Kick i mute w przeglądarce pozostają niewysyłane. Status i health zostają tylko z bearer. `Require local` zostaje. Żadne zagrożenie szczątkowe nie jest zamknięte. Publiczna uwierzytelniona brama, zdalny dostęp administratora, `configuration.manage` i `hub.manage` pozostają osobną pracą.

Check status działa na publicznych runnerach GitHub-hosted `ubuntu-22.04`, `ubuntu-24.04` i `ubuntu-26.04`. Każdy runner ma w specyfikacji cztery logiczne CPU i 16 GB pamięci. Każde zadanie sprawdza, że `nproc` równa się 4, i używa workerów `nproc`. Dodaj `beta-0.1` do gałęzi push. Lokalna walidacja używa ośmiu wątków CPU. Nie czekamy na zakończenie zdalnego CI.
