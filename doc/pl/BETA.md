# Apache2, HTTPS i granica loopback — beta-0.2.5

Gałąź `beta-0.2.5` powstała ze scalonego `main` `56a73f7` po beta-0.2.4. Osobny rozwojowy vhost Apache z TLS nadal nasłuchuje tylko na `127.0.0.1:8443` i przed przekazaniem do Node odrzuca obcy nagłówek Host, query string na trasach API, niedozwolone metody HTTP oraz niezgodny Origin; logowanie i wylogowanie wymagają dokładnie `https://localhost:8443`. Vhost blokuje zwykły `index.html` panelu i lokalne pliki JS/CSS `local-*`, pozostawiając wyłącznie portal zdalny. Apache ogranicza żądania POST do 2048 bajtów dzięki jawnej kontroli Content-Length i Transfer-Encoding przed proxy, ponieważ samo `LimitRequestBody` nie zatrzymało żądania w rzeczywistym teście Apache. Testy uruchamiają jednorazowy Apache HTTPS, sprawdzają odmowy i potwierdzają, że zablokowane żądania nie dotarły do fikcyjnego backendu; dodatkowe testy statyczne chronią konfigurację. Publiczny vhost pozostaje bez zmian i nie jest wdrażany. Hub/API, kontroler i WebAdmin zgłaszają `beta-0.2.5`; pięć tras tylko do odczytu, cookie Secure/HttpOnly/SameSite=Strict, pakiet CMake/npm `0.1.0`, schemat MariaDB v11 i ADC/ADCS bez zmian. Bez MFA, dostępu publicznego, zapisu administracyjnego, migracji ani wydania produkcyjnego. Zobacz [Bramę WebAdmin](REMOTE_GATEWAY.md).

# Czas życia sesji, rotacja i CSRF — beta-0.2.4

Gałąź `beta-0.2.4` rozpoczyna się od scalonego `main` `b30d833` po beta-0.2.3 i jej poprawkach. Naprawiono dwa odtworzone błędy: ponowne uwierzytelnienie unieważnia poprzednie cookie sesyjne tej przeglądarki, a czas bezwzględny i czas bezczynności sesji są liczone **po** asynchronicznym sprawdzeniu hasła i uprawnień, nie od chwili otrzymania żądania. Deterministyczne testy bez gniazd obejmują limit 10 minut, idle 2 minuty, cztery sesje, odrębne cookie/CSRF przeglądarek, wylogowanie i ponowne użycie cookie, błędny Origin, jednorazowe wyzwania współbieżne, trwające odczyty i restart bramy. Playwright sprawdza usuwanie wyświetlonych statystyk po wygaśnięciu. Hub, API, kontroler i WebAdmin zgłaszają `beta-0.2.4`. Pięć tras tylko do odczytu, lokalne HTTPS, cookie Secure/HttpOnly/SameSite=Strict z `Path=/remote/v1`, pakiet `0.1.0`, schemat v11 i ADC/ADCS bez zmian. Brak dostępu publicznego, MFA, migracji, sterowania usługą i wdrożenia. Wydanie nie jest produkcyjne. Zobacz [Bramę WebAdmin](REMOTE_GATEWAY.md).

Dodatkowy przegląd blokuje równoległe ponowne logowania oraz logowanie trwające podczas wylogowania: wcześniejsze cookie musi nadal wskazywać tę samą sesję w chwili zakończenia autoryzacji. Interfejs ignoruje starsze odpowiedzi HTTP po zmianie uwierzytelnienia, ukrywa dane już przy rozpoczęciu wylogowania oraz czyści hasło również po nieudanym pobraniu wyzwania. Wyścigi mają testy bez gniazd i testy przeglądarkowe.

# Uwierzytelnianie i bieżąca autoryzacja — beta-0.2.3

