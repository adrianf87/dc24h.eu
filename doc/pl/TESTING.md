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
| `logging.unit` | Poziomy, czyszczenie/UTF-8, współbieżność, odnowienie budżetu i PL |
| `logging.process` | Zdarzenia TCP/TLS, prywatność, niezależne ostrzeżenia, INI i błędy |
| `security.unit` | Zakresy, okna sztucznego zegara, limity klucza/mapy, czyszczenie i sloty |
| `security.process` | Rzeczywiste IP/TLS, ramki/błędy, terminy bezwzględne i INI/CLI |
| `adc.ncdc` | Początkowy HSUP rzeczywistego ncdc przez zwykły TCP |
| `adcs.ncdc` | ncdc negocjuje TLS z C++; przezroczysty relay, jeden odszyfrowany HSUP, brak jawnego HSUP na łączu |

Testy ncdc mają osobne katalogi i PTY. Relay ADCS nie kończy TLS w Pythonie: szyfrowanie jest realizowane przez ncdc i C++. Zamknięcie po timeout synchronizuje odczyt liczników. Testy TLS ufają jednorazowemu certyfikatowi i weryfikują nazwę hosta. Brak close_notify serwera jest jawnie uwzględniony. Pełne logowanie ADC i stan NORMAL nie są obsługiwane ani deklarowane.

Lokalnie: 16/16 zestawów CTest w Debug, Release i ASan/UBSan na Debian 13 (GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7, ncdc 1.23.1). Szczegóły: [CHANGELOG](../../CHANGELOG.md). CI ma macierz Ubuntu 22.04/24.04 Debug/Release, ale sam plik workflow nie potwierdza zdalnego sukcesu. Debian 12 i Ubuntu 26.04 wymagają osobnej weryfikacji. Nie są to benchmarki, audyt, długotrwały fuzzing ani test DDoS. Nie ma pełnego wstrzykiwania błędów pamięci i kernela.

## Konfiguracja — alpha-0.1.5

`configuration.unit` sprawdza wartości domyślne, pierwszeństwo, ścieżki, BOM/CRLF, UTF-8/control/rozmiary, równoważne IPv6, samo TLS, brakujący klucz z CLI, append, poziomy i wiersze logu z wielu wątków. `configuration.process` sprawdza rzeczywiste limity/timeout, kontrolę bez zajmowania portu i tworzenia logu, ścieżki i kontrolę TLS, chronione pliki/hardlinki/alias katalogu, FIFO/symlink/katalog, oczyszczone błędy i polski język. Oba testy ncdc uruchamiają teraz C++ przez INI z nadpisaniami CLI.

## Logging — alpha-0.1.6

Unit sprawdza UTF-8 i granice ucięcia, znaki ASCII/Unicode/ANSI, poziomy, domyślną flagę, współbieżne budżety, odnowienie sekundy/raport destruktora i PL. Procesy sprawdzają pliki warning/error, start, 200 błędnych ramek z jednym ostrzeżeniem, niezależne limity/bezczynność, błędne/zawieszone/poprawne TLS, brak sekretów/adresów i pierwszeństwo CLI. ncdc ADC/ADCS ma teraz włączone logi i wymaga zdarzeń przyjęcia/zamknięcia lub gotowości TLS. Nadal jest to początkowy HSUP, bez pełnego logowania ADC.

## Security — alpha-0.1.7

Unit obejmuje dolne/górne zakresy, dokładne granice sekundy/minuty bez rzeczywistego czekania, sloty, długie klucze, zapełnienie 65536 wpisów, czyszczenie nieaktywnych i zachowanie aktywnych. Procesy obejmują niezależne źródła loopback, wspólne TCP/TLS, odzysk slotu, flood poprawnych/błędnych/keepalive, dokładne 16 KiB, parametry/UTF-8, powolne częściowe ramki, nowy termin po ukończeniu poprzedniej ramki w tym samym odczycie, odrzucanie za długość, powolne TLS i odzysk oraz wszystkie granice CLI/INI. ncdc ma ustawienia Security i wymaga braku odrzuceń/zamknięć. Test dużego transportu TLS jawnie podnosi tempo do 100000 dla weryfikacji transportu niezależnie od domyślnej ochrony. Nie jest to benchmark DDoS ani audyt.

Rzeczywisty dostarczany przykład INI jest sprawdzany przez `--check-config`; instalacja do katalogu tymczasowego potwierdza zgodność kopii i akceptację przez zainstalowany program.

## Database — alpha-0.1.8

`database.unit` sprawdza opcje i granice prywatnego hasła, CRLF, uprawnienia, symlinki, hardlinki, odrzucenie katalogów/FIFO i limit 4096 bajtów. `database.process` inicjuje tymczasową MariaDB z własnym katalogiem i `--no-defaults`, całkowicie wyłącza TCP i sprząta proces. Zainstaluj `mariadb-server`, aby go włączyć; CMake jawnie informuje o brakujących narzędziach. Testy nie korzystają z bazy hosta. Sprawdzane są rzeczywiste połączenia, Unicode i ciągi przypominające SQL injection jako parametry, NULL, graniczne/zbyt duże/błędne wyniki skalarne i dalsze użycie po błędzie, konflikt blokady migracji, pierwsza/powtórna migracja, SHA-256 źródła, zmienione/obce wpisy migracji, start/stop z bazą, brak skutków ubocznych sprawdzenia, komunikaty US/PL, brak/błędne/publiczne hasło, aliasy log/hasło i błędne INI/operacje. TLS bazy nie został zaimplementowany w tym etapie.

Lokalna weryfikacja alpha-0.1.8: wszystkie 18 zestawów przeszło w Debug, Release i ASan/UBSan na Debian 13, MariaDB 11.8.6 i Connector/C 3.4.9, w tym oba testy rzeczywistego ncdc. Zdalne CI i pozostałe kombinacje systemów/baz nie są oznaczone jako zweryfikowane.
