# Rozwój projektu

Pracuj na gałęziach wydań, zachowując historię `main`. Aktualny etap:
`alpha-0.1.1`. Każdy istotny etap otrzymuje opisowy commit. Nie dodawaj
artefaktów kompilacji, sekretów ani zależności vendored bez uzasadnienia.

| Katalog | Rola |
| --- | --- |
| `src/core`, `include/dc24h/core` | Core i adapter sygnałów |
| `src/logging`, `include/dc24h/logging` | Logowanie |
| `cmake/` | Generowana informacja o wersji |
| `tests/` | Testy C++ i testy procesu w Pythonie |
| `config/` | Miejsce na przyszłą konfigurację |
| `database/migrations/` | Miejsce na przyszłe migracje MariaDB |
| `webadmin/` | Miejsce na przyszły frontend |
| `doc/` | Dokumentacja techniczna |
| `.github/workflows/` | Automatyzacja budowania/testów |

Nowe moduły dodawaj do odrębnych katalogów. Stosuj C++20, RAII, jawne
właścicielstwo i sprawdzanie błędów. Nie używaj globalnych obiektów silnika.
Zachowuj niezależność Core od UI, SQL i parsowania protokołu.

Przed commitem wykonaj budowanie i CTest; aktualizuj `doc/` i `CHANGELOG.md`.
Wszystkie deklaracje ukończenia funkcji muszą odpowiadać kodowi i testom.
