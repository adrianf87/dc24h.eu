# CI na własnym runnerze Ubuntu 24.04

Jedynym aktywnym workflow jest `.github/workflows/build.yml`. Każde zadanie wybiera istniejący runner `server-747585786` przez wszystkie pięć etykiet: `[self-hosted, Linux, X64, server-747585786, ubuntu-24.04]`. Nie ma celu ani zastępczego runnera hostowanego przez GitHub. Gdy własny runner jest offline lub zajęty, zadania czekają w kolejce.

Macierz serwera zachowuje **Debug i Release**, `fail-fast: false` oraz limit jednego zadania macierzy naraz. Instaluje narzędzia kompilacji i wymagane zależności, buduje C++20 i uruchamia wszystkie zestawy CTest z wymaganymi integracjami, w tym ncdc, MariaDB i Apache2. Zadanie WebAdmin używa tego samego runnera z Node 22, produkcyjnym build/typecheck, testami jednostkowymi, testami Chromium i integracją z rzeczywistym API C++. Jeden zarejestrowany runner wykonuje jedno zadanie naraz.

Konto runnera wymaga Git, dostępu sieciowego do repozytoriów pakietów, zapisywalnego katalogu roboczego i sudo bez hasła do instalacji zależności przez apt i Playwright. CMake, kompilator C++ i Python są instalowane jawnie, bez założenia, że dostarcza je obraz runnera GitHub. Powyższe etykiety muszą pozostać przypisane w ustawieniach GitHub.

Workflow uruchamiają zdarzenia push i pull request. Konfigurację sprawdzamy lokalnie; publikacja nie czeka na zdalny **Check status**. Wyniki wykonania są dostępne w GitHub Actions. Istniejące przebiegi używają swojej pierwotnej wersji workflow; zmiana kieruje nowe przebiegi na własny runner.

Dawne workflow VM Proxmox/Debian pozostają wycofane. Archiwalne workflow poza `.github/workflows/` nie są aktywnymi workflow GitHub Actions. Wspierane platformy aplikacji i historyczne wyniki testów pozostają bez zmian.
