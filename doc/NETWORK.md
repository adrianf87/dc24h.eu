# Sieć i proces

Alpha-0.1.1 nie tworzy gniazd i nie nasłuchuje na żadnym porcie.
Nie należy ogłaszać tego procesu jako działającego huba ADC/ADCS.

| Sygnał | Obsługa |
| --- | --- |
| SIGINT (Ctrl+C) | Kontrolowany stop, kod 0 |
| SIGTERM | Kontrolowany stop, kod 0 |
| Powtarzane SIGINT/SIGTERM | Brak ponownego sprzątania i eskalacji |
| SIGHUP | Brak własnej obsługi; nie służy do reload |
| SIGKILL/SIGSTOP | Nie są przechwytywane |

Przyszły Network będzie odpowiadał za TCP, limity klientów, bufory, timeouty
i izolowanie błędów pojedynczego połączenia. ADC i ADCS otrzymają jawnie
konfigurowane adresy i porty; obecnie nie przypisano wartości domyślnych.
