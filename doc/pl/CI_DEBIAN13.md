# Konfiguracja CI

`Check status` używa własnego runnera `server-747585786`.
Ten host włącza każdy kontener testowy tylko na czas sprawdzenia, z 16 wątkami
CPU i 32 GiB RAM, a potem go wyłącza. Opis jest w [CI w GitHub Actions](GITHUB_CI.md).
