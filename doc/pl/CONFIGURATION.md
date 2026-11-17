# Konfiguracja — alpha-0.1.11

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

**alpha-0.7.15 Configuration:** [startowe ustawienia bazy i logowania](DATABASE_AUTH_CONFIGURATION.md) dodają wspólne dziedziczenie timeoutów, zakresy także przy wyłączonym logowaniu, bezpieczny podgląd --show-configuration i stronę Konfiguracja EN/PL tylko do odczytu. Migawki nie zawierają poświadczeń ani prywatnych ścieżek; zmiany wymagają sprawdzenia i restartu. CI sprawdza cztery logiczne CPU na runner i używa nproc bez stałego limitu workerów. Nie czekamy na zdalne CI.

**alpha-0.7.14 WebAdmin Accounts:** [lokalne zarządzanie kontami](WEBADMIN_ACCOUNTS.md) dodaje stronicowanie metadanych, tworzenie wyłączonych kont, aktywację, wyłączenie/reset i zmianę hasła przez osobny program bez listenerów huba. Przed zapisem zatrzymaj hub; operacje chronią hasło administratora i rewizje. Schemat v6 i migawki uwierzytelniania pozostają. CI używa czterech workerów na polecenie; nie czekamy na jego zakończenie.

**alpha-0.7.13 Database Resilience:** [jawne odnowienie połączenia i niezależne limity czasu](DATABASE_RESILIENCE.md) dodają pojedynczą próbę reconnect w wątku właściciela, ponowną walidację pliku hasła oraz ochronę przed powtarzaniem SQL i zastąpieniem aktywnej transakcji. Schemat v6 i migawki uwierzytelniania pozostają. CI korzysta z runnerów GitHub-hosted Ubuntu 24.04; nie czekamy na jego zakończenie. Ten wpis zastępuje wcześniejsze opisy braku reconnect.

**alpha-0.7.12 Anti-Bruteforce:** [wspólne limity prób logowania](ANTI_BRUTEFORCE.md) dodają ograniczone pamięciowo budżety globalne/IP/konta, automatyczne odnawianie i ogólną odmowę dla ADC/ADCS oraz reconnect. Sukces nie zwraca prób; każda rola podlega limitom. Stan pozostaje w pamięci i nie zmienia kont w bazie.

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) wdraża logowanie kont GPA/PAS, osobne hasło ADC szyfrowane AES-256-GCM i prywatny klucz poza bazą, zachowując scrypt. Wymagane są jawna migracja v6, aktywne konto i dokładna para nick/CID; domyślnie obowiązuje ADCS. Zmiany i blokady w katalogu startowym wymagają restartu. Publiczne przygotowanie kont, reset, rotacja i nadawanie ról pozostają kolejnymi etapami. Ten wpis zastępuje wcześniejsze opisy planowanego uwierzytelniania.

**Aktualny etap — alpha-0.7.2:** [Migracje bazy danych](DATABASE_MIGRATIONS.md) dodają uporządkowany kompilowany katalog SQL, dokładne sumy, odczyt `--database-status`, zachowanie v1 przy aktualizacji i trwały stan applying/applied przyszłych zmian po inicjalizacji. Migracje pozostają jawne; bez schematu aplikacji i automatycznego odtwarzania DDL.

**Etap dostarczony w alpha-0.7.1:** [Database Core](DATABASE_CORE.md) dodaje typowane parametry, ograniczone wyniki na własność, metadane zapisu i transakcje RAII właściciela wątku, z redakcją błędów, blokadą zatwierdzania po błędzie i zamykaniem połączenia bez odtwarzania zapisów. Zachowano lokalne poświadczenia/migracje i CI wyłącznie na Ubuntu.

**Etap dostarczony w alpha-0.6.13:** [Konfiguracja transferów](TRANSFER_CONFIGURATION.md) dodaje dokładną ograniczoną politykę startową `allowed_protocols`, wspólne egzekwowanie CTM/RCM w Core oraz efektywne limity/protokoły tylko do odczytu w zabezpieczonym API i dwujęzycznej stronie Połączenia. Domyślne wartości zachowują routing; zmiany wymagają restartu.

