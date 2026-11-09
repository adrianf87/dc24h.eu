# Konfiguracja

alpha-0.1.4 obsługuje tylko CLI; brak pliku konfiguracji i loadera zmiennych środowiskowych.

| Opcja | Domyślnie | Znaczenie |
| --- | --- | --- |
| `--listen IPv4:port` / `--listen [IPv6]:port` | `127.0.0.1:1511` | Zwykły TCP, opcja powtarzalna |
| `--adcs-listen ADRES:port` | Brak | TLS, opcja powtarzalna |
| `--tls-cert ŚCIEŻKA` | Brak | Łańcuch certyfikatów PEM |
| `--tls-key ŚCIEŻKA` | Brak | Pasujący nieszyfrowany klucz PEM |
| `--max-connections N` | 1024 | Wspólny limit 1..65535 |
| `--idle-timeout SEKUNDY` | 120 | Zakres 1..86400; negocjacja TLS do 10 s |
| `--language us\|pl` | `us` | Język pomocy i komunikatów |
| `--help` / `--version` | — | Pomoc / wersja, bez startu Core |

Pierwszy jawny nasłuch TCP lub TLS zastępuje domyślny. Samo ADCS nie otwiera zwykłego TCP. Łącznie do 16 nasłuchów; port 0 wybiera system. Wildcard wymaga jawnego adresu. Ostatnia wartość powtórzonej opcji skalarnej obowiązuje. ADCS wymaga obu ścieżek TLS; ścieżki bez ADCS są błędem. Zaszyfrowany klucz nie wywołuje pytania o hasło, lecz błąd startu.

Pomoc/wersję można łączyć z `--language`, ale nie z opcjami operacyjnymi. Kody wyjścia: 0 poprawny stop/pomoc/wersja; 2 błędne argumenty; 1 błąd wykonania. Błędne wartości wejściowe nie są wypisywane. Katalogi UTF-8: `languages/us` i `languages/pl`. Błędy systemowe bez tłumaczenia pozostają angielskie. Dane protokołu, nazwy komend i pól liczników nie są tłumaczone.

Logi UTC `YYYY-MM-DDTHH:MM:SSZ [INFO|ERROR] komunikat` trafiają na stderr; logger synchronizuje wątki i nie przerywa sprzątania po błędzie wyjścia. Brak rotacji i plików logów. Parser ma stałe limity 16384 bajtów treści i 128 parametrów. Nie loguje parametrów ani haseł. SIGHUP nie przeładowuje ustawień ani certyfikatów.
