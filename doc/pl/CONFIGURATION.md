# Konfiguracja — alpha-0.1.11

Kolejność ustawień: wartości domyślne, jeden jawny `--config PLIK`, następnie nadpisania CLI niezależnie od miejsca `--config`. Brak automatycznego wyszukiwania pliku, zmiennych środowiskowych, include, reload i konfiguracji zdalnej.

```sh
cp config/dc24h.ini.example config/dc24h.ini
./build/dc24h --config config/dc24h.ini --check-config
./build/dc24h --config config/dc24h.ini --max-connections 512 --language pl
```

## Format INI

UTF-8, opcjonalny BOM, LF lub CRLF. Faktyczny odczyt ograniczono do 65536 bajtów, wiersz do 4096 bajtów (razem z ewentualnym CR); ostatni LF jest opcjonalny. Otwarty plik musi być zwykłym plikiem. Końcowe dowiązanie symboliczne, FIFO i urządzenie są odrzucane. Parser sprawdza UTF-8, odrzuca NUL i znaki sterujące poza tab/końcami wierszy, podaje numer wiersza bez ujawniania wartości. Nazwy są rozróżniane wielkością liter. Puste wiersze i całe komentarze `#`/`;` są pomijane. Spacje/tab wokół kluczy i wartości są usuwane. Brak komentarzy po wartości, cytowania i escape; w ścieżce spacja, `#` i `;` są dosłowne. Nieznane klucze/sekcje i powtórzenia kluczy skalarnych są błędami. Powtarzalne są tylko listy nasłuchu.

| Sekcja | Klucz | Domyślnie | Znaczenie |
| --- | --- | --- | --- |
| `general` | `language` | `us` | Język `us` lub `pl` |
| `network` | `listen` | `127.0.0.1:1511` | IPv4:port / [IPv6]:port lub `none` |
| `network` | `adcs_listen` | Brak | Endpoint TLS lub `none` |
| `network` | `max_connections` | 1024 | Wspólny limit 1..65535 |
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
