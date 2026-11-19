# CI w GitHub Actions

Testy ncdc z konfliktem interfejsów rezerwują porty na obu adresach loopback i zachowują drugi nasłuch do gotowości klienta. Wyłącznie kolizja EADDRINUSE przy przydziale wybiera nową parę w ograniczonej pętli; sam test nie jest ponawiany. `integration.ncdc_ports` sprawdza deterministyczne kolizje TCP/UDP i sprzątanie.

Workflow `Check status` w `.github/workflows/build.yml` działa na własnym runnerze
`server-747585786` z etykietami `self-hosted`, `Linux`, `X64` i `server-747585786`.
Push do `main`, `alpha-0.8.7` i `alpha-0.8.8` oraz ręczne uruchomienie włączają CI.
Uprawnienia repozytorium są tylko do odczytu; checkout nie zachowuje poświadczeń.

Host runnera to Debian 13. Zadanie potwierdza ten system, instaluje na hoście
narzędzia C++, MariaDB, Apache i ncdc, a potem uruchamia tam C++ Debug i Release
z co najwyżej 16 workerami kompilacji i CTest. WebAdmin na tym samym hoście
sprawdza typy, buduje wersję produkcyjną, uruchamia testy Node, Chromium i
integrację żywego API C++ na Node.js 22. Niepoprawny tekst na porcie TLS może
skończyć się EOF albo jednym rekordem fatal alert. Zamknięcie klienta akceptuje
nagłe EOF albo zakończone unwrap(); serwer nadal nie wywołuje SSL_shutdown().

Jedna grupa współbieżności, `dc24h-eu-docker-host-sequential`, ma
`cancel-in-progress: false`. Limit czasu zadania wynosi 180 minut. Faktyczne
wyniki każdego przebiegu są dostępne w
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml).
Nie czekamy na zakończenie CI.

Ten wybór runnera zastępuje wcześniej opisane zadania GitHub-hosted Ubuntu 24.04.
Push uruchamia CI; nie czekamy na jego zakończenie.
