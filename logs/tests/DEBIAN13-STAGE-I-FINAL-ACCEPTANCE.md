# Kwalifikacja Etapu I na Debianie 13

**STAGE I: BLOCKED**

Kwalifikacja lokalna Debiana 13 zakończyła się wynikiem PASS. Końcowy odbiór pozostaje otwarty do uzyskania wyników GitHub Actions dla zakwalifikowanego SHA.

Testy przeprowadzono 26 listopada 2026 r. na VPS-ie z Debianem 13.8. Gałąź `public/stage-i-debian13-ncdc-adcs` i PR #12 początkowo wskazywały `8af360efd03803e29e8fc2a733c7184ca31d576c`. Końcowa kwalifikacja objęła `068d77f35175b8f28dd326f5bc520a69c7d90661`. SHA-256 końcowej binarki `dc24h-twilight` to `f31795e77661fd03f68c8c6b51e7d6fd8a11ed776fe5319111101cb64b562082`. [Zakwalifikowane źródło](https://github.com/adrianf87/dc24h.eu-twilight/commit/068d77f35175b8f28dd326f5bc520a69c7d90661) · [PR #12](https://github.com/adrianf87/dc24h.eu-twilight/pull/12) · [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10)

| Obszar | Wynik |
|---|---|
| Build Release | PASS |
| Pełny CTest | PASS — 260/260 oraz osobny `systemd.sandbox` |
| systemd.sandbox | PASS — 4/4 podtesty w menedżerze VPS |
| IPv4 | PASS — osobna przestrzeń IPv4-only oraz próby w dual-stack |
| IPv6 | PASS — `::1` i wymuszony tryb ścisły |
| ADC | PASS |
| ADCS | PASS |
| CA + SAN | PASS — klient referencyjny OpenSSL |
| KEYP | PASS — ncdc 1.23.1 z wcześniej potwierdzonym odciskiem |
| Dwa klienty ncdc | PASS — ADC i ADCS po IPv4 i IPv6 |
| Autoryzacja | PASS |
| Stabilność | PASS |
| Dowody | COMPLETE — komplet dowodów lokalnych |

## Środowisko i izolacja

VPS miał 16 logicznych CPU x86-64 i 64 308 MiB RAM. Użyto jądra `6.12.117+deb13-cloud-amd64`, GCC/G++ 14.2.0 w trybie C++20, CMake/CTest 3.31.6, Python 3.13.5, OpenSSL 3.5.9, ncdc 1.23.1 z GnuTLS 3.8.9, systemd 257.13, Apache 2.4.71 i MariaDB 11.8.9 z Connector/C 3.4.10. Lokalny Node 22.23.5, rozpakowany w katalogu roboczym, obsłużył dodatkową próbę WebAdmin. Domyślny Node 20.19.2 rozpoznał wymóg Node ≥22 ze strony `@simplewebauthn/server`.

Działające instancje Apache i MariaDB pozostały aktywne. CTest, ncdc i pomocnicze próby TLS miały własne przestrzenie sieciowe, PID i montowań, z podniesionym wyłącznie `lo`. Próbne instancje MariaDB używały prywatnych plików i gniazd Unix. Test systemd użył menedżera VPS, konta `nobody`, losowych jednostek tymczasowych, własnych katalogów i listenera `127.0.0.1`. Menedżer działał mimo stanu `degraded` spowodowanego wcześniejszym błędem `systemd-networkd-wait-online.service`.

## Wyniki CTest

| Przebieg | PASS | FAIL | SKIP na poziomie CTest |
|---|---:|---:|---:|
| Pierwotny SHA, pełny zestaw poza systemd | 260 | 0 | 0 |
| Pierwszy commit poprawki, pełny zestaw poza systemd | 259 | 1 | 0 |
| Końcowy SHA, pełny zestaw poza systemd (94,13 s) | 260 | 0 | 0 |
| Poprawka, systemd w trybie ścisłym | 1 | 0 | 0 |
| Poprawka, IPv4-only | 3 | 0 | 0 |
| Poprawka, dual-stack wymagający IPv6 | 4 | 0 | 0 |
| Poprawka, kontrola negatywna wymuszająca IPv6 w IPv4-only | 0 | 2 oczekiwane | 0 |