**Aktualny etap — alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) dodaje stałe zbiorcze liczniki wyników CTM/RCM, kolejek/błędów i raportów klientów, rozszerzenie zabezpieczonego API oraz dwujęzyczny monitoring Połączeń. [Debian 13 CI](CI_DEBIAN13.md) dodaje sekwencyjną macierz Debug/Release własnego runnera i środowisko akcji Node 24, zachowując Node 22 dla WebAdmina.

**Aktualny etap — alpha-0.6.10:** [Transfer Limits](TRANSFER_LIMITS.md) dodaje wspólne budżety liczby/bajtów CTM/RCM nadawcy i sumy żądań odbiorcy, ograniczone prywatne odmowy oraz monotoniczne odnowienie. Konfiguracja jest walidowana przy starcie; zachowano sprzątanie sesji i istniejące kontrole bezpieczeństwa.

**Bieżący etap — alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) dodaje startową politykę CTM/RCM/gości, zaufane nadania gotowych sesji i ponowną autoryzację przygotowania/wysyłki/echa. Wspólne kontrole IP i portu zachowują działanie zarządzanych profili. Testy natywne, mieszane ADC/ADCS i ncdc obejmują odmowy, podszywanie i dalszą komunikację.

**Bieżący etap — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) dodaje do Transfer Core własne wartości protokołu i jawne rozróżnienie zwykłego ADC/1.0 oraz wymagającego TLS ADCS/1.0. Czysty wybór preferencji/możliwości nie obniża wymaganego TLS. CTM/RCM zachowują wybraną nazwę i zgodność rozszerzeń, a odpowiedzi błędów klientów zachowują tokeny. Rzeczywisty ncdc sprawdza TLS klientów z weryfikacją certyfikatu przez oba transporty huba; zachowano regresję pełnego terminala.

**Etap dostarczony w alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) łączy przyjmowanie i dostarczanie D/E CTM oraz RCM we wspólnym routerze SID, jednym typie niezmiennego pakietu i jednej ścieżce ograniczonych kolejek. Odbiorcami są ustalone okresy życia gotowych sesji: D tylko cel, E cel i niezależne echo źródła. Token nie wybiera odbiorców; zachowano role, walidację, błędy i prywatność. Testy równoległych par używają tego samego tokenu bez mieszania odbiorców.

**Etap dostarczony w alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Etap dostarczony w alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) dodaje niezależny silnik C++20 koordynacji połączeń: ograniczone własne żądania, role aktywne/pasywne IPv4/IPv6 i niezmienne plany jednej próby związane z sesjami, anulowaniem i ponownymi kontrolami. Wspólna klasyfikacja adresów Users zachowuje Search. Routing CTM/RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

**Etap dostarczony w alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

**Etap dostarczony w alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) dodaje limity liczby i bajtów zapytań oraz wysyłanych/odbieranych wyników na czas życia połączenia, ograniczone powiadomienia, politykę gości/globalną, zaufane uprawnienia sesji i konfigurowalne limity aktywnych wyszukiwań. Kontrola poprzedza routing/rejestrację tokenu. Opcjonalny wymagany token uzupełnia kontrolę właściciela/generacji; bezpośrednie UDP klientów jest poza kontrolą huba.

**Bieżący etap — alpha-0.4.12:** [Statystyki czatu WebAdmin](WEBADMIN_CHAT.md) dodają osobny widok Czat z dziewięcioma licznikami zbiorczymi, sekcjami wiadomości/kolejek, opisami US/PL, odświeżaniem, obsługą błędów/restartu i bezpiecznym eksportem.

