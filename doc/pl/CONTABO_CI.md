# Konfiguracja CI

`Check status` w tej gałęzi używa własnego runnera `server-747585786`.
Ten host włącza każdy kontener testowy tylko na czas sprawdzenia, z 16 wątkami
CPU i 32 GiB RAM, a potem go wyłącza. Opis jest w [CI w GitHub Actions](GITHUB_CI.md).
Gałąź `contabo-docker-v0.1` nie jest scalana z tą gałęzią ani z `main`.
