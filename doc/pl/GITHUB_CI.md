# CI w GitHub Actions

Testy ncdc z konfliktem interfejsów rezerwują porty na obu adresach loopback i zachowują drugi nasłuch do gotowości klienta. Wyłącznie kolizja EADDRINUSE przy przydziale wybiera nową parę w ograniczonej pętli; sam test nie jest ponawiany. `integration.ncdc_ports` sprawdza deterministyczne kolizje TCP/UDP i sprzątanie.

Workflow `Check status` w `.github/workflows/build.yml` działa na własnym runnerze
`server-747585786` z etykietami `self-hosted`, `Linux`, `X64` i `server-747585786`.
Ten runner nie jest kopiowany z gałęzi `contabo-docker-v0.1`. Ta gałąź nie scala
`contabo-docker-v0.1` i sama nie zmienia `main`. Push oraz pull requesty z tego
samego repozytorium do `main` lub `alpha-0.7.8` / `alpha-0.7.9` / `alpha-0.7.10` / `alpha-0.7.11` / `alpha-0.7.12` / `alpha-0.7.13` / `alpha-0.7.14` / `alpha-0.7.15` / `alpha-0.8.1` / `alpha-0.8.2` / `alpha-0.8.3` / `alpha-0.8.4` / `alpha-0.8.5` / `alpha-0.8.6` / `alpha-0.8.7` / `alpha-0.8.8`, a także ręczne uruchomienie, włączają CI.
Pull requesty z forków nie wykonują się na tym trwałym hoście. Uprawnienia
repozytorium są tylko do odczytu; checkout nie zachowuje poświadczeń.

Jedno zadanie na tym hoście Debian 13 włącza każdy kontener testowy tylko na czas
jego sprawdzenia i wyłącza go przed startem następnego. Kontenery to
`ubuntu2204-github-runner`, `ubuntu2404-github-runner`, `ubuntu2604-github-runner`,
`debian12-github-runner` i `debian13-github-runner`. Każde sprawdzenie ustawia 16
wątków CPU i 32 GiB RAM, odmawia startu, gdy inny z tych kontenerów już działa, i
zostawia kontener zatrzymany. W kontenerze zadanie instaluje narzędzia, MariaDB,
Apache i ncdc, a potem uruchamia C++ Debug i Release z 16 workerami kompilacji i
CTest oraz WebAdmin: typy, testy Node, Chromium i integrację żywego API C++.
Sam host nie jest maszyną budującą.

Zadanie sprawdza, że `RUNNER_NAME` to `server-747585786`, użytkownikiem jest
`github-runner` i że Docker widzi wszystkie pięć zatrzymanych kontenerów. Nie
czekamy na zakończenie CI.

Grupa współbieżności zależy od workflow i ref Git; `cancel-in-progress: false`.
Limit czasu zadania kontenerów wynosi 360 minut. Faktyczne wyniki każdego
przebiegu są dostępne w
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml).

Ten wybór runnera zastępuje wcześniej opisane zadania GitHub-hosted Ubuntu 24.04.
Push uruchamia CI; nie czekamy na jego zakończenie.