**Etap dostarczony w alpha-0.4.11:** [Chat Logging](CHAT_LOGGING.md) dodaje opcjonalne logi wyników bez treści, z limitem tempa, oraz nasycane liczniki klienckiego B/D/E MSG w uwierzytelnionym API i WebAdmin US/PL. Przyjęcie do kolejki jest odróżnione od dostarczenia przez transport.

**Etap dostarczony w alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) grupuje startowe ustawienia czatu i dodaje limit odkodowanego tekstu, niezależne przełączniki publicznego/prywatnego/akcji i opcjonalne własne powitanie. Domyślne wartości zachowują działanie; odwracalne odmowy nie omijają walidacji ani Anti-Flood.

**Etap dostarczony w alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

Kolejność ustawień: wartości domyślne, jeden jawny `--config PLIK`, następnie nadpisania CLI niezależnie od miejsca `--config`. Brak automatycznego wyszukiwania pliku, zmiennych środowiskowych, include, reload i konfiguracji zdalnej.

```sh
cp config/dc24h.ini.example config/dc24h.ini
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini --max-connections 512 --language pl
```

## Format INI

UTF-8, opcjonalny BOM, LF lub CRLF. Faktyczny odczyt ograniczono do 65536 bajtów, wiersz do 4096 bajtów (razem z ewentualnym CR), z wyjątkiem `[chat] welcome_message`: do 8192 bajtów, aby zmieścić klucz/białe znaki i pełne 4096 bajtów wartości; ostatni LF jest opcjonalny. Otwarty plik musi być zwykłym plikiem. Końcowe dowiązanie symboliczne, FIFO i urządzenie są odrzucane. Parser sprawdza UTF-8, odrzuca NUL i znaki sterujące poza tab/końcami wierszy, podaje numer wiersza bez ujawniania wartości. Nazwy są rozróżniane wielkością liter. Puste wiersze i całe komentarze `#`/`;` są pomijane. Spacje/tab wokół kluczy i wartości są usuwane. Brak komentarzy po wartości, cytowania i escape; w ścieżce spacja, `#` i `;` są dosłowne. Nieznane klucze/sekcje i powtórzenia kluczy skalarnych są błędami. Powtarzalne są tylko listy nasłuchu.

| Sekcja | Klucz | Domyślnie | Znaczenie |
| --- | --- | --- | --- |
| `general` | `language` | `us` | Język `us` lub `pl` |
| `network` | `listen` | `127.0.0.1:1511` | IPv4:port / [IPv6]:port lub `none` |
| `network` | `adcs_listen` | Brak | Endpoint TLS lub `none` |
| `network` | `max_connections` | 1024 | Wspólny limit 1..65535 |
| `users` | `max_sessions` | 65535 | Limit sesji 1..65535; obowiązuje też max_connections |
| `users` | `max_users` | 65535 | Limit zarejestrowanych użytkowników 1..65535; obowiązują też limity sesji/połączeń |
| `network` | `idle_timeout` | 120 | 1..86400 sekund |
| `tls` | `certificate` | Brak | Pełny łańcuch PEM |
| `tls` | `private_key` | Brak | Zgodny nieszyfrowany klucz PEM |
| `logging` | `level` | `info` | `info`, `warning`, `error`, `off` |
| `logging` | `connections` | `off` | `on` lub `off`; zdarzenia klientów INFO |
| `logging` | `file` | `stderr` | `stderr` lub ścieżka pliku append |

Pierwszy jawny nasłuch usuwa domyślny TCP; samo `adcs_listen` włącza wyłącznie TLS. Kolejne wpisy tworzą listę danego transportu. `none` musi być jedynym wpisem. Łącznie wymagany jest co najmniej 1 i najwyżej 16 nasłuchów. Równoważne zapisy IPv6 nie mogą powtarzać portu; port 0 wolno powtarzać dla niezależnych portów systemowych. Wildcard jest jawny. Konflikt wildcard/adres na tym samym porcie może ujawnić się dopiero podczas bind.

