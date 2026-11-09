# Konfiguracja

Alpha-0.1.3 konfiguruje Network przez CLI. Nie ma parsera plików, zmiennych
środowiskowych aplikacji ani dynamicznego przeładowania.

| Opcja | Domyślnie | Znaczenie |
| --- | --- | --- |
| `--listen IPv4:port` lub `--listen '[IPv6]:port'` | `127.0.0.1:1511` | Numeryczny adres i port 0..65535; do 16 wystąpień |
| `--max-connections N` | `1024` | Łączny limit aktywnych klientów: 1..65535 |
| `--idle-timeout SECONDS` | `120` | Timeout bez odczytu danych: 1..86400 sekund |
| `--help` | — | Pomoc, bez uruchamiania Core |
| `--version` | — | Nazwa i wersja kompilacji |

Pierwsze `--listen` zastępuje adres domyślny. Powtórzone opcje limitów przyjmują
ostatnią wartość. `--help` i `--version` muszą być jedynym argumentem.
Adresy wildcard `0.0.0.0` i `[::]` wymagają jawnego podania. Port 0 jest
przeznaczony np. do testów; faktyczny port trafia do logu gotowości.

Kod zakończenia: 0 po poprawnym stop lub pomocy/wersji, 2 po błędnych
argumentach, 1 po błędzie wykonania (np. zajęty port). Argumenty są sprawdzane
przed otwarciem listenerów. Start wszystkich portów jest atomowy względem
gotowości: nieudany start jednego zamyka pozostałe.

Logi mają format `YYYY-MM-DDTHH:MM:SSZ [INFO|ERROR] komunikat` i trafiają
na stderr. Logger synchronizuje wątki i działa best-effort; błąd wyjścia nie
przerywa sprzątania. Rotacja i plikowe logowanie nie są zaimplementowane.
Treść danych sieciowych i niepoprawne wartości CLI nie są zapisywane w logach.

SIGHUP nie przeładowuje ustawień i zachowuje domyślną semantykę systemową.
Szczegóły limitów i TCP: [NETWORK.md](NETWORK.md).

Parser ADC ma obecnie stałe limity aplikacji: 16384 bajty treści (bez LF)
i 128 parametrów (bez pól routingu). Nie ma opcji CLI do ich zmiany.
Każdy klient ma osobny ograniczony bufor. Niepoprawna ramka jest ignorowana;
przekroczenie rozmiaru przełącza dekoder w odrzucanie aż do LF.
Na stopie dodatkowy log `ADC stopped` podaje liczniki ramek poprawnych
składniowo, nieznanych komend, błędnych ramek i keepalive. Są to liczniki
parsera, bez potwierdzenia wykonania komendy. Szczegóły: [ADC_PROTOCOL.md](ADC_PROTOCOL.md).
