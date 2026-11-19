# CI w GitHub Actions

Testy ncdc z konfliktem interfejsów rezerwują porty na obu adresach loopback i zachowują drugi nasłuch do gotowości klienta. Wyłącznie kolizja EADDRINUSE przy przydziale wybiera nową parę w ograniczonej pętli; sam test nie jest ponawiany. `integration.ncdc_ports` sprawdza deterministyczne kolizje TCP/UDP i sprzątanie.

Workflow `Check status` w `.github/workflows/build.yml` działa na runnerach GitHub-hosted `ubuntu-24.04`. Push do `main`, `alpha-0.8.7`, `alpha-0.8.8`, `alpha-0.8.9`, `alpha-0.8.10`, `alpha-0.8.11`, `alpha-0.8.12` i `alpha-0.8.13` oraz ręczne uruchomienie włączają CI. Pull requesty z forków nie działają. Uprawnienia repozytorium są tylko do odczytu; checkout nie zachowuje poświadczeń.

Działają dwa zadania. Każde sprawdza, że runner ma cztery logiczne CPU, i potem używa workerów `nproc`. Zadanie C++ buduje i testuje Debug oraz Release, łącznie z testami integracyjnymi, po instalacji narzędzi, MariaDB, Apache i ncdc. Zadanie WebAdmin buduje binarny hub Release, a potem sprawdza typy, buduje wersję produkcyjną, uruchamia testy Node, Chromium i integrację żywego API C++ na Node.js 22. Niepoprawny tekst na porcie TLS może skończyć się EOF albo jednym rekordem fatal alert. Zamknięcie klienta akceptuje nagłe EOF albo zakończone unwrap(); serwer nadal nie wywołuje SSL_shutdown().

Jedna grupa współbieżności na workflow i ref, `dc24h-github-` plus nazwa workflow i ref, ma `cancel-in-progress: false`. Zadanie C++ ma limit 120 minut, a WebAdmin 60 minut. Faktyczne wyniki każdego przebiegu są dostępne w [GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml). Nie czekamy na zakończenie CI.

Ten wybór zastępuje własny runner `server-747585786` dla nowych pushy tych gałęzi. Push uruchamia CI; nie czekamy na jego zakończenie.
