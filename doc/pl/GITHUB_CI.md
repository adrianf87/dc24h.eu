# CI w GitHub Actions

Testy ncdc z konfliktem interfejsów rezerwują porty na obu adresach loopback i zachowują drugi nasłuch do gotowości klienta. Wyłącznie kolizja EADDRINUSE przy przydziale wybiera nową parę w ograniczonej pętli; sam test nie jest ponawiany. `integration.ncdc_ports` sprawdza deterministyczne kolizje TCP/UDP i sprzątanie.

Workflow `Check status` w `.github/workflows/build.yml` działa na własnym runnerze
`server-747585786` z etykietami `self-hosted`, `Linux`, `X64` i `server-747585786`.
Ten runner nie jest kopiowany z gałęzi `contabo-docker-v0.1`. Ta gałąź nie scala
`contabo-docker-v0.1` i sama nie zmienia `main`. Push oraz pull requesty z tego
samego repozytorium do `main` lub `alpha-0.7.8` / `alpha-0.7.9` / `alpha-0.7.10` / `alpha-0.7.11` / `alpha-0.7.12` / `alpha-0.7.13` / `alpha-0.7.14` / `alpha-0.7.15` / `alpha-0.8.1` / `alpha-0.8.2` / `alpha-0.8.3` / `alpha-0.8.4` / `alpha-0.8.5` / `alpha-0.8.6` / `alpha-0.8.7` / `alpha-0.8.8`, a także ręczne uruchomienie, włączają CI.
Pull requesty z forków nie wykonują się na tym trwałym hoście. Uprawnienia
repozytorium są tylko do odczytu; checkout nie zachowuje poświadczeń.

Trzy zadania korzystają z tego jednego runnera:

- **C++ Debug** i **C++ Release**: osobne zadania macierzy konfigurują C++20 z
  `BUILD_TESTING=ON` i `DC24H_REQUIRE_INTEGRATION_TESTS=ON`, kompilują projekt i
  uruchamiają wszystkie zestawy CTest. Runner musi już mieć kompilator, MariaDB,
  Apache i ncdc. Zadania nie instalują pakietów i nie używają sudo. Brak narzędzi
  kończy sprawdzenie runnera. Błąd jednego wariantu nie anuluje drugiego.
- **WebAdmin**: Node 22, `npm ci`, kontrola typów i kompilacja produkcyjna, testy
  jednostkowe Node, testy przeglądarkowe Chromium oraz integracja rzeczywistego
  API C++ z React. Zadanie kompiluje własny plik `dc24h` w Release z
  `BUILD_TESTING=OFF`; pełne testy C++ wykonuje powyższa macierz. Playwright
  instaluje Chromium dla użytkownika runnera i nie instaluje pakietów systemowych.

Każde zadanie sprawdza, że `RUNNER_NAME` to `server-747585786` i że systemem jest
Ubuntu 24.04. Kompilacja i testy korzystają z `nproc` tej maszyny, bez stałego
limitu liczby workerów i bez max-parallel w macierzy. Nie czekamy na zakończenie CI.
Testy uruchamiają własne, odizolowane usługi.

Grupa współbieżności zależy od workflow i ref Git; `cancel-in-progress: false`.
Limit czasu zadań C++ wynosi 120 minut, a WebAdmin 60 minut. Faktyczne wyniki
każdego przebiegu są dostępne w
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml).

Ten wybór runnera zastępuje wcześniej opisane zadania GitHub-hosted Ubuntu 24.04.
Push uruchamia CI; nie czekamy na jego zakończenie.