Ścieżki INI są względne do rozwiązanego katalogu pliku; ścieżki CLI do katalogu roboczego procesu. Program nie tworzy katalogów. ADCS wymaga obu ścieżek TLS, a ścieżki wymagają nasłuchu ADCS. Wyłączenie TLS wymaga usunięcia nasłuchu i ustawień certyfikatu/klucza razem. Składnia i zakres pól pliku muszą być prawidłowe nawet po nadpisaniu CLI. Zależności między polami są sprawdzane po CLI, więc brakujący klucz TLS można dostarczyć argumentem.

## CLI i sprawdzanie

- `--config PLIK` wybiera jeden plik.
- `--check-config` sprawdza ustawienia po nadpisaniach i wczytuje parę TLS, bez bind, startu Core i tworzenia/otwierania logu.
- `--listen ENDPOINT/none` i `--adcs-listen ENDPOINT/none` zastępują listę danego transportu przy pierwszym użyciu; kolejne dodają wpisy.
- `--tls-cert`, `--tls-key`, `--max-connections`, `--idle-timeout` nadpisują odpowiednie pola.
- `--log-connections on/off` włącza/wyłącza zdarzenia połączeń przy INFO.
- `--log-level info/warning/error/off`, `--log-file ŚCIEŻKA/-` i `--language us/pl` nadpisują logowanie/język. `-` oznacza stderr.
- `--help` i `--version` wolno łączyć tylko z wyborem języka.

Nadpisanie listy zachowuje drugi jawnie skonfigurowany transport. Pierwszy jawny nasłuch usuwa jedynie domyślny niejawny TCP. Ostatnia powtórzona opcja skalarna CLI obowiązuje. Sprawdzanie nie bada dostępności portu, prawa zapisu logu ani istnienia jego katalogu; zasoby są otwierane podczas startu. Sprawdza kolizje ścieżek i wczytywalność/zgodność TLS, lecz zaufanie, nazwę i ważność certyfikatu weryfikuje klient.

Kody: 0 sukces sprawdzenia/pomocy/wersji/stopu; 2 błędne argumenty, INI lub nieudane sprawdzenie; 1 błąd uruchomienia/wykonania. Diagnostyka nie ujawnia wartości wejściowych. CLI `--language` wybiera też język błędów parsera; przed wczytaniem języka z pliku używany jest US. Brakujące tłumaczenia systemowe pozostają angielskie. Dane protokołu i klucze liczników nie są tłumaczone.

## Logowanie i ścieżki

`info` emituje INFO/WARN/ERROR, `warning` WARN/ERROR, `error` tylko ERROR, `off` wyłącza oba poziomy, ale wskazany plik nadal jest otwierany i sprawdzany. Format UTC: `YYYY-MM-DDTHH:MM:SSZ [INFO|WARN|ERROR] komunikat`. Mutex chroni kompletne wiersze między wątkami. Append zachowuje stare dane. Nowy plik żąda praw 0600 (umask może je ograniczyć); istniejące prawa nie są zmieniane. Wymagany jest zwykły plik z jednym dowiązaniem. Końcowe symlinki, FIFO i urządzenia są odrzucane; używane są O_APPEND/O_CLOEXEC/O_NOFOLLOW/O_NONBLOCK. Log nie może wskazywać używanego INI, certyfikatu ani klucza, także przez istniejące hardlinki i alias katalogu symlink.

Katalogi muszą być zaufane i kontrolowane przez użytkownika usługi; mechanizm nie zastępuje ochrony przed równoczesną podmianą ścieżek przez administratora. Zapis do zwykłego pliku jest synchroniczny i może czekać na dysk. Błędy działającego wyjścia są best effort, bez wyjątków z loggera. Brak rotacji, reopen, asynchronicznego loggera i reload przez SIGHUP. Wzrost logu wymaga obsługi operacyjnej. Nie loguj sekretów ani ramek klientów.

Zobacz [TLS](ADCS_TLS.md), [bezpieczeństwo](SECURITY.md) i [przykład INI](../../config/dc24h.ini.example).

