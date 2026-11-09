# Audyt alpha-0.1 i przygotowanie alpha-0.2

Data audytu: 2026-11-09. Etap: **alpha-0.2.1 Project**. Podstawa: `main` w commicie `8277bc14ea3a722fdfc72f600be3e9cb5f2d9158` (po scaleniu alpha-0.1.11).

## Wniosek i zakres

Jedenaście etapów alpha-0.1 daje osobno testowane podstawy transportu C++20, parsera, konfiguracji, logowania, zabezpieczeń, SQL, lokalnego API i frontendu. Serwer nie potrafi jeszcze ukończyć zwykłego logowania klienta ADC. Działający hub wymaga przede wszystkim wysyłania danych i semantyki sesji. Zielone testy transportu nie potwierdzają pełnej zgodności ADC ani gotowości produkcyjnej.

To przegląd źródeł/konfiguracji i odtwarzalny audyt regresji z prawdziwym ncdc, tymczasową MariaDB, osobnym Apache i integracją przeglądarkową. Nie obejmuje niezależnego pentestu, wyczerpującej analizy zależności, długotrwałego obciążenia ani kampanii fuzzingu. Etap naprawia wykazane drobne błędy i luki w weryfikacji; nowe funkcje huba pozostają planowane.

## Inwentaryzacja modułów

| Obszar | Sprawdzona implementacja i dowody | Zakres nadal do wykonania |
| --- | --- | --- |
| Core | Cykl życia, synchroniczne sygnały, zatrzymywanie i cofanie nieudanego startu; `src/core/`, `tests/process_tests.py` | Osobna gotowość transportu/sesji; health oznacza obecnie odpowiedź API |
| Network | Nieblokujący epoll, IPv4/IPv6, RAII, ograniczone odczyty/przyjęcia, limity i terminy; `src/network/tcp_server.cpp`, testy network/security | Bufory wyjściowe, częściowe zapisy, backpressure i polityka wolnych odbiorców |
| ADC | Ramki LF, limity, UTF-8/NFC, escape, nagłówki i reprezentacja nieznanych komend; `src/adc/parser.cpp`, testy ADC | Serializer, negocjacja SUP/hash, SID, walidacja stanu/tożsamości i przekazywanie komend |
| ADCS | Nieblokujący handshake/odczyt OpenSSL, minimum TLS 1.2, weryfikacja pary klucza; `src/adcs/tls.cpp`, testy ADCS/ncdc | Zapis/retry, ograniczone close_notify i reload certyfikatów; domyślne szyfry zależą od lokalnej polityki OpenSSL |
| Configuration / Logging | Ograniczony INI, pierwszeństwo CLI, kolizje ścieżek, oczyszczone rekordy i budżety zdarzeń; odpowiednie testy | Reload, rotacja i asynchroniczne wyjście; wolny zapis synchroniczny może blokować wywołujący wątek |
| Security / języki | Ograniczone liczniki IP/ramek, terminy bezwzględne, pliki prywatne, katalogi komunikatów US/PL; `src/security/`, testy security/API | Uwierzytelnianie użytkowników, uprawnienia i bany; limity nie dowodzą odporności DDoS |
| Database | Jawne opcjonalne połączenie Unix, prepared statements, prywatne hasło, migracja z sumą; `src/database/database.cpp`, tymczasowy SQL | Migracja tworzy tylko rejestr; brak kont/ról/banów, kolejki zapytań i zweryfikowanego zdalnego TLS SQL |
| API | Prywatny socket/token tego samego UID, ograniczone HTTP/JSON tylko do odczytu; `src/api/`, testy API | Brak zdalnego API, sesji/ról, operacji zapisu/audytu i bieżącego health SQL; liczniki próbkowane niezależnie |
| WebAdmin | Typowany panel EN/PL tylko do odczytu i lokalny adapter Vite; `webadmin/src/`, `server/`, testy unit/browser/live | Produkcyjne logowanie WWW, brama Apache, role i zapis; token usługi musi pozostać poza zasobami przeglądarki |
| Apache2 | Lokalny przykład statycznego HTTPS, przekierowanie i testy dostępu/TLS; `config/apache2-webadmin-https.conf.example`, test Apache | Wdrożenie/odnowienie certyfikatów przez operatora i uwierzytelniona brama API; przykład nie aktywuje witryny |
| Users / Chat / Search / Transfers | Wyłącznie architektura i przyszły zakres | Brak implementacji i ukończonych kryteriów odbioru |

