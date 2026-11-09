# Rozwój projektu

Pracuj na gałęziach wydań, zachowując historię `main`. Aktualny etap:
`alpha-0.1.6`. Każdy istotny etap otrzymuje opisowy commit. Nie dodawaj
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
| `database/migrations/` | Miejsce na przyszłe migracje MariaDB |
| `webadmin/` | Miejsce na przyszły frontend |
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