CMake zarejestrował 261 pozycji, w tym 53 nazwane testy ncdc. Dokładnie 260 wykonało się w przebiegu szerokim, tak jak w poprzednim raporcie. `systemd.sandbox` był uruchomiony oddzielnie z `DC24H_REQUIRE_SYSTEMD_HOST=1`. We wszystkich przebiegach szerokich z `umask 022` przeszły `beta.07.ci.deb.prototype`, `apache2.https`, `apache2.https.proxy` i `apache2.remote.gateway`.

Poza CTest wystąpiły dwa pominięcia wewnętrzne na Debianie, oba przewidziane przez testy dla środowiska hosted Ubuntu. `beta.05.stage4.rollback` pominął 1/1 próbę odtwarzania. `apache2.remote.gateway` wykonał 12/13 prób i pominął 1 próbę długotrwałą HTTPS. Odpowiadający im workflow na GitHub był w toku przy zamknięciu raportu. Dodatkowa lokalna próba `beta08_postrollback_state_tests.py` wykonała 2/2 testy Node.

W IPv4-only oba testy CTM/RCM zgłosiły łącznie cztery pominięte warianty IPv6. W osobnym przebiegu ze sprawnym `::1` i `DC24H_REQUIRE_IPV6_LOOPBACK=1` wykonały wszystkie osiem wariantów IPv4/IPv6 × ADC/ADCS × CTM/RCM. Kontrola z tym samym wymuszeniem i wyłączonym IPv6 na `lo` odrzuciła oba testy z jawnym komunikatem o niedostępnym `::1` i zwróciła kod CTest 8.

## Systemd

Wszystkie cztery podtesty `systemd.sandbox` przeszły. Obejmowały dyrektywy i walidację jednostki, użycie sysusers we własnym katalogu, start rzeczywistego huba i próby zapisu. Potwierdzono UID bez uprawnień, `NoNewPrivs=1`, zerowe `CapEff` i `CapBnd`, katalogi prywatne `0700`, gniazdo Unix `0600` i prawidłowego właściciela. Zapis do `/etc`, `/usr` i `/root` został zablokowany, a zapis w przeznaczonych do tego katalogach się powiódł. Syntetyczny token administracyjny nie wystąpił w sprawdzanych logach. SIGTERM zakończył jednostkę ze statusem sukcesu. Dodatkowa jednostka potwierdziła odrębne i zapisywalne `/tmp`, przesłonięcie `/root` i montowania tylko do odczytu dla `/etc` oraz `/usr`. [Raport systemd](systemd-debian13.redacted.log)

## ADC, ADCS i ncdc

Dwa odrębne profile ncdc zalogowały się równocześnie w każdym z czterech wariantów macierzy ADC/ADCS i IPv4/IPv6. W każdym potwierdzono czat publiczny i wiadomości prywatne w obu kierunkach, aktywne wyszukiwanie dokumentu i pasywne wyszukiwanie wideo ze zwróconym wynikiem od drugiego ncdc. Pliki w udziałach były syntetyczne.

CTest potwierdził w rzeczywistym ncdc negocjację ADC i ADCS, logowanie i powrót po rozłączeniu oraz CTM/RCM z obserwatorem protokołu. Próby procesu sprawdziły routing CTM i RCM w obu rodzinach adresów. `accounts.adc_authentication.process` na prywatnej MariaDB potwierdził odmowę po błędnym haśle i poprawne zalogowanie z właściwym hasłem zarówno w ADC, jak i ADCS. Próba stabilności obejmowała odmowę duplikatu nicku, odmowę `+kick` dla gościa oraz limit i zwolnienie sesji.

| Tryb weryfikacji | Scenariusz | Obserwacja |
|---|---|---|
| CA + SAN, klient referencyjny | Zaufany testowy CA i `DNS:localhost` | Dozwolone TLS 1.3 i ADC NORMAL. CTest potwierdził również TLS 1.2. |
| CA + SAN, klient referencyjny | Połączenie po IP z samym DNS SAN | Odmowa, kod 64 |
| CA + SAN, klient referencyjny | Obce CA o tej samej nazwie | Odmowa, kod 20 |
| CA + SAN, klient referencyjny | Wygasły liść | Odmowa w połączeniu TLS, kod 10. Hub odrzucił też pliki przed otwarciem portu. |
| CA + SAN, klient referencyjny | Liść jeszcze nieważny | Odmowa w połączeniu TLS, kod 9. Hub odrzucił też pliki przed otwarciem portu. |
| KEYP, ncdc | Poprawny SHA-256 potwierdzony z lokalnego liścia w OpenSSL i Pythonie | Dozwolone połączenie także po IP przy liściu z samym DNS SAN |
| KEYP, ncdc | Błędny odcisk lub stary odcisk po wymianie liścia na tym samym porcie | Odmowa. Nowy poprawny pin przywrócił logowanie. |
| ADCS | Odmowa TLS i kontrola portów | Liczniki nie wykazały prób na listenerze ADC. CTest odrzucił jawny ADC na porcie ADCS i sprawdził szyfrowanie ruchu ncdc. |

