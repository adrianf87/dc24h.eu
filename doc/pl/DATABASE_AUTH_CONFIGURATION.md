# Konfiguracja bazy i logowania — alpha-0.7.15

Etap porządkuje istniejące ustawienia MariaDB i logowania ADC, dodaje wspólny
mechanizm dziedziczenia timeoutów, bezpieczny podgląd CLI i stronę Konfiguracja
WebAdmin tylko do odczytu. Zachowuje schemat v6, hasła, tożsamości, GPA/PAS,
role, reguły gości i osobny tryb administracyjny do zapisu kont.

## Prywatny plik INI

Edytuj jawny plik `--config FILE`. Ścieżki względne odnoszą się do katalogu INI.
Plik ma UTF-8, ograniczony rozmiar, ścisłe nazwy kluczy i wartości literalne.
Nieznane i powtórzone klucze, wartości inne niż on/off oraz liczby poza zakresem
są odrzucane przed startem. Haseł bazy i kluczy ADC nie podaje się w argumentach,
rozwijanych zmiennych środowiska, publicznym JSON ani plikach przeglądarki.

[Domyślny przykład](../../config/dc24h.ini.example) pozostawia bazę i logowanie
kont wyłączone. Nowy [przykład logowania przez ADCS](../../config/adc-accounts.ini.example)
zawiera kompletny lokalny profil z samym ADCS, wymagający przygotowanych plików
i kont. Nie instaluje ich automatycznie i nie zmienia reguł dostępu gości.
Chroń pliki produkcyjne, ich katalogi oraz prywatne pliki poświadczeń.

| Klucz `[database]` | Domyślnie / zakres |
| --- | --- |
| enabled | off; tylko on/off |
| socket | /run/mysqld/mysqld.sock; niepusta ścieżka lokalnego gniazda Unix |
| name | dc24h; 1..64 liter ASCII, cyfr lub znaków podkreślenia |
| user | dc24h; 1..128 bajtów bez znaków sterujących |
| password_file | prywatny plik wymagany przy enabled=on |
| timeout | 5; liczba całkowita 1..60 sekund |
| connect_timeout, read_timeout, write_timeout | opcjonalnie 1..60; pominięte dziedziczą timeout |

Połączenie Connector/C i publiczny podgląd używają tego samego mechanizmu.
Zero w strukturze C++ oznacza dziedziczenie; jawne zero INI jest błędem. Limity
dotyczą łączenia i operacji pakietowych, a nie łącznego czasu zapytania,
transakcji lub zamykania. SQL nadal używa tylko gniazda Unix, bez awaryjnego TCP,
automatycznego reconnect i powtarzania zapisów. Zobacz [odporność bazy](DATABASE_RESILIENCE.md).

| Klucz `[authentication]` | Domyślnie / zakres |
| --- | --- |
| enabled | off; tylko on/off |
| key_file | osobny prywatny plik klucza AES 32 bajty, wymagany po włączeniu |
| require_tls | on; logowanie kont przez ADCS, chyba że jawnie ustawiono off |
| max_accounts | 1024; 1..4096, także w wyłączonych ustawieniach C++ |
| attempts_per_ip / attempts_per_account | 20 / 10; każde 1..100000 |
| attempts_per_second | 128; globalnie 1..100000 |
| attempt_window_seconds | 60; czas uzupełnienia budżetu 1..3600 sekund |
| max_tracked_attempt_ips / max_tracked_attempt_accounts | 4096 / 4096; każde 1..65536 |

Logowanie kont wymaga włączonej bazy i prywatnego pliku hasła, osobnego klucza
ADC, aktualnego schematu i przygotowanych kont z hasłami ADC. require_tls=on
wymaga też listenera ADCS. Przy starcie certyfikat TLS i klucz muszą być zgodne.
Nie ma cichego przejścia na nieszyfrowane logowanie. Dostęp gości nadal określają
istniejące reguły tożsamości i funkcji huba.

Ochrona logowania używa również `[security]`: login_timeout=30,
tls_handshake_timeout=10, output_timeout=10, close_timeout=2 (każde 1..3600 sekund),
max_pending_logins=256, max_pending_per_ip=64 (1..65535), max_login_bytes=65536
(1..16777216) i max_login_frames=64 (1..65535). Budżety prób są wspólne dla ADC/ADCS
i kolejnych połączeń, naliczają też udane próby i uzupełniają się ciągle.
Nie są ustawieniami trwałych banów.