Logger ogranicza teraz wiersze UTF-8, dodaje opcjonalne typowane zdarzenia połączeń i niezależne limity ostrzeżeń. [LOGGING.md](LOGGING.md) opisuje nazwy zdarzeń, prywatność, raporty pominięć i synchroniczne I/O.

## Security — alpha-0.1.7

Domyślne wartości obowiązują także bez sekcji security. Każda jest dodatnią liczbą całkowitą; zero nie wyłącza limitu. Składnia/zakres pliku muszą być poprawne przed CLI, a końcowe ustawienia sprawdza `security::validate`.

| Klucz INI w `[security]` | Nadpisanie CLI | Domyślnie | Zakres | Znaczenie |
| --- | --- | --- | --- | --- |
| `max_connections_per_ip` | `--max-per-ip` | 128 | 1..65535 | Aktywni klienci na IP |
| `max_accepts_per_minute` | `--accepts-per-minute` | 256 | 1..100000 | Przyjęcia/IP/stała minuta |
| `max_frames_per_second` | `--frames-per-second` | 1024 | 1..100000 | Wyniki dekodera/klient/stała sekunda |
| `max_invalid_frames` | `--max-invalid-frames` | 512 | 1..65535 | Błędy/czas życia połączenia |
| `tls_handshake_timeout` | `--tls-handshake-timeout` | 10 | 1..3600 | Bezwzględny termin TLS, sekundy |
| `partial_frame_timeout` | `--partial-frame-timeout` | 10 | 1..3600 | Termin niedokończonej ramki, sekundy |

Porty współdzielą stan przyjęć IP. Tempo jest na połączenie, także keepalive i błędy dekodera. N-ty błąd w czasie życia zamyka klienta. TLS ograniczają jego termin bezwzględny i bezczynność. Termin częściowej ramki nie odnawia się przy kolejnych bajtach, tylko przy następnej ramce po LF. Dokładne okna, pamięć, liczniki i terminy: [SECURITY.md](SECURITY.md).

## Database — alpha-0.1.8

Sekcja INI `[database]`: `enabled=off`, `socket=/run/mysqld/mysqld.sock`, `name=dc24h`, `user=dc24h`, `timeout=5`, opcjonalny `password_file` (wymagany po włączeniu). Obsługiwane jest lokalne gniazdo Unix. `--check-database` łączy i wykonuje ping; `--migrate-database` jawnie stosuje/sprawdza metadane migracji. Te operacje i `--check-config` wzajemnie się wykluczają. Dry-run nie odczytuje hasła i nie łączy z MariaDB. Log nie może wskazywać hasła ani jego aliasów. Zakresy, wymagania pliku i kody zakończenia: [DATABASE.md](DATABASE.md).

## Lokalne API — alpha-0.1.9

Opcje wyłącznie INI `[api]`: `enabled=off`, brak domyślnych `socket`/`token_file`, `max_connections=16` (1..128), `request_timeout=2` sekundy (1..30). Włączenie wymaga obu ścieżek; względne są rozwiązywane względem INI. Brak wartości tokena, zdalnych adresów i nadpisań CLI. `--check-config` sprawdza składnię/kolizje, ale nie czyta tokena, nie bada prywatnego katalogu wykonawczego i nie binduje; operacje utrzymania bazy nie uruchamiają API. Zwykły start wymaga prywatnego katalogu właściciela i prywatnego pliku tokena 64-hex. Kolizje logu/gniazda z konfiguracją/sekretami są odrzucane. Szczegóły: [API.md](API.md).

## Konfiguracja deweloperska WebAdmin

Frontend jest niezależny od INI: serwerowe `DC24H_API_SOCKET`/`DC24H_API_TOKEN_FILE` ustawiają opcjonalny lokalny adapter Vite tego samego UID; ścieżki absolutne, token poza WebAdmin. Brak nowego ustawienia wykonawczego C++. Statyczny build nie zawiera adaptera/tokena. Zobacz [WEBADMIN.md](WEBADMIN.md).

