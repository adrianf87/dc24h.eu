# CI na własnym runnerze Ubuntu 24.04

Jedynym aktywnym workflow jest `.github/workflows/build.yml`. Każde zadanie wybiera istniejący runner `server-747585786` przez wszystkie pięć etykiet: `[self-hosted, Linux, X64, server-747585786, ubuntu-24.04]`. Nie ma celu ani zastępczego runnera hostowanego przez GitHub. Gdy własny runner jest offline lub zajęty, zadania czekają w kolejce.

Macierz serwera zachowuje **Debug i Release**, `fail-fast: false` oraz limit jednego zadania macierzy naraz. Sprawdza uprzednio zainstalowane narzędzia kompilacji i wymagane zależności, buduje C++20 i uruchamia wszystkie zestawy CTest z wymaganymi integracjami, w tym ncdc, MariaDB i Apache2. Zadanie WebAdmin używa tego samego runnera z Node 22, produkcyjnym build/typecheck, testami jednostkowymi, testami Chromium i integracją z rzeczywistym API C++. Jeden zarejestrowany runner wykonuje jedno zadanie naraz.

Konto runnera wymaga Git, Pythona 3, dostępu sieciowego do repozytoriów pakietów i zapisywalnego katalogu roboczego. CI nie wywołuje sudo i nie pyta o hasło. Powyższe etykiety muszą pozostać przypisane w ustawieniach GitHub.

## Jednorazowe przygotowanie przez administratora

Pierwszy przebieg na własnym runnerze zatrzymał się przed kompilacją, ponieważ konto runnera nie mogło użyć sudo bez hasła. W sprawdzonej kopii repozytorium na **server-747585786 (Ubuntu 24.04)** administrator jednorazowo instaluje pakiety serwera i systemowe zależności Chromium:

```sh
sudo python3 scripts/ci/runner_dependencies.py install all
```

To jawne przygotowanie używa apt i może uruchomić pakietowe usługi MariaDB/Apache; wykonaj je na dedykowanej maszynie CI. Nie zmienia sudoers, uprawnień runnera, haseł ani sekretów repozytorium. Skrypt odmawia instalacji na innym systemie lub bez roota. Listę pakietów można przejrzeć bez uprawnień: `python3 scripts/ci/runner_dependencies.py list all`.

Zwykłe zadania wykonują wyłącznie `check server` / `check webadmin`. Brak pakietów kończy się czytelnym błędem; testy nie są pomijane. Playwright pobiera własną wersję Chromium do pamięci podręcznej konta runnera przez `npx playwright install chromium`, bez `--with-deps` i sudo. Po zmianie zależności administrator powtarza przygotowanie. Lista pakietów odpowiada zależnościom Chromium z katalogu Ubuntu 24.04 x64 w przypiętej wersji Playwright.

Workflow uruchamiają zdarzenia push i pull request. Konfigurację sprawdzamy lokalnie; publikacja nie czeka na zdalny **Check status**. Wyniki wykonania są dostępne w GitHub Actions. Istniejące przebiegi używają swojej pierwotnej wersji workflow; zmiana kieruje nowe przebiegi na własny runner.

Dawne workflow VM Proxmox/Debian pozostają wycofane. Archiwalne workflow poza `.github/workflows/` nie są aktywnymi workflow GitHub Actions. Wspierane platformy aplikacji i historyczne wyniki testów pozostają bez zmian.
