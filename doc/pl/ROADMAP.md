# Plan rozwoju

| Etap | Zaimplementowany zakres |
| --- | --- |
| alpha-0.1.1 Core | Cykl życia, start/stop, sygnały i logger |
| alpha-0.1.2 Network | TCP IPv4/IPv6, wiele portów i klientów, limity zasobów |
| alpha-0.1.3 ADC | Ograniczony parser strumienia, komendy i walidacja danych |
| alpha-0.1.4 ADCS | Certyfikaty, nieblokujący TLS, testy, dokumentacja i komunikaty US/PL |
| alpha-0.1.5 Configuration | INI, nadpisania CLI, walidacja ścieżek/limitów i logowanie append |
| alpha-0.1.6 Logging | Podsumowania startu, WARN, zdarzenia połączeń, ograniczone UTF-8 i budżety |

Kolejne wersje wymagają osobnego zakresu i kryteriów testowych:

1. Serializer ADC, ograniczone kolejki wyjściowe, backpressure, SID i automat sesji; pełne logowanie ncdc.
2. close_notify, przeładowanie certyfikatów, telemetria i asynchroniczne/rotowane logi.
3. Użytkownicy, uwierzytelnianie, role, bany i migracje MariaDB.
4. Chat publiczny/prywatny, wyszukiwanie i koordynacja transferów.
5. Zabezpieczone HTTP API oraz React/TypeScript/Vite WebAdmin przez Apache2.

W ramach tego zadania nie importowano wersji późniejszych niż alpha-0.1.6. Istnienie innych gałęzi zdalnych nie potwierdza gotowości ich kodu.

## alpha-0.1.5

Zaimplementowano: jawny INI, nadpisania CLI, walidację nasłuchów/limitów/ścieżek, tryb sprawdzania i logowanie append z wyborem poziomu. Późniejsze wersje z innych gałęzi pozostają poza zakresem zadania.
