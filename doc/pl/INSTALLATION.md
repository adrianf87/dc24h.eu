# Instalacja na Debian / Ubuntu

**Aktualny etap — alpha-0.3.1:** [Session Core](SESSION_CORE.md) dostarcza niezależny model Users i trwałe w obrębie reaktora ID sesji, podłączone do rzeczywistej gotowości, zamykania i czasów. Wcześniejsze opisy zachowują zakres pierwotnego etapu.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Systemy docelowe: Debian 12/13 oraz Ubuntu 22.04/24.04/26.04 LTS. Zakres rzeczywistej weryfikacji podaje [TESTING.md](TESTING.md).

```sh
sudo apt-get update
sudo apt-get install build-essential cmake git python3 libutf8proc-dev libgcrypt20-dev libssl-dev openssl ncdc libmariadb-dev
git clone --branch alpha-0.1.11 https://github.com/adrianf87/dc24h.eu.git
cd dc24h.eu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/dc24h --language pl --help
./build/dc24h --language pl --listen 127.0.0.1:1511
```

Uruchamiaj jako zwykły użytkownik. Program działa na pierwszym planie i kończy po Ctrl+C/SIGTERM. Nie tworzy PID-file, nie zmienia firewalla, nie instaluje i nie uruchamia usługi. Pod systemd można użyć Type=simple, pełnej ścieżki ExecStart, dedykowanego użytkownika i stderr w journalu.

ADCS wymaga własnego certyfikatu i klucza z prawami odczytu dla usługi; zobacz [ADCS_TLS.md](ADCS_TLS.md). Program nie pozyskuje ani nie odnawia certyfikatów. Apache2 jest opcjonalny dla statycznego WebAdmin, produkcyjna brama uwierzytelniona jest planowana; połączenie MariaDB jest opcjonalne i domyślnie wyłączone. Zobacz [DATABASE.md](DATABASE.md). Opcje portów i języka: [CONFIGURATION.md](CONFIGURATION.md).

Aby użyć pliku, skopiuj [przykład](../../config/dc24h.ini.example), dostosuj ścieżki i wykonaj `./build/dc24h --config config/dc24h.ini --check-config`, a następnie uruchom z tym samym `--config`. Lokalne INI są ignorowane przez Git.

Instalowany szablon HTTPS, ścieżki certyfikatów, dostęp lokalny i jawna aktywacja Apache: [APACHE2_HTTPS.md](APACHE2_HTTPS.md). Przykład zawsze trafia do `etc/dc24h`; CMake nie aktywuje certyfikatu/witryny/usługi.
