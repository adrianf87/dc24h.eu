# Konfiguracja

W alpha-0.1.1 nie ma konfiguracji plikowej, zmiennych środowiskowych aplikacji
ani dynamicznego przeładowania. Nie podawaj jeszcze portów, haseł czy ścieżek
certyfikatów: parser takich ustawień nie istnieje.

| Wywołanie | Działanie | Kod |
| --- | --- | --- |
| `dc24h` | Start Core na pierwszym planie | 0 po poprawnym stop; 1 po błędzie |
| `dc24h --help` | Pomoc, bez uruchamiania Core | 0 |
| `dc24h --version` | Nazwa i wersja kompilacji | 0 |
| Nieznane lub dodatkowe argumenty | Błąd użycia na stderr | 2 |

Logi mają format `YYYY-MM-DDTHH:MM:SSZ [INFO|ERROR] komunikat` i trafiają
na stderr. Logger synchronizuje wątki i działa best-effort; błąd wyjścia nie
przerywa sprzątania. Rotacja i plikowe logowanie nie są zaimplementowane.

Plan: walidowana konfiguracja Network/TLS/Database/API, bez sekretów w Git.
SIGHUP nie przeładowuje ustawień i zachowuje domyślną semantykę systemową.
