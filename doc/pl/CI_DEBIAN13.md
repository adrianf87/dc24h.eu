# CI na runnerach GitHub Ubuntu; runner Debiana wycofany

Na tej gałęzi jedynym aktywnym workflow jest `.github/workflows/build.yml`. Kompilacja C++20 i wszystkie integracje CTest działają na runnerach GitHub **Ubuntu 22.04 i Ubuntu 24.04**, w trybach **Debug i Release** (`fail-fast: false`). Wymagane narzędzia integracyjne obejmują ncdc, MariaDB i Apache2. Zadanie WebAdmin pozostaje na runnerze GitHub **Ubuntu 24.04** z Node 22, build/typecheck produkcyjnym, testami jednostkowymi, Chromium i integracją z rzeczywistym API C++.

Usunięto workflow startu/testów/wyłączenia VM Proxmox/Debian 13 oraz sprawdzania połączenia, jeżeli występowały. Workflow Contabo/Debian, jeżeli występowały, są archiwum w `.contabo/workflows/` dla lokalnych testów narzędzi i nie są workflow GitHub Actions. Aktywne CI nie używa kontenera Debiana, własnego runnera, sterowania VM przez SSH ani wykrywania/reinstalacji zewnętrznego hosta. Usunięto zbędną konfigurację etykiety własnego runnera.

Zmiana dotyczy konfiguracji CI. Debian pozostaje wspieraną platformą aplikacji; historyczne wyniki lokalnych testów Debiana zachowują znaczenie. Kod aplikacji, workflow Ubuntu i jego kompletne polecenia testów pozostają bez zmian. Commity nie modyfikują rejestracji runnerów, sekretów repozytorium, zewnętrznych maszyn ani historycznych przebiegów Actions.

Zdarzenia push i pull request uruchamiają `build.yml`. Lokalnie sprawdzono YAML, macierz Ubuntu/trybów kompilacji i cele zadań. Wyniki zdalne należy sprawdzić w GitHub Actions; usunięcie workflow nie usuwa starych nieudanych statusów.