## Szablon HTTPS Apache2

Nowy `apache2-webadmin-https.conf.example` jest niezależny od INI. Ustaw nazwę hosta, katalog zasobów i ścieżki certyfikatu/klucza w kopii Apache. Nie zawiera Listen, korzysta z ports.conf dystrybucji; domyślnie dostęp lokalny/statyczny z blokadą API. Brak nowego parametru wykonawczego C++. Zobacz [APACHE2_HTTPS.md](APACHE2_HTTPS.md).


## alpha-0.2.8

Sekcja [security] dodaje login_timeout (domyślnie 30 sekund) i output_timeout (10 sekund), obie 1–3600. Tylko INI/biblioteka, bez nowych opcji CLI. Termin logowania biegnie bezwzględnie od accept do własnego INF, obejmując TLS; termin wyjścia liczy brak postępu wysyłki. Stałe limity: 256 KiB/klient, 16 MiB/hub. Zobacz [ADC_LOGIN.md](ADC_LOGIN.md).


## alpha-0.2.9

INI `[security] close_timeout=2` dopuszcza 1–3600 sekund. To bezwzględny termin końcowego opróżnienia, bez odnawiania przez I/O; brak nowej opcji CLI. Zobacz [ADC_STATUS.md](ADC_STATUS.md).

## Limity sesji i użytkowników — alpha-0.3.12

INI `[users] max_sessions` i `max_users` przyjmują 1..65535, domyślnie 65535. CLI `--max-sessions N` i `--max-users N` nadpisują te wartości. Efektywne sesje = min(max_connections, max_sessions); efektywni użytkownicy = min(efektywne sesje, max_users). Wyższe pułapy są poprawne i nie podnoszą niższych; zero nie wyłącza limitu. Każdy utrzymywany model liczy się jako sesja, także anonimowy TLS. Użytkownicy liczą się od identyfikacji przez closing/closed aż do usunięcia, także podczas powitania. Pełny limit sesji/połączeń zamyka nadmiarowe gniazdo przed alokacją TLS; pełny limit użytkowników daje ograniczone ISTA 211 → własne IQUI przed powitaniem/listą. Log startowy podaje efektywne wartości. [Limits](LIMITS.md) opisuje rozliczanie, typowane bramki i testy.

## Anti-Flood czatu — alpha-0.4.9

Rzeczywisty Network włącza te limity połączenia dla BMSG, DMSG i EMSG. Tempo odnawia budżet ciągle; liczba wiadomości i odkodowane bajty tekstu są obciążane razem. Nieznane/powtórzone klucze, zero i wartości poza zakresem kończą walidację błędem. Nie ma opcji wyłączenia ani nowej flagi CLI. Zakresy, odwracalne ISTA 100, ograniczanie powiadomień i reconnect opisuje [Anti-Flood](ANTI_FLOOD.md).

```ini
[chat]
max_messages_per_second = 5
message_burst = 10
max_bytes_per_second = 32768
byte_burst = 131072
```

## Polityka klienta i powitanie — alpha-0.4.10

Dotychczasowe `[chat]` przyjmuje dodatkowo `max_text_bytes` (1–16384, domyślnie 16384), `public_enabled`, `private_enabled`, `actions_enabled` i `welcome_enabled` (ściśle on/off, domyślnie on), oraz literalne `welcome_message` (UTF-8/NFC, do 4096 bajtów). Puste powitanie jest poprawne tylko przy wyłączeniu. Wartości są sprawdzane przed startem i kopiowane do niemutowalnej polityki adapterów; zmiana wymaga restartu. Nie dodano nadpisania CLI ani edycji WebAdmin. [Chat Configuration](CHAT_CONFIGURATION.md) opisuje komplet dziesięciu kluczy, zakres, stałą odwracalną diagnostykę i własne/wyłączone powitanie.