Gałąź `beta-0.2.3` powstaje z `main` `070dd44` po scaleniu beta-0.2.2. Brama odrzuca odpowiedź dowodu hasła, jeśli identyfikator konta różni się od żądanego, nawet gdy zwrócono rolę administratora albo Ownera. Równoległy odczyt statusu nie kończy się sukcesem, jeśli inny odczyt w międzyczasie unieważnił sesję przeglądarki. Deterministyczne testy bez gniazd sieciowych sprawdzają logowanie, konta nieaktywne i bez uprawnień, błędne hasła i identyfikatory, utratę roli lub grantu, awarie API/DB, wyścigi odczytów i limit prób. Hub, API, kontroler i WebAdmin zgłaszają `beta-0.2.3`. Pięć tras, numer pakietu CMake/npm `0.1.0`, schemat MariaDB v11 i zachowanie ADC/ADCS nie zmieniają się. Bez dostępu publicznego, MFA, zdalnych zmian, migracji i wdrożenia. To nie jest wydanie produkcyjne. Zobacz [Bramę WebAdmin](REMOTE_GATEWAY.md).

# Uszczelnienie walidacji HTTP — beta-0.2.2

Gałąź `beta-0.2-code` rozpoczyna się od `main` w commicie `fbb7b4e`, po scaleniu beta-0.2.1. Brama nadal udostępnia wyłącznie pięć stałych tras z dokładnie określonymi metodami. Odrzuca teraz *wszystkie* nagłówki `X-Forwarded-*` podane przez przeglądarkę oraz inne niezaufane nagłówki tożsamości proxy i przepisywania ścieżek, zanim utworzy wyzwanie lub skontaktuje się z API C++. Testy negatywne obejmują warianty ścieżek, metody, ramkowanie HTTP, błędny JSON, limity rozmiaru oraz odmowy bez sekretów; przykład Apache usuwa popularne fałszywe nagłówki. Hub, API, kontroler usługi i WebAdmin zgłaszają `beta-0.2.2`; numer pakietu `0.1.0` i schemat v11 pozostają bez zmian. Brak dostępu publicznego, zdalnych zmian, MFA, zmian ADC/ADCS i wdrożenia. Zobacz [Bramę WebAdmin](REMOTE_GATEWAY.md). Nie jest to wydanie produkcyjne.

# Wersja bazowa utrzymania — beta-0.2.1

Gałąź `beta-0.2.1` powstaje z chronionego `main` po scaleniu beta-0.2 (`fbd5ab3`). Hub, lokalne API, kontroler usługi i WebAdmin zgłaszają `beta-0.2.1`; numer pakietu CMake i npm pozostaje `0.1.0`. Brama z beta-0.2 nadal wymaga uwierzytelnienia, jest dostępna wyłącznie lokalnie i pozwala tylko na odczyt. Nie dodano MFA, dostępu z Internetu, zdalnego zapisu, kontroli systemd, migracji, zmian protokołu ADC/ADCS ani wdrożenia produkcyjnego. To wersja rozwojowa, nie produkcyjna. Zobacz [Bramę WebAdmin](REMOTE_GATEWAY.md).

## Poprzedni etap

# Lokalna brama WebAdmin tylko do odczytu — beta-0.2

Gałąź beta-0.2 bazuje na aktualnym chronionym main z wersją bazową beta-0.1.8. Hub, API, kontroler usługi i WebAdmin zgłaszają beta-0.2. Osobny portal HTTPS pod /remote.html weryfikuje hasło aktywnego konta administratora albo Ownera, korzysta z krótkich sesji po stronie serwera, CSRF i ściśle określonych pól statystyk. Brama jest wyłącznie lokalna, ponieważ MFA nie jest zaimplementowane. Nie ma publicznego nasłuchu, zapisu konfiguracji, zmiany ról ani sterowania systemd. Schemat v11 i pakiet 0.1.0 bez zmian. To nie jest wydanie produkcyjne. Zobacz [Bramę WebAdmin](REMOTE_GATEWAY.md).

## Poprzedni etap

# Wersja bazowa rozwoju — beta-0.1.8

Gałąź `beta-0.1.8` powstaje z chronionego main `cebcd12`. Hub, lokalne API administracyjne, niezależny kontroler usługi i WebAdmin zgłaszają `beta-0.1.8`, a numer pakietu pozostaje `0.1.0`. Ta wersja bazowa zachowuje sterowanie usługą huba tylko dla Ownera, potwierdzanie operacji i audyt z beta-0.1.7 oraz poprawki scalone do main. Nie dodano nowej funkcji, migracji schematu, publicznej trasy proxy, zmian protokołu ADC/ADCS ani wdrożenia produkcyjnego. To nie jest wydanie produkcyjne.

## Poprzedni etap

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
