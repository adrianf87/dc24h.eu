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

## Lokalne API — alpha-0.1.9

`api.unit` obejmuje ścisłe nagłówki, fragmentację w każdym prefiksie bajtów, kompletną granicę 8192 bajtów, duplikaty/niejednoznaczności, format/porównanie tokena, HEAD, deterministyczny korpus błędnych bajtów, jednorazowy start, 0600 gniazda, konkurencyjne serwery, zachowanie podmienionego inode i izolację błędu callbacku. `api.process` sprawdza rzeczywiste JSON/liczniki, autoryzację przed routingiem, 401/404/405/400/431, nagłówki ochronne, fragmenty, odrzucenie duplikatów/ramkowania/body/pipeline, wielu klientów, odzyskiwanie slotów, nieprzesuwalne terminy, aktywne zatrzymanie/restart, prywatne/błędne tokeny, stare/symboliczne/zwykłe ścieżki gniazda, kolizje, dry-run, wyłączenie, US/PL, rollback po błędzie Network oraz wymóg UID po poluzowaniu uprawnień plików (przypadek innego UID wymaga root, inaczej jest jawnie pomijany).

Oba testy rzeczywistego ncdc włączają API i odczytują rzeczywiste liczniki ADC/ADCS przed zatrzymaniem; nadal sprawdzają początkowe HSUP, nie pełne logowanie ADC. Test tymczasowej MariaDB sprawdza obecność włączonej bazy przez API i usuwanie gniazda. Testy API nie wymagają przeglądarki, curl ani dodatkowej usługi. CI zachowuje Ubuntu 22.04/24.04 Debug/Release; wyniki zdalne i inne systemy nie wynikają z lokalnych testów.

Lokalny wynik alpha-0.1.9: **20/20 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, w tym ncdc ADC/ADCS, tymczasowa MariaDB i odrzucenie innego UID, na Debian 13 / GCC 14.2 / OpenSSL 3.5.7 / MariaDB 11.8.6 / Connector/C 3.4.9. Sprawdzono instalację tymczasową, zgodność zainstalowanych INI/migracji, curl do zainstalowanego programu przez Unix z tokenem na stdin, względne ścieżki API, sprzątanie, UTF-8/linki/parzystość dokumentacji i białe znaki Git. Zdalne CI i inne systemy nie są tutaj oznaczone jako zweryfikowane.

## WebAdmin — alpha-0.1.10

Lokalnie: 20/20 CTest przeszło w Debug, Release i ASan/UBSan, w tym ncdc ADC/ADCS i tymczasowa MariaDB. Frontend: sprawdzenie TypeScript/build Vite, 3 testy jednostkowe Node, 3 testy Playwright oraz rzeczywista integracja C++ → Unix API → adapter Vite → Chromium zakończyły się sukcesem. Sprawdzono EN/PL i pamięć preferencji, desktop/mobile, dane dostępne/brakujące/błędne/utracone, eksport/nawigację, schemat i bezpieczne liczby, prywatne pliki/izolację katalogu, token tylko po stronie serwera, ochronę host/origin/site, przeciążenie oraz limity czasu/rozmiaru. Token nie pojawił się w żądaniach/HTML/JSON przeglądarki ani przez serwowanie plików Vite.

Przykład Apache2 przeszedł sprawdzenie składni i odizolowane serwowanie lokalne z CSP/blokadą API. Opcjonalna instalacja CMake zachowała dokładne bajty builda/przykładu/INI; zainstalowany program przyjął przykład. Audyt runtime npm nie zgłosił podatności w chwili wykonania; nie jest to audyt bezpieczeństwa. Środowisko: Debian 13, Node 20.19.2, npm 9.2, React 19.3.1, TypeScript 7.0.2, Vite 8.3.6, Chromium 156, Apache 2.4.69. CI ma osobny job Node 22, ale wyniki zdalne i inne systemy/przeglądarki nie są oznaczone jako zweryfikowane. Zobacz [WEBADMIN.md](WEBADMIN.md).

## Apache2 HTTPS — alpha-0.1.11

Lokalnie **21/21 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, w tym nowe `apache2.https`, rzeczywiste ncdc ADC/ADCS i tymczasowa MariaDB. Siedem przypadków Apache korzysta z polityki dostarczanego szablonu pod osobnym tymczasowym ServerRoot, na lokalnych efemerycznych portach, z jednorazowym certyfikatem/kluczem i zasobami fixture. Sprawdzono zaufane TLS 1.2/1.3, odrzucenie starszych wersji, stały redirect/ścieżki/query/Host, nagłówki sukcesu/błędów, HSTS tylko na HTTPS, blokady API/ukrytych ścieżek/sekretów/indeksów/symlinków, błędy zaufania/nazwy klienta, brak plików przy sprawdzaniu składni i rzeczywiste odrzucenie startu z niezgodną parą. Nie zmieniano witryn, zaufania, certyfikatów ani konfiguracji usługi hosta.

Wykrywanie Apache2/mod_ssl jest opcjonalne, a brak raportowany; CI instaluje Apache2. Typecheck/build frontendu, 3 testy Node, 3 przeglądarkowe i rzeczywista integracja C++/Vite/Chromium także przeszły z alpha-0.1.11. Instalacje tymczasowe sprawdziły zgodność szablonu HTTPS z opcjonalnymi zasobami WebAdmin i bez nich oraz poprawność zainstalowanego INI. Sprawdzono UTF-8/linki/parzystość języków i białe znaki Git. Lokalnie Apache 2.4.69 / OpenSSL 3.5.7 na Debian 13; zdalne CI i pozostałe systemy nie są oznaczone jako zweryfikowane. Zobacz [APACHE2_HTTPS.md](APACHE2_HTTPS.md).
