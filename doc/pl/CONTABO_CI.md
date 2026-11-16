# Contabo CI

Workflow [Check status](../../.github/workflows/build.yml) tego brancha używa
wyłącznie istniejącego runnera Ubuntu 24.04 `server-747585786` (ID 27).
Wymagane etykiety to `self-hosted`, `Linux`, `X64`, `server-747585786`
i `ubuntu-24-04`. Unikalna etykieta serwera musi należeć tylko do tego runnera.
Nie ma fallbacku do GitHub-hosted. Gdy Contabo jest offline, zadania czekają.

Push i PR z tego samego repozytorium do tego brancha uruchamiają workflow;
dostępne jest też ręczne uruchomienie. Checkout testuje commit zdarzenia.
Przed testami sprawdzane są host, nazwa i środowisko runnera, x86_64, wersja
Ubuntu oraz konto `github-runner` bez roota. PR z forków są pomijane.
Workflow wymaga tylko odczytu repozytorium i nie zachowuje danych logowania
checkout.

Jedno zadanie wykonuje kompilację i CTest w Debug oraz Release, następnie
typecheck, build produkcyjny, testy Node, Chromium i integrację WebAdmin
z prawdziwym API C++. Równoległość kompilacji/CTest, testów Node i liczba
workerów Playwright wynoszą **6**. Wymagane są narzędzia testów integracyjnych,
w tym ncdc ADC/ADCS, prywatna MariaDB i Apache. Ten workflow nie potwierdza
testów innych systemów niż Ubuntu 24.04.

Grupa `dc24h-contabo-server-747585786-${{ github.ref }}` jest osobna dla
każdego brancha, dzięki czemu oczekujące przebiegi różnych branchy nie anulują
się wzajemnie. Jeden runner Contabo szereguje wykonanie zadań. W obrębie brancha
GitHub domyślnie zachowuje tylko jeden oczekujący przebieg.
Administrator przygotowuje pakiety przez `contabo/scripts/dependencies.py`;
CI tylko je sprawdza i nie używa sudo. Workflow instaluje Node 22 i Chromium
w katalogu użytkownika. Zobacz [przygotowanie workera](../../contabo/README.md).
Hasła, tokeny rejestracyjne i `.credentials*` nie należą do repozytorium.

Dotychczasowy workflow Proxmox został zastąpiony na tym branchu. Pozostałe
branche są niezależne i zmieniają się tylko zgodnie z poleceniem użytkownika.
GitHub zachowuje historię starych przebiegów; usunięcie pliku workflow nie
usuwa wyników ani nie anuluje wcześniej zakolejkowanych zadań.

CI startuje po opublikowaniu commitów. Zgodnie z poleceniem nie oczekujemy
w tym zadaniu na koniec przebiegów; kolejka i trwający test nie oznaczają sukcesu.

Checkout jest przypięty do v5 `fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09`, korzystającego z bezpośredniego tymczasowego nagłówka HTTP, po błędach fetch w v6 na trwałym runnerze. `persist-credentials: false` usuwa uwierzytelnienie po checkout. Nie dodano PAT ani szerszych uprawnień. Nowy fetch wymaga weryfikacji w CI; zmiana konfiguracji nie oznacza ukończonych testów.
