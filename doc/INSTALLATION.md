# Instalacja na Debian i Ubuntu

Systemy docelowe projektu: Debian 12/13 oraz Ubuntu 22.04, 24.04 i 26.04 LTS.
Pełna macierz zgodności nie jest jeszcze zweryfikowana; aktualne dowody testów
opisuje [TESTING.md](TESTING.md).

```sh
sudo apt-get update
sudo apt-get install build-essential cmake git python3
git clone --branch alpha-0.1.2 https://github.com/adrianf87/dc24h.eu.git
cd dc24h.eu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/dc24h
```

Uruchamiaj jako zwykły użytkownik. Zatrzymanie przez Ctrl+C lub SIGTERM.
Program pozostaje na pierwszym planie; nie tworzy PID-file i nie daemonizuje
się. Do uruchamiania pod systemd można użyć zwykłej usługi `Type=simple`
z pełną ścieżką `ExecStart` do programu i dedykowanym nieuprzywilejowanym
użytkownikiem. `systemctl stop` przekazuje SIGTERM; logi stderr odbierze journal.
Ten etap nie instaluje i nie uruchamia automatycznie żadnej usługi.

Network domyślnie zajmuje `127.0.0.1:1511`. Opcje portów i limitów:
[CONFIGURATION.md](CONFIGURATION.md). Certyfikaty, Apache2 i MariaDB nie są
jeszcze wykorzystywane. Instalacja nie zmienia firewalla.