Odniesienie: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), zwłaszcza składnia linii i zarządzanie stanem klient–hub. Obecny parser sprawdza składnię z bardziej restrykcyjną lokalną polityką znaków sterujących; rozpoznanie komendy nie realizuje jej semantyki. Testy ncdc kończą się po początkowym HSUP i sprawdzeniu liczników transportu.

## Ustalenia i decyzje

P1 blokuje wskazaną następną funkcję lub szersze wdrożenie. P2 oznacza lukę niezawodności/weryfikacji. P3 dotyczy spójności dokumentacji. Otwarte pozycje są opisanymi ograniczeniami, a nie twierdzeniem o odkryciu nowych podatności.

| ID | Priorytet | Dowód / skutek | Stan i kryterium odbioru |
| --- | --- | --- | --- |
| A01 | P3 | `App.tsx` wskazywał dokumentację alpha-0.1.10; główny README twierdził, że frontend nie istnieje | **Zamknięte:** link wynika z WEBADMIN_VERSION; regresja przeglądarkowa sprawdza adresy US/PL; README poprawiony |
| A02 | P2 | CMake pomijał testy SQL, Apache i ncdc przy braku narzędzi, również w CI | **Zamknięte:** `DC24H_REQUIRE_INTEGRATION_TESTS=ON` przerywa konfigurację przy brakach; CI włącza opcję. Domyślne lokalne budowanie zachowuje opcjonalność |
| A03 | P2 | Spójność wersji i dokumentacji sprawdzano ręcznie | **Zamknięte:** `project.metadata` porównuje CMake/npm/lock/frontend, wersje we wstępach, UTF-8, nazwy plików Markdown US/PL i lokalne cele linków bez Node/Git. Nie sprawdza sensu tłumaczeń, nagłówków ani adresów zewnętrznych |
| A04 | P3 | Plan zachował stare sformułowania zadania alpha-0.1.5 i nie miał kryteriów alpha-0.2 | **Zamknięte:** nowy plan, zasady wersji i dwujęzyczny audyt określają podstawę oraz proponowane bramki odbioru |
| A10 | P1 | `TcpServer::run` dekoduje wejście, ale nie wysyła odpowiedzi ADC; brak menedżera sesji | **Otwarte / pierwsza funkcja alpha-0.2:** ograniczony serializer/zapis, następnie SID/SUP/INF i walidacja stanu/tożsamości. Wymagane pełne logowanie ncdc przez ADC i ADCS |
| A11 | P2 | Reaktor skanuje połączenia pod kątem TLS/terminów; logger zapisuje synchronicznie | **Otwarte:** zmierzyć sprawiedliwość obsługi, pamięć i opóźnienie stop przy wielu klientach; odseparować wolne logi. Dodać wolnych odbiorców do testów kolejek przed rozsyłaniem danych |
| A12 | P2 | core_running, health i database.enabled nie oznaczają gotowości nasłuchów/sesji ani bieżącego SQL | **Otwarte:** zdefiniować osobne stany i błędy gotowości przed monitoringiem operacyjnym; zachować znaczenie v1 |
| A13 | P1 | Vite jest lokalnym adapterem deweloperskim; Apache blokuje API i udostępnia statyczne pliki | **Otwarte / przed administracją zdalną:** logowanie WWW, brama z minimalnymi uprawnieniami, sesje/role, polityka CSRF/origin i audyt. Bez tokenu usługi w przeglądarce |
| A14 | P1 | Migracja 1 przechowuje jedynie rejestr migracji; brak kont/ról i obsługi haseł użytkowników | **Otwarte / przed kontami:** wersjonowany schemat aplikacji, projekt przechowywania haseł i testy autoryzacji. Nie zmieniać zastosowanej migracji bazowej |
| A15 | P2 | Lokalnie sprawdzono Debian 13; macierz CI obejmuje Ubuntu 22.04/24.04 | **Otwarte:** osobno zweryfikować Debian 12 i Ubuntu 26.04; dodać fuzzing, obciążenie, fault injection i przegląd zależności. Deklaracja wersji systemu nie jest wynikiem testu |

