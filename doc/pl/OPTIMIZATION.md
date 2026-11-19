# Optymalizacja — alpha-0.9.14

Pamięć i routing C++20. Gałąź powstaje z main `f7e465c`. Nazwa gałęzi to `alpha-0.9.14`.

Publikacje routingu zachowują te same bajty ADC. Zmieniają się dwa lokalne koszty. Nie ma nowej trasy, klucza INI, migracji schematu, ustawienia Apache, flagi CLI, strony WebAdmin ani pola statusu. To nie jest deklaracja pojemności ani test obciążenia.

## Wspólne ramki

Czat publiczny, czat prywatny, wyszukiwanie, wyniki, connect-to-me, reverse connect, komunikaty huba i routowany status budują jedną niezmienną ramkę. Ramka i blok kontrolny `std::shared_ptr` pochodzą teraz z jednej alokacji `std::make_shared`. Passkey zostawia tworzenie przy routerze, który już był właścicielem. Dispatch nadal podaje każdemu odbiorcy tę samą ramkę. Zerowy budżet alokacji podczas dispatch oznacza, że rozesłanie nie alokuje kolejnej ramki.

## Lista powitalna

Przygotowanie początkowej listy użytkowników już koduje każdą ramkę, aby zmierzyć budżet wyjścia. To kodowanie zostaje w planie. Logowanie kolejkuje te bajty i nie serializuje tych samych komunikatów ponownie. Zapisany komunikat pozostaje kopią, więc późniejsza zmiana nazwy sesji źródłowej nie zmienia planu. Nowa serializacja zapisanego komunikatu zgadza się z zachowanym kodowaniem.

## Granice

Format przewodu, zbiór odbiorców, kwoty wyjścia i błędy odmowy zostają. `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Kick i mute w przeglądarce nadal nie są wysyłane. Schemat v9 pozostaje bez zmian. `Require local` zostaje. Status i health nadal wymagają tylko bearer. To nie zamyka zagrożenia szczątkowego.
