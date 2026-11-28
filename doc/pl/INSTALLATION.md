# Instalacja na Debian / Ubuntu

Zbuduj beta-0.9.20 do lokalnych testów i uruchom hub na adresie loopback. Przy aktualizacji z Twilight lub zastanej instalacji systemd skorzystaj z [instrukcji migracji i wycofania](REBRAND_MIGRATION.md).

**alpha-0.8.5 Ban Core:** [Trwałe blokady wejścia kont/CID/IP](BAN_CORE.md) dodają jawny schemat v8, dodawanie/cofanie z hasłem, egzekwowanie ze startu i wygaśnięcie. Admin Core ma dziesięć poleceń; users.ban/blocks.manage działają. Zastępuje wcześniejsze opisy v7/planowanych banów.

**alpha-0.8.2 Operators:** [Spis zespołu i lokalne tworzenie/przekazanie własności](OPERATORS.md) zastępują wcześniejsze ograniczenia bootstrap/przekazania Ownera. Schemat v7 i zwykła ochrona Ownera pozostają.

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

**alpha-0.7.15 Configuration:** [startowe ustawienia bazy i logowania](DATABASE_AUTH_CONFIGURATION.md) dodają wspólne dziedziczenie timeoutów, zakresy także przy wyłączonym logowaniu, bezpieczny podgląd --show-configuration i stronę Konfiguracja EN/PL tylko do odczytu. Migawki nie zawierają poświadczeń ani prywatnych ścieżek; zmiany wymagają sprawdzenia i restartu. CI sprawdza cztery logiczne CPU na runner i używa nproc bez stałego limitu workerów. Nie czekamy na zdalne CI.

**alpha-0.7.13 Database Resilience:** [jawne odnowienie połączenia i niezależne limity czasu](DATABASE_RESILIENCE.md) dodają pojedynczą próbę reconnect w wątku właściciela, ponowną walidację pliku hasła oraz ochronę przed powtarzaniem SQL i zastąpieniem aktywnej transakcji. Schemat v6 i migawki uwierzytelniania pozostają. CI korzysta z runnerów GitHub-hosted Ubuntu 24.04; nie czekamy na jego zakończenie. Ten wpis zastępuje wcześniejsze opisy braku reconnect.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Systemy docelowe: Debian 12/13 oraz Ubuntu 22.04/24.04/26.04 LTS. Zakres rzeczywistej weryfikacji podaje [TESTING.md](TESTING.md).

```sh
sudo apt-get update
sudo apt-get install build-essential cmake git python3 libutf8proc-dev libgcrypt20-dev libssl-dev openssl ncdc libmariadb-dev
git clone --branch main https://github.com/adrianf87/dc24h.eu.git
cd dc24h.eu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/dc24h --language pl --help
./build/dc24h --language pl --listen 127.0.0.1:1511
```

Uruchamiaj jako zwykły użytkownik. Program działa na pierwszym planie i kończy po Ctrl+C/SIGTERM. Nie tworzy PID-file, nie zmienia firewalla, nie instaluje i nie uruchamia usługi. [Bezpieczeństwo systemu](SYSTEM_SECURITY.md) dodaje przykład Type=simple z pełną ścieżką ExecStart, zablokowanym użytkownikiem i stderr w journalu. Budowanie nie włącza tej jednostki.

ADCS wymaga certyfikatu i klucza z prawami odczytu dla konta usługi. Hub może korzystać z osobno skonfigurowanego [odnawiania TLS](TLS_AUTO_RENEWAL.md). W przykładach lokalnych Apache2 jest opcjonalny. Publiczna brama etapu IV działa oddzielnie z Apache2, Node.js i MariaDB. Zatwierdzona instalacja Ubuntu używa portu 7178, a historyczny Debian używa portu 7278. Przy lokalnych testach połączenie huba z MariaDB jest opcjonalne. [ADCS TLS](ADCS_TLS.md) · [Obsługa etapu IV](STAGE_IV_DEDICATED_SERVICE.md) · [Baza danych](DATABASE.md) · [Konfiguracja](CONFIGURATION.md)

Aby użyć pliku, skopiuj [przykład](../../config/dc24h.ini.example), dostosuj ścieżki i wykonaj `./build/dc24h --config config/dc24h.ini --check-config`, a następnie uruchom z tym samym `--config`. Lokalne INI są ignorowane przez Git.

Instalowany szablon HTTPS, ścieżki certyfikatów, dostęp lokalny i jawna aktywacja Apache: [APACHE2_HTTPS.md](APACHE2_HTTPS.md). Przykład odwrotnego proxy jest instalowany obok niego: [APACHE2_PROXY.md](APACHE2_PROXY.md). Oba zawsze trafiają do `etc/dc24h`; CMake nie aktywuje certyfikatu, witryny ani usługi.