## Dowody i odtworzenie

Historyczny push GitHub Actions [42561475283](https://github.com/adrianf87/dc24h.eu/actions/runs/42561475283) zakończył się sukcesem dla dokładnego SHA main podanego wyżej: Ubuntu 22.04/24.04 Debug/Release oraz frontend. Ten zaobserwowany wynik nie oznacza wyniku CI nowej gałęzi.

Lokalna podstawa: 21/21 testów CTest w Release oraz produkcyjny build frontendu / 3 testy Node. `npm audit --json` nie zgłosił podatności dla zablokowanego drzewa zależności produkcyjnych i deweloperskich w dniu audytu; wynik rejestru nie obejmuje bibliotek C++/systemu i nie gwarantuje braku błędów. Wersje zależności zachowano.

Końcowe sprawdzenia etapu zapisano w [CHANGELOG.md](../../CHANGELOG.md). Odtworzenie z zainstalowanymi narzędziami integracyjnymi:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 tests/project_checks.py
npm --prefix webadmin ci
npm --prefix webadmin run build
npm --prefix webadmin test
npm --prefix webadmin run test:e2e
npm --prefix webadmin run test:integration -- ../build/dc24h
npm --prefix webadmin audit
```

Playwright wymaga własnego Chromium albo `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`. Release należy budować w osobnym katalogu; flagi sanitizerów opisuje [BUILD.md](BUILD.md). W lokalnym audycie dodano `-fno-pie` / `-no-pie` do budowania z sanitizerami. Próby negatywne wyłączają każdą rodzinę narzędzi przez wpisy cache CMake: tryb wymagany odrzuca konfigurację, opcjonalny informuje o pominięciu. Tryb wymagany z BUILD_TESTING=OFF jest odrzucany. Nowy test metadanych sprawdzono na tymczasowych niespójnych wersjach, brakujących tłumaczeniach i błędnych lokalnych linkach.

Środowisko: Debian 13, GCC 14.2, CMake 3.31.6, OpenSSL 3.5.7, MariaDB 11.8.6 / Connector/C 3.4.9, ncdc 1.23.1, Apache 2.4.69, Node 20.19.2, npm 9.2 i Chromium 156. Testy używają własnych tymczasowych usług i poświadczeń. Konfiguracja systemowego Apache/bazy nie jest aktywowana.

## Przekazanie gałęzi

`alpha-0.2.1` powstaje z badanego SHA main i zawiera raport, ograniczone poprawki, kontrole regresji i wersję 0.2.1. `alpha-0.2` wskazuje ten sam sprawdzony commit jako znacznik linii. Żadna gałąź nie scala się samoczynnie do main. Nowe numerowane etapy nadal powstają z aktualnego main zgodnie z poleceniem; każdy sprawdzony etap należy osobno scalić przed rozpoczęciem następnego. Historia alpha-0.1 i bajty migracji SQL pozostają zachowane.

Proponowane kolejne etapy i mierzalne kryteria znajdują się w [ROADMAP.md](ROADMAP.md). Ich numery/zakres to plan, a nie zaimplementowane funkcje.
