# CI w GitHub Actions

Workflow `Check status` w `.github/workflows/build.yml` korzysta z jednorazowych
runnerów `ubuntu-24.04` utrzymywanych przez GitHub. Uruchamia się dla push i pull
requestów do `main` lub `alpha-0.7.8` / `alpha-0.7.9` / `alpha-0.7.10` / `alpha-0.7.11` / `alpha-0.7.12`, także z forków, oraz ręcznie.
Uprawnienia repozytorium są tylko do odczytu; checkout nie zachowuje poświadczeń.

Trzy zadania mogą działać niezależnie:

- **C++ Debug** i **C++ Release**: osobne zadania macierzy instalują zależności
  kompilacji oraz MariaDB, Apache i ncdc, konfigurują C++20 z `BUILD_TESTING=ON`
  i `DC24H_REQUIRE_INTEGRATION_TESTS=ON`, kompilują projekt i uruchamiają wszystkie
  zestawy CTest. Brak narzędzi integracyjnych przerywa konfigurację. Błąd jednego
  wariantu nie anuluje drugiego.
- **WebAdmin**: Node 22, `npm ci`, kontrola typów i kompilacja produkcyjna, testy
  jednostkowe Node, testy przeglądarkowe Chromium oraz integracja rzeczywistego
  API C++ z React. Zadanie kompiluje własny plik `dc24h` w Release z
  `BUILD_TESTING=OFF`; pełne testy C++ wykonuje powyższa macierz. Playwright
  instaluje Chromium i jego zależności systemowe.

Kompilacja C++, CTest, testy Node i Playwright wykorzystują pełną dostępną
liczbę CPU runnera (`nproc`) jako liczbę równoległych workerów. Testy uruchamiają własne, odizolowane usługi. Pakiety są
instalowane przez sudo na jednorazowych maszynach GitHub; kompilacja i testy
pracują jako użytkownik runnera. Rejestracja własnego runnera, host Contabo
ani maszyny Proxmox nie są wymagane.

Grupa współbieżności zależy od workflow i ref Git; `cancel-in-progress: false`.
Limit czasu zadań C++ wynosi 120 minut, a WebAdmin 60 minut. Faktyczne wyniki
każdego przebiegu są dostępne w
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml).
