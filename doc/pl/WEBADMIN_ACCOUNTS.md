# Zarządzanie kontami WebAdmin — alpha-0.7.14

**alpha-0.8.2 Operators:** [Spis zespołu i lokalne tworzenie/przekazanie własności](OPERATORS.md) zastępują wcześniejsze ograniczenia bootstrap/przekazania Ownera. Schemat v7 i zwykła ochrona Ownera pozostają.

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

Strona Konta obsługuje listę po 25 kont, wybór konta, tworzenie wyłączonych kont,
aktywację, wyłączenie, zmianę hasła po podaniu obecnego oraz reset przez
administratora. Domyślny język to EN/US; panel ma polskie tłumaczenie.
ID i rewizje są tekstem dziesiętnym, aby zachować precyzję BIGINT.
Usuwanie kont, edycja ról, mapowanie CID, tworzenie pierwszego administratora,
eksport poświadczeń i publiczna rejestracja nie należą do tego etapu.

## Tryb administracyjny

Hub ładuje katalog uwierzytelniania przy starcie. Przed zapisem zatrzymaj wszystkie
instancje huba korzystające z tej bazy. Nowy program `dc24h-account-admin` otwiera
wyłącznie prywatne gniazdo lokalnego API: nie otwiera ADC/ADCS i nie ładuje migawki
kont. Wymaga włączonej bazy i API, wyłączonego uwierzytelniania ADC w konfiguracji
oraz czystego, aktualnego schematu v6. Nie uruchamia migracji automatycznie.

Przygotuj osobny prywatny plik INI:

```ini
[database]
enabled = on
socket = /run/mysqld/mysqld.sock
name = dc24h
user = dc24h_maintenance
password_file = /etc/dc24h/maintenance.password
[api]
enabled = on
socket = /run/user/1000/dc24h-maintenance/admin.sock
token_file = /etc/dc24h/maintenance.token
[authentication]
enabled = off
```

Schemat i dedykowane konto bazy przygotuj według istniejącej instrukcji instalacji.
Katalog gniazda musi należeć do użytkownika procesu i mieć tryb 0700, a pliki hasła
i tokenu — prywatne uprawnienia właściciela. Konto SQL wymaga SELECT w schemacie,
INSERT/UPDATE dla dc24h_accounts i dc24h_account_credentials oraz DELETE dla
dc24h_account_adc_credentials. Po jawnych migracjach nie potrzebuje DDL. Po zatrzymaniu huba uruchom:

```sh
dc24h-account-admin --config /etc/dc24h/maintenance.ini
cd webadmin
DC24H_API_SOCKET=/run/user/1000/dc24h-maintenance/admin.sock \
DC24H_API_TOKEN_FILE=/etc/dc24h/maintenance.token npm run dev
```

W lokalnym panelu wybierz Konta. Token API odczytuje adapter po stronie serwera;
nie trafia do przeglądarki. Sprawdź konto i operację, podaj odpowiednie hasło
konta i potwierdź zatrzymanie huba. Tworzenie, aktywacja, wyłączenie i reset
wymagają istniejącego aktywnego administratora i aktualnego hasła. Administrator
nie może zarządzać własnym dostępem ani innym administratorem. Zmiana hasła
wymaga obecnego hasła wybranego konta. Obowiązują dotychczasowe zasady uprawnień. Tworzenie konta ponownie sprawdza
migawkę administratora i dokładny rekord użyty do potwierdzenia hasła pod blokadami
w tej samej transakcji co zapis nowego konta i skrótu hasła.

Nowe konta są registered/disabled. Reset pozostawia konto wyłączone. Aktywacja,
wyłączenie oraz zmiana hasła atomowo usuwają poświadczenia ADC i aktualizują
metadane. W razie potrzeby ustaw świeże osobne hasło ADC przez istniejącą zaufaną
usługę; ten etap nie dodaje tej operacji do przeglądarki. Zatrzymaj program
administracyjny i ponownie uruchom hub, aby wczytać końcową migawkę kont.
Program nie wykrywa instancji huba na innym hoście: zatrzymanie wszystkich
instancji jest obowiązkiem operatora i jawnym warunkiem operacji w panelu.