## Sprawdzanie i podgląd

```sh
dc24h --config /etc/dc24h/hub.ini --show-configuration
dc24h --config /etc/dc24h/hub.ini --check-config
dc24h --config /etc/dc24h/hub.ini --check-database
```

`--show-configuration` sprawdza scaloną składnię, zakresy i zależności, po czym
zwraca niezależny od języka JSON z efektywnymi timeoutami i bezpiecznymi limitami
logowania. Nie odczytuje hasła, klucza ADC ani plików TLS, nie łączy się z SQL,
nie otwiera listenerów i nie tworzy logu. Sukces nie potwierdza więc poprawności
poświadczeń, certyfikatów i dostępności bazy. `--check-config` dodatkowo sprawdza
włączone poświadczenia ADC/TLS; `--check-database` osobno sprawdza połączenie SQL.
Migracje i odczyt stanu schematu nadal wymagają osobnych jawnych poleceń.

Podglądu nie łączy się z check-config, operacjami bazy, help ani version; takie
kombinacje zwracają 2. Pozostaje istniejąca kolejność INI i nadpisań CLI.
`--language` tłumaczy komunikaty i pomoc, a nie klucze JSON. Obowiązują wcześniejsze
zabezpieczenia plików i ścieżek oraz brak automatycznych migracji.

## Migawka publiczna i panel

Uwierzytelnione `GET /api/v1/status` może zawierać `configuration`: tryb `hub`
lub `maintenance`, restart_required=true, stan konfiguracji bazy, transport Unix,
domyślny i efektywne timeouty oraz przełączniki i limity logowania.
Typ danych C++ celowo nie ma nazwy bazy, użytkownika, ścieżki gniazda, hasła,
klucza ani ścieżek poświadczeń. Tylko dozwolone pola trafiają do odpowiedzi,
adaptera i eksportu. CLI i działająca usługa używają tego samego odwzorowania.
API administracyjne pokazuje swój rzeczywisty tryb bez listenerów i wyłączone
logowanie kont; nie ładuje katalogu uwierzytelniania huba.

Nawigacja Konfiguracja otwiera stronę EN/PL tylko do odczytu. Rozróżnia ustawienia
startowe od bieżącego stanu SQL, pokazuje dziedziczenie timeoutów, wymagania ADCS,
limity logowania i sposób zastosowania zmian. Brak pola w starszym API oznacza
niedostępność podglądu, bez blokowania monitorowania. Błędne nowe ustawienia
odrzucają całą migawkę i usuwają stare wartości oraz możliwość eksportu.
Odświeżanie, pauza i bezpieczny eksport są wspólne z istniejącym panelem.

Zmień prywatny INI, sprawdź go i uruchom usługę ponownie. Etap nie dodaje zapisu
konfiguracji przez HTTP, edytora haseł połączenia, przeładowania na żywo, sondy SQL
w tle ani wymiany katalogu kont w działającym hubie. Do zapisu kont nadal służy
[osobny tryb administracyjny](WEBADMIN_ACCOUNTS.md). Przykłady Apache nie zmieniają się.

## Testowanie i CI

Testy C++ obejmują zakresy przy wyłączonym logowaniu, dziedziczenie timeoutów,
poufność podglądu i zależności trybu. Testy procesu dowodzą braku skutków ubocznych
przy nieistniejących poświadczeniach SQL/ADC/TLS, odmowy sprzecznych poleceń
i identycznego JSON w US/PL. Tymczasowa MariaDB oraz ADC/ADCS porównują ustawienia
z działającą migawką; testy rzeczywistego ncdc pozostają włączone.
Node i Chromium sprawdzają typy, zakresy, pomijanie poświadczeń, starsze API,
eksport, brak edycji i polski widok mobilny. Integracja C++ → adapter → przeglądarka
sprawdza wyświetlanie efektywnych timeoutów.

Każdy runner GitHub Ubuntu 24.04 publicznego repozytorium otrzymuje cztery logiczne
CPU, sprawdzane przez nproc=4. Równoległość poleceń wynika z nproc, bez stałego
limitu workerów. Nie czekamy na zakończenie zdalnego CI. Zobacz [CI](GITHUB_CI.md).
