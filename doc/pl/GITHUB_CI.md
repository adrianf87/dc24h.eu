# CI w GitHub Actions

Testy ncdc z konfliktem interfejsów rezerwują porty na obu adresach loopback i zachowują drugi nasłuch do gotowości klienta. Wyłącznie kolizja EADDRINUSE przy przydziale wybiera nową parę w ograniczonej pętli; sam test nie jest ponawiany. `integration.ncdc_ports` sprawdza deterministyczne kolizje TCP/UDP i sprzątanie.

Workflow `Check status` w `.github/workflows/build.yml` działa na własnym runnerze
`server-747585786` z etykietami `self-hosted`, `Linux`, `X64` i `server-747585786`.
Push do `main`, `alpha-0.8.7` i `alpha-0.8.8` oraz ręczne uruchomienie włączają CI.
Uprawnienia repozytorium są tylko do odczytu; checkout nie zachowuje poświadczeń.

Jedno zadanie na tym hoście Debian 13 wyłącza przestarzałe usługi runnerów
w kontenerach, a potem sprawdza Ubuntu 22.04, Ubuntu 24.04, Ubuntu 26.04,
Debian 12 i Debian 13 po jednym kontenerze. Każde sprawdzenie ustawia 16 wątków
CPU i 32 GiB RAM, zatrzymuje bezczynny kontener przed testem i zatrzymuje go
ponownie po teście. Kontener, w którym już działa worker GitHub runnera,
przerywa zadanie zamiast zostać przerwany. Niepoprawny tekst na porcie TLS może
skończyć się EOF albo jednym rekordem fatal alert, zależnie od OpenSSL w
kontenerze. Zamknięcie klienta akceptuje nagłe EOF albo zakończone unwrap();
serwer nadal nie wywołuje SSL_shutdown(). W kontenerze zadanie instaluje
narzędzia, MariaDB, Apache i ncdc, a potem uruchamia C++ Debug i Release z 16
workerami kompilacji i CTest oraz WebAdmin: typy, testy Node, Chromium i
integrację żywego API C++. Sam host nie jest maszyną budującą. Po pięciu
sprawdzeniach każdy kontener jest zatrzymany.

Jedna grupa współbieżności, `dc24h-eu-docker-host-sequential`, ma
`cancel-in-progress: false`. Limit czasu zadania wynosi 360 minut. Faktyczne
wyniki każdego przebiegu są dostępne w
[GitHub Actions](https://github.com/adrianf87/dc24h.eu/actions/workflows/build.yml).
Nie czekamy na zakończenie CI.

Ten wybór runnera zastępuje wcześniej opisane zadania GitHub-hosted Ubuntu 24.04.
Push uruchamia CI; nie czekamy na jego zakończenie.
