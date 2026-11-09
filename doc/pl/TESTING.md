# Testowanie

Uruchom `ctest --test-dir build --output-on-failure` i analogicznie dla `build-release`. Testy korzystają wyłącznie z loopback, portów 0 i danych tymczasowych, bez konfiguracji i udziałów użytkownika ncdc. Oczekiwania mają timeout i sprzątanie.

| Zestaw CTest | Zakres |
| --- | --- |
| `core.lifecycle` | Stany, jednorazowy run, stop przed startem, współbieżność, sygnały |
| `core.process` | CLI, wersja, pomoc, języki US/PL, SIGINT/SIGTERM i serie sygnałów |
| `network.unit` | Adresy, limity, stop i brak wycieków deskryptorów przy błędzie startu |
| `network.process` | 48 klientów, wiele portów, IPv6, limit i ponowne użycie slotu, FIN/reset, timeout, flood i limit deskryptorów |
| `adc.parser` | Nagłówki/komendy, escape, UTF-8/NFC, limity, fragmentacja, ucięcie i korpus losowy |
| `adc.process` | Strumienie, resynchronizacja, niezależne bufory i brak sekretów w logach |
| `adcs.crypto` | Negocjacja i odszyfrowanie OpenSSL z wcześniej zapełnioną kolejką błędów |
| `adcs.process` | Weryfikacja nazwy/certyfikatu, TLS 1.2/1.3, samo TLS, mieszane porty, fragmentacja, odrzucenie starego TLS, timeout negocjacji, błędne/zaszyfrowane klucze, reset w trakcie zapisu TLS, wielu klientów, duże strumienie i stop |
| `configuration.unit` | Składanie INI/CLI, ścieżki, granice i współbieżność loggera |
| `configuration.process` | Rzeczywiste limity, konfiguracja TLS, tryb sprawdzania i bezpieczeństwo plików |
| `adc.ncdc` | Początkowy HSUP rzeczywistego ncdc przez zwykły TCP |
| `adcs.ncdc` | ncdc negocjuje TLS z C++; przezroczysty relay, jeden odszyfrowany HSUP, brak jawnego HSUP na łączu |

Testy ncdc mają osobne katalogi i PTY. Relay ADCS nie kończy TLS w Pythonie: szyfrowanie jest realizowane przez ncdc i C++. Zamknięcie po timeout synchronizuje odczyt liczników. Testy TLS ufają jednorazowemu certyfikatowi i weryfikują nazwę hosta. Brak close_notify serwera jest jawnie uwzględniony. Pełne logowanie ADC i stan NORMAL nie są obsługiwane ani deklarowane.

Lokalnie: 12/12 zestawów CTest w Debug, Release i ASan/UBSan na Debian 13 (GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7, ncdc 1.23.1). Szczegóły: [CHANGELOG](../../CHANGELOG.md). CI ma macierz Ubuntu 22.04/24.04 Debug/Release, ale sam plik workflow nie potwierdza zdalnego sukcesu. Debian 12 i Ubuntu 26.04 wymagają osobnej weryfikacji. Nie są to benchmarki, audyt, długotrwały fuzzing ani test DDoS. Nie ma pełnego wstrzykiwania błędów pamięci i kernela.

## Konfiguracja — alpha-0.1.5

`configuration.unit` sprawdza wartości domyślne, pierwszeństwo, ścieżki, BOM/CRLF, UTF-8/control/rozmiary, równoważne IPv6, samo TLS, brakujący klucz z CLI, append, poziomy i wiersze logu z wielu wątków. `configuration.process` sprawdza rzeczywiste limity/timeout, kontrolę bez zajmowania portu i tworzenia logu, ścieżki i kontrolę TLS, chronione pliki/hardlinki/alias katalogu, FIFO/symlink/katalog, oczyszczone błędy i polski język. Oba testy ncdc uruchamiają teraz C++ przez INI z nadpisaniami CLI.
