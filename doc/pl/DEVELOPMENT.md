# Rozwój projektu

Podstawę alpha-0.2 i priorytety odbioru opisuje [audyt alpha-0.1](AUDIT_ALPHA_0.1.md). Numerowane gałęzie nadal powstają z aktualnego main. `alpha-0.2` oznacza sprawdzony commit audytu. Pełna weryfikacja używa `DC24H_REQUIRE_INTEGRATION_TESTS=ON`; zachować spójność project.metadata, npm lock, wersji programu/panelu i obu języków dokumentacji.

Pracuj na gałęziach wydań, zachowując historię `main`. Aktualny etap:
`alpha-0.2.2`. Każdy istotny etap otrzymuje opisowy commit. Nie dodawaj
artefaktów kompilacji, sekretów ani zależności vendored bez uzasadnienia.

| Katalog | Rola |
| --- | --- |
| `src/core`, `include/dc24h/core` | Core i adapter sygnałów |
| `src/network`, `include/dc24h/network` | TCP, listenery, klienci i epoll |
| `src/adc`, `include/dc24h/adc` | Parser składni i dekoder strumienia ADC |
| `src/logging`, `include/dc24h/logging` | Logowanie |
| `cmake/` | Generowana informacja o wersji |
| `tests/` | Testy C++ i testy procesu w Pythonie |
| `config/` | Przykładowy INI i lokalne ustawienia |
| `src/database`, `include/dc24h/database` | Adapter połączenia MariaDB i zapytań |
| `database/migrations/` | Niezmienna migracja rejestru wersji MariaDB |
| `webadmin/` | Frontend React/TypeScript/Vite i lokalny adapter |
| `doc/` | Dokumentacja techniczna |
| `.github/workflows/` | Automatyzacja budowania/testów |

Nowe moduły dodawaj do odrębnych katalogów. Stosuj C++20, RAII, jawne
właścicielstwo i sprawdzanie błędów. Nie używaj globalnych obiektów silnika.
Zachowuj niezależność Core od UI, SQL i parsowania protokołu.

Przed commitem wykonaj budowanie i CTest; aktualizuj `doc/` i `CHANGELOG.md`.
Wszystkie deklaracje ukończenia funkcji muszą odpowiadać kodowi i testom.

Nowe gałęzie twórz z aktualnego `main`. Gałąź alpha-0.1.4 zachowuje przez merge historię referencyjnej chatgpt-alpha-0.1.4. Kod i domyślne komunikaty: EN/US UTF-8; dokumentacja: `doc/us` i `doc/pl`, katalogi komunikatów: `languages/us` i `languages/pl`. Kolejność wpisów katalogu musi być zgodna. Nie tłumacz danych protokołu.

alpha-0.1.5 powstaje z main po jawnym scaleniu alpha-0.1.4. Kod konfiguracji/loggera i testy chatgpt-alpha-0.1.5 sprawdzono i dostosowano, zachowując poprawki TLS i języków z main. Nie importowano dodatkowego katalogu en/. Usunięto zbędne indeksy modułów w korzeniu doc/.

alpha-0.1.6 powstaje z main po alpha-0.1.5. Sprawdzono i dostosowano zmiany loggera chatgpt-alpha-0.1.6; zachowano bezpieczniejsze INI/pliki, planowanie TLS i US/PL z main. Używaj enum ConnectionEvent, bez ramek i danych klientów w logach.

alpha-0.1.7 powstaje z main po scaleniu alpha-0.1.6. Sprawdzono i dostosowano kod/testy chatgpt-alpha-0.1.7. Zachowuj ograniczoną pamięć, kanoniczne IP, jedno zwolnienie slotu i bezwzględne terminy ramek; nie loguj kluczy IP i danych. Security pozostaje niezależne od planowanego uwierzytelniania.

## Etap Database

alpha-0.1.8 powstaje z main po alpha-0.1.7. Nie przenoś SQL i haseł do Core/Network, wiąż wartości zapytań i nie edytuj zastosowanych migracji. Przed nieidempotentnym DDL dodaj obsługę częściowej awarii. Testy bazy muszą mieć własny katalog danych/socket i `--no-defaults`; nie korzystaj z bazy dewelopera.

## Etap API

alpha-0.1.9 powstaje z main po scaleniu alpha-0.1.8. Zachowuj wąski kontrakt HTTP bez body i lokalne zabezpieczenia. Zmiany stanu, parser body, udostępnienie Apache i token w przeglądarce wymagają projektu uprawnień/ról/audytu. Callbacki muszą być bezpieczne współbieżnie, ograniczone czasowo i żyć dłużej niż API. Testy mają używać tymczasowych prywatnych gniazd/tokenów oraz weryfikować liczniki ncdc i stan MariaDB przez rzeczywiste API.

## Etap WebAdmin

alpha-0.1.10 powstaje z main po alpha-0.1.9. Oddzielaj schemat/formatowanie przeglądarki od serwerowego adaptera Vite, zachowuj parzystość EN/PL, nie dodawaj sekretów/node_modules/dist/wyników testów do Git i aktualizuj package-lock przy zależnościach. Sprawdzaj build, testy jednostkowe/przeglądarkowe/integrację z C++ i istniejące testy serwera. Zmiany administracyjne i produkcyjne logowanie WWW wymagają osobnego projektu.

## Etap Apache2

alpha-0.1.11 powstaje z main po alpha-0.1.10. Oddzielaj aktywację hosta, rzeczywiste certyfikaty i reload od tworzenia/testowania przykładu. Testuj dokładnie dostarczaną politykę TLS/dostępu/nagłówków z tymczasowymi ścieżkami/portami, bez konfiguracji hosta. Sukces apache2 -t nie dowodzi zgodności certyfikatu/klucza; sprawdzaj inicjalizację. Zachowuj oba języki dokumentacji i spójne wersje fixture.

## Etap ADC Parser

alpha-0.2.2 powstaje z main po jawnym scaleniu alpha-0.2/alpha-0.2.1. Oddzielać składnię argumentów BASE od autoryzacji, tożsamości i wykonania. Zachować nieinterpretowane układy rozszerzeń, kolejność pól i powtórzenia. NamedParameterView wskazuje odkodowany token; nie przechowywać go po modyfikacji/zniszczeniu. Nowe reguły wymagają testów aktywnych w Release i porównywania pełnych wyników strumienia, następnie TCP, TLS i obu transportów ncdc.
