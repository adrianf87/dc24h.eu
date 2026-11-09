# API

## Wewnętrzne C++ w alpha-0.1.5

Nagłówek `dc24h/core/server.hpp`:

- `Server(logging::Logger&, network::TcpServer* = nullptr)` — Core z opcjonalnym
  transportem. Logger i wskazany Network muszą żyć dłużej niż Core.
- `run()` — blokujący, jednorazowy start. Bez transportu czeka na zmiennej
  warunkowej; z transportem wykonuje jego pętlę. Wyjątek Network ustawia
  `stopped` i jest przekazywany do wywołującego.
- `request_stop()` — bezpieczne współbieżnie, idempotentne żądanie stop,
  przekazywane również do Network. Żądanie przed startem jest zapamiętywane.
- `state()` — spójny odczyt `created`, `running`, `stopping` lub `stopped`.

Nagłówek `dc24h/network/tcp_server.hpp`:

- `Endpoint { address, port }`, `parse_endpoint(string_view)` — numeryczne
  IPv4:port / [IPv6]:port, walidacja przez `inet_pton` i `from_chars`.
- `Options { listeners, tls_listeners, tls_certificate, tls_private_key, max_connections, idle_timeout }` — konfiguracja TCP;
  konstruktor sprawdza zakresy, bez otwierania portów.
- `TcpServer(Logger&, Options = {})` — tworzy deskryptor budzenia `eventfd`.
- `run()` — jednorazowo otwiera listenery, obsługuje klientów i zamyka gniazda
  przy stopie/błędzie. Drugie wywołanie rzuca `logic_error`.
- `request_stop() noexcept` — atomowa flaga i nieblokujące budzenie; bezpieczne
  współbieżnie, również przed `run()`.
- `statistics()` — niezależnie odczytane atomowe liczniki `active`, `accepted`,
  `rejected`, `received_bytes`, `adc_frames`, `adc_unknown`, `adc_invalid`,
  `adc_keepalives`, `adcs_handshakes`, `adcs_incomplete`. Nie stanowią wspólnego transakcyjnego snapshotu.

Właściciel dołącza wątek `run()` i wszystkich wywołujących `request_stop()` przed
zniszczeniem obiektów. Destruktor nie zastępuje stop/join. Gniazda TCP należą
wyłącznie do wątku pętli; `request_stop()` nie zamyka ich z innego wątku.
Transport nie udostępnia jeszcze callbacków wiadomości ani zapisu do klienta.

`TerminationSignals` jest adapterem procesu dla Linux; ustawienie maski należy
wykonać w głównym wątku przed utworzeniem innych wątków.

## Parser ADC

Nagłówek `dc24h/adc/parser.hpp` udostępnia `Message`, `MessageType`, `Command`,
`FeatureSelector`, `ParseError`, wariant `ParseResult`, `parse_line`,
`recognize_command` oraz `StreamDecoder` z `feed`, `finish` i `buffered_bytes`.
Szczegółowy kontrakt, zasady własności, limity i semantyka błędów:
[ADC_PROTOCOL.md](ADC_PROTOCOL.md). Dekoder jest jednowątkowy; niezależne
instancje oraz `parse_line` można używać równolegle.

## Plan API WebAdmin

Nie istnieją jeszcze endpointy HTTP, uwierzytelnianie API ani kontrakt JSON.
Docelowo API C++ będzie dostępne przez zabezpieczony reverse proxy Apache2,
z uwierzytelnianiem, kontrolą ról, walidacją danych i rejestrem operacji
administracyjnych.

## ADCS i języki

`adcs::Context` posiada kontekst TLS. `adcs::Connection` wymaga, by fd i kontekst żyły dłużej niż sesja. `handshake()` i `receive()` zwracają complete/want_read/want_write/closed/error wraz z liczbą bajtów. `pending()` zgłasza odszyfrowane dane w buforze. Pętla odpowiada za gotowość gniazda i obsługę SIGPIPE. Brak API zapisu. `Logger(Language = us)` i `i18n::parse_language/translate/help` dostarczają komunikaty US/PL.

## Konfiguracja i logger

`configuration::Settings` zawiera opcje sieci/loggera i pochodzenie (`source_file`, `implicit_plain_listener`). `load_file` parsuje składnię i pojedyncze pola; zależności sprawdza osobne `validate(Settings)` po nadpisaniach. `parse_arguments(argc, argv)` wczytuje plik, stosuje CLI i waliduje; `CommandLine` dodaje flagi check/help/version. `logging::Options` ma level, file_path (puste = stderr), language i connections. `Logger(Options = {})` otwiera plik w konstruktorze; pozostaje też `Logger(Language)`. Obiekt jest niekopiowalny i musi żyć dłużej niż wywołujący. Brak API zmiany ustawień/reload.

## Logger — alpha-0.1.6

`Level` ma info/warning/error/off. `info`, `warning`, `error` przyjmują niesekretny tekst wewnętrzny i nie rzucają. `connection(ConnectionEvent, uint64_t id, bool secure)` dopuszcza tylko enum zdarzeń, bez dowolnej treści. `connection_enabled()` sprawdza niezmienną flagę i próg INFO. Mutex chroni wspólny limit 64 wpisów/okno sekundy i raport pominięć w destruktorze/przy odnowieniu. Kolejność Options: level, file_path, language, connections; wcześniejsze inicjalizacje pozostają zgodne. Szczegóły: [LOGGING.md](LOGGING.md).

## Security — alpha-0.1.7

`network::Options::security` zawiera sześć pól `security::Limits`. `security::validate` odrzuca wartości poza zakresem. `PeerLimiter(Limits)` należy do reaktora: `admit(kanoniczny_klucz, czas_monotoniczny)` przyznaje aktywny slot/przyjęcie; po błędzie/zamknięciu wykonaj dokładnie jedno `release(key)` dla każdego sukcesu. Długość klucza i mapa są ograniczone; `tracked_peers()` zwraca liczbę wpisów. `FrameLimiter(maximum)::admit(czas)` daje okno sekundy na klienta. Czasy muszą być monotoniczne. `Statistics` dodaje niezależne atomowe security_rejected/security_closed. Brak współdzielenia stanu między instancjami/procesami. Szczegóły: [SECURITY.md](SECURITY.md).
