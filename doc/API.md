# API

## Wewnętrzne C++ w alpha-0.1.2

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
- `Options { listeners, max_connections, idle_timeout }` — konfiguracja TCP;
  konstruktor sprawdza zakresy, bez otwierania portów.
- `TcpServer(Logger&, Options = {})` — tworzy deskryptor budzenia `eventfd`.
- `run()` — jednorazowo otwiera listenery, obsługuje klientów i zamyka gniazda
  przy stopie/błędzie. Drugie wywołanie rzuca `logic_error`.
- `request_stop() noexcept` — atomowa flaga i nieblokujące budzenie; bezpieczne
  współbieżnie, również przed `run()`.
- `statistics()` — niezależnie odczytane atomowe liczniki `active`, `accepted`,
  `rejected`, `received_bytes`. Nie stanowią wspólnego transakcyjnego snapshotu.

Właściciel dołącza wątek `run()` i wszystkich wywołujących `request_stop()` przed
zniszczeniem obiektów. Destruktor nie zastępuje stop/join. Gniazda TCP należą
wyłącznie do wątku pętli; `request_stop()` nie zamyka ich z innego wątku.
Transport nie udostępnia jeszcze callbacków wiadomości ani zapisu do klienta.

`TerminationSignals` jest adapterem procesu dla Linux; ustawienie maski należy
wykonać w głównym wątku przed utworzeniem innych wątków.

## Plan API WebAdmin

Nie istnieją jeszcze endpointy HTTP, uwierzytelnianie API ani kontrakt JSON.
Docelowo API C++ będzie dostępne przez zabezpieczony reverse proxy Apache2,
z uwierzytelnianiem, kontrolą ról, walidacją danych i rejestrem operacji
administracyjnych.
