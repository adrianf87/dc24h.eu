# Wersjonowanie

Pierwsze wydanie: `alpha-0.1.1`, gałąź `alpha-0.1.1` utworzona z aktualnego
`main`. Historia i istniejąca licencja pozostają zachowane. Nie tworzono
automatycznie tagu ani GitHub Release.

Drugie wydanie: `alpha-0.1.2`, gałąź `alpha-0.1.2` utworzona z `main`
po scaleniu Core (`fa00f31`). Gałąź i historia alpha-0.1.1 pozostają zachowane.

Trzecie wydanie: `alpha-0.1.3`, gałąź `alpha-0.1.3` utworzona z aktualnego
`main` (`9199ae6`) po scaleniu Network. Istniejące gałęzie i historia zachowane.

W CMake `PROJECT_VERSION` zawiera `0.1.3`, a `DC24H_VERSION` dodaje `alpha-`.
Generowany nagłówek zapewnia tę samą wersję w logu startowym i `--version`.
Każdy build jest identyfikowany numerem wersji oraz SHA źródłowego commita Git.
Budowanie lokalne nie zmienia samoczynnie plików wersji ani historii zmian.

Nowe wydanie/iteracja wymaga świadomej aktualizacji numeru, wpisu
w `CHANGELOG.md`, dokumentacji oraz testów sprawdzających wersję.
Alpha oznacza etap rozwojowy, bez deklaracji stabilności produkcyjnej.
