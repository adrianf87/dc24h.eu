# API

## Wewnętrzne C++ w alpha-0.1.1

Nagłówek `dc24h/core/server.hpp`:

- `Server(logging::Logger&)` — nowy Core; logger musi żyć dłużej niż obiekt.
- `run()` — blokujący, jednorazowy start, zakończony po żądaniu stop.
- `request_stop()` — bezpieczne współbieżnie, idempotentne żądanie stop.
- `state()` — spójny odczyt `created`, `running`, `stopping` lub `stopped`.

Właściciel dołącza wątek wykonujący `run()` przed zniszczeniem obiektu.
`TerminationSignals` jest adapterem procesu dla Linux; ustawienie maski należy
wykonać w głównym wątku przed utworzeniem innych wątków.

## Plan API WebAdmin

Nie istnieją jeszcze endpointy HTTP, uwierzytelnianie API ani kontrakt JSON.
Docelowo API C++ będzie dostępne przez zabezpieczony reverse proxy Apache2,
z uwierzytelnianiem, kontrolą ról, walidacją danych i rejestrem operacji
administracyjnych. Nie udostępniaj niezabezpieczonego endpointu zarządzającego.
