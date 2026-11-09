# API

## Wewnętrzne C++ w alpha-0.1.4

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
