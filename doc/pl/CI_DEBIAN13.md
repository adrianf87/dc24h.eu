# CI na własnych runnerach Proxmox

Workflow alpha-0.7.4 `.github/workflows/build.yml` wywołuje [workflow VM](https://github.com/adrianf87/dc24h.eu/blob/20ca46bd5f297a98bbbbe59604bcb0a041839b6d/.github/workflows/proxmox-vm.yml) z **github-runner-v0.1**, przypięty do `20ca46bd5f297a98bbbbe59604bcb0a041839b6d`. Gałąź rozwojowa powstała z main; nie dodano merge, cherry-pick ani rodzica z gałęzi runnerów. Gałąź źródłowa pozostaje bez zmian.

| Etykieta runnera | Odpowiedzialność |
| --- | --- |
| `proxmox-worker-1` | Przygotowanie i sprzątanie klonów przez zainstalowany kontroler hosta |
| `proxmox-worker-2` | VM 200 z zatrzymanej VM 100; Ubuntu 24.04 |
| `proxmox-worker-3` | VM 201 z zatrzymanej VM 101; Ubuntu 22.04 i G++ 12 |

Wszystkie zadania używają własnych runnerów Linux X64. Pełny cykl VM 200, wraz z usunięciem, kończy się przed utworzeniem VM 201. Wspólna grupa concurrency `dc24h-proxmox-vms-200-201` szereguje tę gałąź z gałęzią infrastruktury; `cancel-in-progress: false` pozwala zakończyć sprzątanie. Aktywny przebieg źródłowej gałęzi może opóźnić kolejkę. Pakiety źródłowych VM pozostają bez zmian; VM 300/301 są poza workflow.

Provision pobiera wyłącznie dokładną kopię skryptu `proxmox-worker/scripts/guest/runner_dependencies.py`, porównuje ją z zainstalowaną wersją roota i instaluje zależności w pasującym oznaczonym klonie przez ograniczone `sudo -n /usr/local/sbin/dc24h-ci-dependencies install all`. To jedyna uprzywilejowana operacja przygotowania; buildy/testy aplikacji działają bez sudo. Skrypt odmawia instalowania pakietów w źródłach i przy niewłaściwym systemie/serialu DMI. Nie dodano haseł, tokenów rejestracji, ogólnych praw sudo ani instalatorów infrastruktury.

Każde Ubuntu wykonuje C++20 Debug i Release z wymaganymi integracjami MariaDB/Apache/ncdc CTest, następnie build/typecheck WebAdmin, testy Node, Chromium i rzeczywiste API C++/React. Równoległość build/CTest/Node/Playwright wynosi cztery. Checkouty aplikacji pomijają `proxmox-worker`, podobnie archiwa źródeł przez `.gitattributes`. Zwykły clone wymaga jawnego sparse checkout, aby pominąć śledzone pliki.

Wymagane są zainstalowane kontroler hosta i bootstrap gości z gałęzi runnerów. Przypięta [instrukcja infrastruktury](https://github.com/adrianf87/dc24h.eu/blob/20ca46bd5f297a98bbbbe59604bcb0a041839b6d/proxmox-worker/README.md) opisuje instalację, własność klonów, dzierżawy, reaper po sześciu godzinach, naprawę i ponawianie. Nie uruchamiaj źródła równolegle z klonem, nie omijaj kontroli własności i nie ponawiaj samych końcowych zadań po sprzątaniu: używaj **Re-run all jobs**. Sprzątanie zwraca błąd przy nieudanym przygotowaniu, testach lub usuwaniu. PR z forków są wykluczone przed działaniami infrastruktury.

Caller obsługuje push i PR dla `alpha-0.7.4` i `main` oraz ręczne uruchomienie. Filtr main zadziała dopiero po osobno autoryzowanym scaleniu; ten etap nie scala main. Sprawdzenie lokalne:

```sh
actionlint -config-file .github/actionlint.yaml .github/workflows/build.yml
python3 -m py_compile proxmox-worker/scripts/guest/runner_dependencies.py
```

Wyniki lokalne opisuje [Testowanie](TESTING.md), a rzeczywisty wynik runnerów jest w GitHub Actions. Stary runner `server-747585786` nie jest już celem zadań; jego usunięta rejestracja nie jest odtwarzana.