Tryb CA/SAN ustala zaufanie do łańcucha i tożsamość dla wybranej nazwy. Jawny KEYP ncdc 1.23.1 porównuje odcisk liścia z wcześniej potwierdzoną wartością. Wbudowana akceptacja przy pierwszym użyciu ncdc działa przy braku pina. Próby kwalifikujące tożsamość w ncdc używały jawnego KEYP. Polityka OWNER dla przyszłego publicznego huba wymaga zaufanego CA i właściwego DNS SAN, z opcjonalnym KEYP potwierdzonym poza pasmem. [Instrukcja ncdc](https://dev.yorhel.nl/ncdc/man) · [Polityka OWNER](../../doc/pl/PUBLIC_STAGE_I_NCDC.md)

## Poprawka i regresja

Na pierwotnym SHA ncdc wysłał 18 pól `EX` przy `/search -t doc`. Wbudowany typ `video` wysyła 20. Limit 16 był wspólny dla walidatora SCH i Search Core. Hub zwracał `ISTA 100 Invalid search query`. Zmieniono wspólny limit na 20 i dodano do `adc.sch` obie komendy ncdc. Regresja wykonana z limitem 16 zakończyła się odmową. Pierwsza powtórka CTest na `b7dd286` ujawniła, że `adc.sch.process` nadal oczekiwał odmowy dla 17 pól. Zaktualizowano próbę procesu tak, aby sprawdzała przekazanie 20 pól i odrzucenie 21 na ADC i ADCS. Zmiana trafiła do `068d77f`. Próby graniczne, limity ramek i izolacja błędów alokacji przeszły na końcowym SHA. [Pierwotna odmowa](stage-i-8af360e/search-regression-debian13.redacted.log) · [JUnit pierwszej powtórki](stage-i-b7dd286/ctest-full-debian13.xml) · [Źródła pakietu ncdc](https://deb.debian.org/debian/pool/main/n/ncdc/ncdc_1.23.1.orig.tar.gz)

## Stabilność

W ograniczonej czasowo próbie ADC przeprowadzono 20 cykli połączenia i rozłączenia oraz 10 powrotów po kontrolowanej odmowie duplikatu nicku. Hub odrzucił trzecią próbę przy limicie dwóch sesji. Po zatrzymaniu i ponownym starcie na tym samym porcie dwa klienty ncdc ponownie weszły w stan NORMAL. Oba pokolenia huba i oba klienty zamknęły się z kodem 0. Przed restartem 43 połączenia przyjęto i wszystkie 43 zamknięto. Zarejestrowano 1 odrzucenie na limicie, 12 ostrzeżeń, w tym jednokrotny błąd I/O zamkniętej sesji, i 0 wpisów ERROR. Hub zużył 0,030 s CPU w 3,879 s pomiaru. RSS wzrósł z 8204 do 8828 KiB, a liczba deskryptorów z 8 do 10 w trakcie pracy i wróciła do 8. Po restarcie zamknięto obie sesje i odnotowano 0 ostrzeżeń oraz 0 błędów. [Dane stabilności](stability-debian13.redacted.log)

## GitHub Actions

Stan dla `068d77f35175b8f28dd326f5bc520a69c7d90661` o 02:20:47 UTC. Na polecenie OWNER monitorowanie zakończono przy tym stanie.

| Workflow | Wynik w chwili odczytu |
|---|---|
| [Audyt Stage IV i walidacja błędów](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45740911565) | PASS |
| [TSAN i parser fuzz](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45740911613) | PASS |
| [CodeQL](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45740922362) | W toku |
| [Macierz kompilacji i testów](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45740911498) | W toku |
| [Stage IV na izolowanych Ubuntu VM](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45740911552) | W toku |

Po pomyślnym zakończeniu pozostałych kontroli CI przekazać ten pakiet OWNER do odbioru Etapu I.

## Polecenia i dowody

Klon i weryfikacja refów poleceniami `git ls-remote`, `git clone --single-branch --branch public/stage-i-debian13-ncdc-adcs` oraz `git rev-parse HEAD`. Po wysłaniu poprawki zweryfikowano SHA i drzewo przez `git fetch`, `git rev-parse 'FETCH_HEAD^{tree}'` i `git write-tree`. Proces kwalifikacji był uruchomiony jako root na VPS.

```bash
# Z katalogu zweryfikowanego klonu
umask 022
source_dir=$PWD
work=/root/dc24h-stage1-work
build=$work/final-release
cmake -S "$source_dir" -B "$build" -DCMAKE_BUILD_TYPE=Release -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build "$build" --parallel 16
sha256sum "$build/dc24h-twilight"
"$build/dc24h-twilight" --version

# Skrypty kwalifikacyjne skopiowano do katalogu roboczego.
env -u DC24H_REQUIRE_IPV6_LOOPBACK bash "$work/run_qualification.sh" "$source_dir" "$build" "$work"

unshare --net --mount --pid --fork --kill-child --mount-proc bash -ec '
    mount -t tmpfs -o mode=1777,size=512M tmpfs /tmp
    ip link set lo up
    ulimit -n 256
    ulimit -v 2097152
    exec timeout -k 3s 120s python3 "$1/stage1_live.py" "$2" "$3/dc24h-twilight" /usr/bin/ncdc -v
' bash "$work" "$source_dir" "$build"
```

Przed próbami sprawdzono `/etc/os-release`, `uname`, `nproc`, `free`, `ip -brief addr show lo`, `ss -ltn`, `getent` i `systemctl list-units`. Dostępność menedżera potwierdzono przez `systemctl show -P Version` i `systemd-run --quiet --wait --pipe --collect -p RuntimeMaxSec=10s /usr/bin/true`. Wersje potwierdzono przez `dpkg-query -W`, `g++ --version`, `ctest --version`, `openssl version`, `python3 --version`, `ncdc --version` i `mariadb_config --cc_version`.

Dla pierwotnego SHA użyto osobnego `$work/release` z tymi samymi parametrami CMake i CTest. Diagnostyka regresji wykonała `cmake --build "$work/release" --target adc_sch_tests --parallel 16` i `ctest --test-dir "$work/release" -R '^adc\.sch$' --output-on-failure` przed zmianą limitu. Po zmianie przebudowano i powtórzono `ctest --test-dir "$work/release" --parallel 16 -R '^(adc\.sch|search\.(core|filters))$' --output-on-failure`. Pierwszy SHA poprawki zbudowano od zera w `$work/final-release`. Po zmianie wyłącznie testu procesu wykonano ponownie konfigurację i build dla końcowego SHA i sprawdzono identyczny SHA-256 binarki.

Node 22 pobrano z katalogu `https://nodejs.org/dist/latest-v22.x` przez `curl -fsS` wraz z `SHASUMS256.txt`. Weryfikacja `sha256sum -c -` poprzedziła `tar -xf`. W `webadmin/` wykonano `npm ci --ignore-scripts --no-audit --no-fund --cache "$work/npm-cache"` z Node 22 na początku `PATH`, a następnie w przestrzeni testowej `timeout 60s python3 tests/beta08_postrollback_state_tests.py -v`.

- [Oczyszczony CTest](ctest-debian13.redacted.log) · [Pełny JUnit](ctest-full-debian13.xml) · [JUnit systemd](ctest-systemd-debian13.xml)
- [JUnit IPv4-only](ctest-ipv4-debian13.xml) · [JUnit ścisły dual-stack](ctest-ipv6-strict-debian13.xml) · [JUnit kontroli negatywnej](ctest-ipv4-strict-negative-debian13.xml)
- [Wyniki ncdc i TLS](ncdc-adc-adcs-debian13.redacted.log) · [Skrypty kwalifikacji](qualification/) · [SHA256SUMS](SHA256SUMS)

Zachowano status i czas każdej pozycji CTest. Do opublikowanych logów i XML przeniesiono wyłącznie zweryfikowane podsumowania i komunikaty. Klucze, dane uwierzytelniające, zapisy terminali i surowe dane procesów z prób pozostają poza repozytorium.

**STAGE I: BLOCKED**
