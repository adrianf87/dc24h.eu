# Sterowanie usługą huba w WebAdmin — beta-0.1.7

Gałąź `beta-0.1.7` powstaje z chronionego main `a314fe0`. Lokalny panel WebAdmin wyłącznie dla Ownera odczytuje niezależny stan systemd, potwierdza Start/Stop/Restart, pokazuje świeże dane o użytkownikach online z osobnego API i prowadzi do audytu wymagającego dowodu dostępu. Bez nowego schematu, publicznej trasy proxy, dowolnych komend i wdrożenia produkcyjnego. Zobacz [WebAdmin huba](HUB_WEBADMIN.md). Hub, API i WebAdmin zgłaszają `beta-0.1.7`; wersja pakietu nadal wynosi `0.1.0`. To nie jest wydanie produkcyjne.

## Poprzedni etap

# Operacje usługi tylko dla Owner — beta-0.1.6

Gałąź `beta-0.1.6` powstaje z chronionego main `463e56e`. Oddzielne API obsługowe sprawdza aktualną rolę Owner, grant, sesję, CSRF i wyraźne potwierdzenie przed użyciem kontrolera stałej jednostki `dc24h.service`. Dozwolone są tylko Start, Stop i Restart. Schemat v11 zapisuje trwały zamiar `started` przed akcją i zweryfikowany wynik `succeeded` albo `failed`; niepewny wynik `outcome_unknown` nie jest ponawiany automatycznie. Brak interfejsu przeglądarkowego i publicznego proxy. Zobacz [Operacje systemd](SERVICE_OPERATIONS.md). To nie jest wydanie produkcyjne.

# Kontroler usługi tylko do odczytu — beta-0.1.5

Gałąź `beta-0.1.5` powstaje z main `e8d00ae`. Hub, API i WebAdmin zgłaszają `beta-0.1.5`. Osobny proces C++20 `dc24h-service-controller` odczytuje stan ustalonej usługi `dc24h.service` przez lokalne gniazdo Unix z autoryzacją, także przy zatrzymanym hubie. Obsługiwany jest wyłącznie odczyt stanu. Nie dodano trasy HTTP, przycisków WebAdmin, Start/Stop/Restart ani migracji. Zobacz [Kontroler usługi](SERVICE_CONTROLLER.md). Schemat v10 bez zmian. Uprawnienie `hub.manage` nadal nie jest wykonywalne.

## Poprzedni etap

# Edytor konfiguracji WebAdmin — beta-0.1.4

Gałąź `beta-0.1.4` powstaje z main `3f7f03d`. Hub, API i WebAdmin zgłaszają `beta-0.1.4`. Ograniczony edytor na stronie Konfiguracja odczytuje INI z oddzielnego procesu administracyjnego, pokazuje cztery niesekretne ustawienia i wartości startowe tego procesu, waliduje oraz prezentuje różnice, a następnie wymaga potwierdzenia zapisu atomowego albo przywrócenia kopii. Apache i adapter Node przekazują wyłącznie dokładne lokalne trasy z kontrolą pochodzenia, grantu, sesji, CSRF i uprawnienia po stronie C++. Nadal wymagany jest kontrolowany restart huba; brak przeładowywania na żywo i kontrolera usługi. Schemat v10 bez zmian. Zobacz [Edytor konfiguracji](CONFIGURATION_EDITOR.md).

## Poprzedni etap

# Atomowy zapis konfiguracji — beta-0.1.3

Gałąź `beta-0.1.3` powstaje z main `83b224c`. Hub, API i WebAdmin zgłaszają `beta-0.1.3`. Oddzielne lokalne API administracyjne umożliwia atomowy zapis/przywrócenie INI z ograniczonej listy ustawień i rewizją zawartości. Migracja v10 rejestruje rozpoczęcie i wyniki operacji; historia v9 pozostaje nienaruszona. Zobacz [Zapis konfiguracji](CONFIGURATION_WRITE.md). Nie dodano edytora WWW, przeładowywania huba ani kontrolera usługi.

## Poprzedni etap

# Przegląd konfiguracji — beta-0.1.2

Nazwa gałęzi to `beta-0.1.2`. Hub, API i WebAdmin zgłaszają `beta-0.1.2`. Lokalny interfejs administracyjny czyta cztery niesekretne ustawienia, rozróżnia zapisane od aktywnych w procesie administracyjnym oraz waliduje propozycje na podstawie rewizji zawartości, bez zapisu. Aktywna konfiguracja pracującego huba jest niedostępna w procesie administracyjnym. Zobacz [Przegląd konfiguracji](CONFIGURATION_REVIEW.md). Pełne zarządzanie konfiguracją i hubem pozostaje niezaimplementowane. Schemat v9 pozostaje bez zmian.

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
