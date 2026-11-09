# Plan rozwoju

| Etap | Zaimplementowany zakres |
| --- | --- |
| alpha-0.1.1 Core | Cykl życia, start/stop, sygnały i logger |
| alpha-0.1.2 Network | TCP IPv4/IPv6, wiele portów i klientów, limity zasobów |
| alpha-0.1.3 ADC | Ograniczony parser strumienia, komendy i walidacja danych |
| alpha-0.1.4 ADCS | Certyfikaty, nieblokujący TLS, testy, dokumentacja i komunikaty US/PL |
| alpha-0.1.5 Configuration | INI, nadpisania CLI, walidacja ścieżek/limitów i logowanie append |
| alpha-0.1.6 Logging | Podsumowania startu, WARN, zdarzenia połączeń, ograniczone UTF-8 i budżety |
| alpha-0.1.7 Security | Ograniczone przyjęcia IP, ramki i terminy bezwzględne |
| alpha-0.1.8 Database | Opcjonalne połączenie lokalne MariaDB, prepared statements i jawny rejestr migracji |
| alpha-0.1.9 API | Lokalne uwierzytelnione HTTP/JSON: stan i health, tylko odczyt |
| alpha-0.1.10 WebAdmin | Układ React/TypeScript/Vite tylko do odczytu, EN/PL, lokalny adapter i statyczny przykład Apache |

Kolejne wersje wymagają osobnego zakresu i kryteriów testowych:

1. Serializer ADC, ograniczone kolejki wyjściowe, backpressure, SID i automat sesji; pełne logowanie ncdc.
2. close_notify, przeładowanie certyfikatów, telemetria i asynchroniczne/rotowane logi.
3. Użytkownicy, Argon2id, role/bany, migracje tabel aplikacji i zweryfikowany zdalny TLS bazy.
4. Chat publiczny/prywatny, wyszukiwanie i koordynacja transferów.
5. Operacje API zmieniające stan/role/audyt, uwierzytelniona brama Apache2 oraz produkcyjne sesje/role WWW.

Wersje późniejsze niż alpha-0.1.10 pozostają planowane. Inne gałęzie nie potwierdzają gotowości wydania.

## alpha-0.1.5

Zaimplementowano: jawny INI, nadpisania CLI, walidację nasłuchów/limitów/ścieżek, tryb sprawdzania i logowanie append z wyborem poziomu. Późniejsze wersje z innych gałęzi pozostają poza zakresem zadania.