## Lokalne API HTTP

Wszystkie trasy wymagają prywatnego tokenu Bearer i tego samego UID na gnieździe
Unix. Obsługę kont rejestruje tylko osobny program administracyjny. Zwykły `dc24h`
zachowuje API health/status tylko do odczytu, bez treści żądań.

| Metoda | Trasa | Dane/wynik |
| --- | --- | --- |
| GET | `/api/v1/accounts?after=0` | Do 25 kont według ID; next_after lub null |
| GET | `/api/v1/accounts/{id}` | Sprawdzone metadane i maintenance=true |
| POST | `/api/v1/accounts` | actor_id, proof, name, password; konto wyłączone |
| POST | `/api/v1/accounts/{id}/activate` | revision, actor_id, proof |
| POST | `/api/v1/accounts/{id}/disable` | revision, actor_id, proof |
| POST | `/api/v1/accounts/{id}/reset` | revision, actor_id, proof, password |
| POST | `/api/v1/accounts/{id}/password` | revision, proof, password |

POST używa `application/x-www-form-urlencoded` i dokładnego Content-Length.
Nie obsługuje chunking, potokowania ani powtórzonych nagłówków i pól; nieznane pola
są odrzucane. Żądanie ma do 8192 bajtów, treść do 7000, zdekodowane pole do 1024.
UTF-8 po dekodowaniu procentowym sprawdzają istniejące usługi. Hasło musi mieć
co najmniej 15 znaków i najwyżej 1024 bajty UTF-8.
Zapisy są wykonywane kolejno w lokalnym wątku API, najwyżej raz na sekundę na
proces, także po odmowie. Limit żądania w trybie administracyjnym wynosi 30 sekund.
Każde żądanie tworzy połączenie SQL we własnym wątku; nie ma SQL w reaktorze,
współdzielonego połączenia, automatycznego reconnect ani powtarzania zapisów.

Wyniki zawierają tylko ID, nazwę, rolę, stan, rewizję i znaczniki czasu UTC.
Komunikaty błędów są stałe i nie ujawniają poświadczeń. Konflikt rewizji daje 409,
limit prób 429, awaria bazy 503. Utrata potwierdzenia zapisu daje `outcome_unknown`;
najpierw odśwież i sprawdź wynik, zanim świadomie zlecisz nowy zapis. Panel i adapter
nie powtarzają operacji automatycznie.

Adapter Vite zachowuje kontrolę adresu lokalnego, Host, Origin i Fetch-Metadata.
Zapis dodatkowo wymaga zgodnego Origin oraz nagłówka `X-DC24H-Admin: 1`.
Adapter ogranicza treść oraz liczbę połączeń do czterech i odtwarza odpowiedzi
z dozwolonych pól. Hasła nie trafiają do stanu React, local/session storage,
adresów URL, eksportu ani logów. Formularz czyści się przy wysłaniu, a bufory
żądań i formularzy C++ są zerowane. Nie można obiecać wymazania wewnętrznych kopii
przeglądarki, JavaScript i biblioteki połączenia. Zamykanie czeka na bieżącą operację.

To lokalny tryb administracyjny. Przykłady Apache nadal serwują pliki statyczne
i blokują ścieżki API. Logowanie zdalne w przeglądarce, zmiany podczas działania
huba i uwierzytelniona brama produkcyjna pozostają kolejnymi etapami.

## Testy

`accounts.webadmin.process` używa tymczasowej MariaDB i sprawdza stronicowanie,
rzeczywiste hasła, aktywację/wyłączenie/reset/zmianę, duplikaty, rewizje, role,
odmowę zmiany własnego dostępu, limity prób, poufność i zamykanie bez listenerów
huba. Pozostają istniejące testy kont, ADC i ncdc. Testy Node obejmują walidację,
precyzję BIGINT, dozwolone pola odpowiedzi oraz Origin i nagłówek adaptera.
Chromium sprawdza wybór kont, potwierdzanie zapisu, konflikty, czyszczenie haseł,
niedostępne operacje i polski widok mobilny. CI korzysta z GitHub Ubuntu 24.04
i czterech workerów na polecenie. Nie czekamy na zakończenie zdalnego CI.
