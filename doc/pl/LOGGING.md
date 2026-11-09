# Logowanie — alpha-0.1.6

`dc24h_logging` zapisuje zsynchronizowane wiersze UTC na stderr lub do zwykłego pliku append. Błędy zapisu działają best effort. Logger musi żyć dłużej niż wywołujący; przed zniszczeniem zakończ i dołącz wszystkie wątki. Domyślne komunikaty są US, opcjonalnie PL z katalogów. Dane protokołu i klucze zdarzeń nie są tłumaczone.

## Poziomy i ustawienia

| Ustawienie | Emitowane poziomy |
| --- | --- |
| `info` (domyślne) | INFO, WARN, ERROR |
| `warning` | WARN, ERROR |
| `error` | ERROR |
| `off` | Brak |

INFO obejmuje wersję/start, niesekretne limity/liczby nasłuchów, gotowość portów, Core, sygnały i liczniki końcowe. WARN obejmuje błędne/ucięte ramki ADC, limit połączeń, bezczynność, negocjację TLS i błędy I/O klienta. ERROR opisuje błędy startu/infrastruktury oraz brak zasobów. Błędy argumentów i wynik sprawdzenia konfiguracji trafiają bezpośrednio na stdout/stderr przed utworzeniem loggera; `off` ich nie ukrywa.

```ini
[logging]
level = info
file = stderr
connections = off
```

```sh
./build/dc24h --config config/dc24h.ini.example \
  --log-level info --log-file /var/log/dc24h/hub.log --log-connections on --language pl
```

`--log-file -` wybiera stderr. Prawa, aliasy, ścieżki względne i pierwszeństwo INI/CLI opisuje [CONFIGURATION.md](CONFIGURATION.md). Nowy plik żąda praw 0600; istniejące nie są zmieniane. Plik jest otwierany/sprawdzany także przy `off`. Katalogi nie powstają automatycznie.

## Zdarzenia połączeń i prywatność

Wpisy INFO dla klientów wymagają `connections=on` lub `--log-connections on` oraz poziomu `info`; domyślnie są wyłączone. Rekord zawiera tylko wewnętrzny token, transport i stałą nazwę zdarzenia:

```text
2026-11-09T00:00:00Z [INFO] Połączenie id=2 transport=ADCS event=tls_ready
```

Enum `ConnectionEvent` dopuszcza accepted, peer_closed, tls_ready, tls_handshake_failed, tls_closed, tls_read_error, tcp_read_error, socket_error, idle_timeout, handshake_timeout, shutdown, security_quota i partial_frame_timeout. API przyjmuje enum, liczbowe ID i flagę TLS, bez dowolnego tekstu zdarzenia. ID jest tymczasowe i lokalne dla procesu, nie oznacza uwierzytelnionego konta. Negocjacja TLS nie oznacza logowania ADC. Odebrany close_notify ma inne zdarzenie niż błąd odczytu; wysyłanie close_notify serwera pozostaje planem.

Rekordy nie otrzymują adresów/portów klientów, nicków, CID/PID, hashy PAS ani treści ramek. Gotowość nasłuchu nadal podaje jawny lokalny adres bind. Podsumowanie startu zawiera liczby nasłuchów, limity, timeout i stan logowania połączeń, bez ścieżek INI/logu/klucza. Wywołujący ogólne info/warning/error także musi pomijać sekrety: oczyszczanie nie usuwa dowolnych poufnych tekstów.

## Limity i duży ruch

Logger emituje do 64 zdarzeń połączeń w jednym oknie sekundy zegara monotonicznego, wspólnym dla klientów i transportów. Okno zaczyna pierwszy wpis i odnawia się przy następnym zdarzeniu. Nie jest to okno kroczące, więc granica sąsiednich okien dopuszcza większy chwilowy ruch. Nadmiar jest zliczany bez kolejki. Jeden WARN podaje liczbę pominiętych zdarzeń przy kolejnym zdarzeniu po odnowieniu okna lub w destruktorze. Licznik nasyca się przy uint64 max. Historia może być niepełna i nie stanowi rejestru audytowego. Poziom nie zmienia się w czasie życia loggera.

Reaktor sieci osobno emituje najwyżej jedno ostrzeżenie na pięć sekund dla każdej kategorii: błędne ramki (także ucięty EOF), limit połączeń, zwykła bezczynność, błąd tworzenia/negocjacji TLS, termin negocjacji oraz I/O klienta. Kategorie są niezależne: limit nie tłumi ostrzeżenia o bezczynności. Pominięte ostrzeżenia nie mają kolejki ani osobnych raportów; zbiorcze liczniki parsera/sieci pozostają dostępne przy stopie INFO. Błędy accept/watch zachowują sekundową przerwę nasłuchu. Bezpośrednie wywołania warning/error loggera nie mają globalnego limitu.

Treść rekordu ma do 1024 bajtów razem ze znacznikiem `...[truncated]`, poza prefiksem czasu/poziomu i końcowym LF. Wejście jest ograniczane przed tłumaczeniem, bez kopiowania dowolnie dużego widoku. utf8proc zamienia błędne bajty, znaki Cc/Cf (m.in. ESC, CR/LF/NUL i sterowanie bidi), separatory wiersza/akapitu na `?`. Pozostały tekst, w tym polski i emoji, zachowuje UTF-8. Ucięcie zachowuje granice kodów znaków, ale niekoniecznie całych grafemów. Źródło: [utf8proc](https://juliastrings.github.io/utf8proc/doc/utf8proc_8h.html).

## Ograniczenia operacyjne

Zapis jest synchroniczny. Dysk i zablokowany potok stderr mogą opóźnić reaktor; limity ograniczają ilość wpisów, nie czas nośnika. Obsługiwane są częściowe zapisy i EINTR, błędy działającego wyjścia są best effort, wyjątki nie wychodzą z loggera. Błąd otwarcia pliku przerywa start. Brak kolejki async, rotacji, reopen, dynamicznego poziomu, reload przez SIGHUP i zdalnego kolektora. Kontroluj wzrost pliku i używaj zaufanych katalogów. Mechanizmy nie zastępują limitów sieci, uwierzytelniania i ochrony DDoS. Testy opisuje [TESTING.md](TESTING.md).

## Security — alpha-0.1.7

Limity przyjęć, ramek i termin częściowej ramki mają własne kategorie ostrzeżeń na pięć sekund. Bezczynność i termin TLS pozostają niezależne. Nowe typowane zamknięcia to security_quota i partial_frame_timeout; TLS zachowuje handshake_timeout. Stop dodaje Security stopped; rejected=N closed=N; znaczenia podaje [SECURITY.md](SECURITY.md). IP klientów nadal nie są logowane, mimo ich użycia w pamięci limitera.
