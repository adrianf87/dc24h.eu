# Wersjonowanie

Pierwsze wydanie: `alpha-0.1.1`, gałąź `alpha-0.1.1` utworzona z aktualnego
`main`. Historia i istniejąca licencja pozostają zachowane. Nie tworzono
automatycznie tagu ani GitHub Release.

W CMake `PROJECT_VERSION` zawiera `0.1.1`, a `DC24H_VERSION` dodaje `alpha-`.
Generowany nagłówek zapewnia tę samą wersję w logu startowym i `--version`.
Każdy build jest identyfikowany numerem wersji oraz SHA źródłowego commita Git.
Budowanie lokalne nie zmienia samoczynnie plików wersji ani historii zmian.

Nowe wydanie/iteracja wymaga świadomej aktualizacji numeru, wpisu
w `CHANGELOG.md`, dokumentacji oraz testów sprawdzających wersję.
Alpha oznacza etap rozwojowy, bez deklaracji stabilności produkcyjnej.
