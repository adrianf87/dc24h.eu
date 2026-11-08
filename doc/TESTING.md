# Testowanie

```sh
ctest --test-dir build --output-on-failure
```

`core.lifecycle` jest programem C++ sprawdzającym stan początkowy, działanie
run/stop, stop przed startem, idempotencję, odrzucanie ponownego i jednoczesnego
run oraz 20 powtórzeń konkurencyjnych żądań stop z ośmiu wątków. Sprawdza też
anulowanie oczekiwania na sygnał i odbiór zakolejkowanego SIGTERM.
Warunki testów działają również w Release (nie opierają się na `assert`).

`core.process` uruchamia 7 przypadków unittest w Pythonie: wersja, pomoc,
błędne argumenty, SIGINT, SIGTERM, sygnał w trakcie startu oraz seria sygnałów.
Dwa ostatnie scenariusze wykonują po 10 uruchomień procesu. Sprawdza kod
wyjścia 0, log odebranego sygnału i dokładnie jedną sekwencję zatrzymania.
Synchronizacja następuje po logu startu/gotowości, a oczekiwanie ma timeout.
Każdy pozostawiony proces testowy jest sprzątany; CTest ogranicza czas suite.

Lokalna weryfikacja alpha-0.1.1: Debian 13, GCC 14.2, CMake 3.31,
konfiguracje Debug i Release. Definicja GitHub Actions obejmuje Ubuntu 22.04
i 24.04 w obu konfiguracjach; obecność workflow nie oznacza, że zdalne CI
zostało już wykonane lub zakończyło się sukcesem. Debian 12 i Ubuntu 26.04
wymagają osobnej weryfikacji.

Nie są jeszcze testowane moduły TCP/ADC/TLS, baza, API ani WebAdmin.
