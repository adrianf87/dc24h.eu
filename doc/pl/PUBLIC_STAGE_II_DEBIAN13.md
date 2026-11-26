# Publiczny hub — Etap II: instalacja i odzyskiwanie Debian 13 (P0)

**Zadanie:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), Etap II.  
**Stan:** rozpoczęcie prac i gotowy preflight inwentaryzacyjny tylko do odczytu. **Odbiór Etapu II pozostaje otwarty.**  
**Zakres:** bez instalowania usługi na VPS, migracji danych prywatnych, otwierania publicznych portów, zmian DNS/firewall, uruchamiania WebAdmin lub decyzji produkcyjnej.

Etap I ma [zaakceptowany raport Debian 13/ncdc](../../logs/tests/DEBIAN13-STAGE-I-FINAL-ACCEPTANCE.md). Etap II wymaga odrębnych dowodów instalacji i odzyskiwania: zielony CI i testowy pakiet `.deb` nie świadczą o bezpieczeństwie instalacji na istniejącym Debianie.

## 1. Pierwsza część — bezpieczna inwentaryzacja

`scripts/public_stage2_preflight.py` **niczego nie zapisuje** i zwraca ograniczony raport JSON na stdout. Uruchom na hoście zaakceptowanym przez OWNER do inwentaryzacji, z wcześniej sprawdzonego i przygotowanego checkoutu. Kompletny odczyt procesów i metadanych wymaga uprawnień roota; bez nich preflight blokuje odbiór:

```sh
stage2_sha="<zatwierdzony-dokladny-40-znakowy-sha>"
test "$(git rev-parse HEAD)" = "$stage2_sha" || exit 2
sudo python3 scripts/public_stage2_preflight.py --source-dir . --expected-sha "$stage2_sha"
```

Preflight nie instaluje pakietów, nie zatrzymuje usług, nie czyta sekretów ani prywatnej bazy, nie tworzy znacznika zgody i nie zmienia listenerów. Sprawdza:

- Dokładny SHA źródła, Debian 13, jądro, architekturę, CPU, systemd jako PID 1 i dostępność menedżera.
- Obecność wymaganych narzędzi (bez zrzucania zmiennych środowiskowych).
- Stany `dc24h.service` i `dc24h-twilight.service`, liczbę procesów huba i obecność znacznika OWNER (nigdy treść).
- Tylko typ, tryb i UID/GID **znanych z góry** ścieżek instalacyjnych poprzez `lstat` (symlinki są sygnalizowane, nie odczytywane).
- Liczby adresów IPv4/IPv6, **bez ujawniania IP**. Topologia NAT i rzeczywisty dostęp z Internetu wymagają osobnego sprawdzenia.
- Kontrakt przykładowej jednostki: `Conflicts`, znacznik zgody i ścieżki; zgodność `server.name` WebAdmin ze źródłem.

Kod wyjścia **0** oznacza wyłącznie kompletną inwentaryzację bez wykrytych blokerów (`INVENTORY_CLEAR`), **nie** zgodę na instalację. Kod **2** (`BLOCKED`) oznacza brak warunku lub nieznany stan usługi. Brak uprawnień, nieczytelny znacznik/ścieżka lub przejściowy stan usługi także blokuje wynik. Pola `production_go`, `private_state_migration_go`, `public_listener_go` są zawsze `false`. Przed dodaniem raportu na GitHub sprawdź jego redakcję, w tym czy wolno opublikować lokalne UID/GID.

Adres `51.255.80.233` może być trasowany lub mieć NAT. Nie zakładaj, że jest adresem interfejsu VPS; preflight nie jest testem portów publicznych.

## 2. Powtarzalny build i instalacja do katalogu testowego

Na **jednorazowej VM/klonie Debian 13**, a nie w katalogach działającej usługi, po sprawdzeniu SHA:

```sh
stage2_root="$(mktemp -d -t twilight-stage2-XXXXXXXX)"
umask 022
cmake -S . -B "$stage2_root/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$stage2_root/prefix" \
  -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build "$stage2_root/build" --parallel 2
ctest --test-dir "$stage2_root/build" --output-on-failure
"$stage2_root/build/dc24h-twilight" --version
"$stage2_root/build/dc24h-twilight" --config config/dc24h.ini.example --check-config
cmake --install "$stage2_root/build"
sha256sum "$stage2_root/build/dc24h-twilight" \
  "$stage2_root/prefix/bin/dc24h-twilight"
```

Porównaj obie sumy binarki. WebAdmin powinien wymagać dokładnie `server.name = dc24h.eu-twilight`. Do kompilacji WebAdmin użyj Node.js 22+, lockfile, `npm ci`, `npm run build`, a opcjonalne `DC24H_INSTALL_WEBADMIN=ON` tylko w **izolowanym** checkout. Nie publikuj zbudowanych zasobów ani testowego proxy w Internecie.

Nie stosuj `cmake --install` z prefiksem `/usr` lub `/usr/local` na żywym VPS. Istniejący `.deb` jest **wyłącznie testowym fixture**, nie pakietem instalacyjnym produkcji.

## 3. Plan testów systemd, upgrade i rollback — nadal otwarty

1. Zachować użytkownika/grupę `dc24h` oraz katalogi `/etc/dc24h`, `/var/lib/dc24h`, `/run/dc24h`, `/var/log/dc24h`. Nie tworzyć dwóch procesów zapisujących ten sam stan.
2. Zweryfikować kopię `dc24h-twilight.service` przez `systemd-analyze verify`. Zachować `Conflicts=dc24h.service` i `AssertPathExists=/etc/dc24h/twilight-unit-migration-approved`.
3. **Nie tworzyć prawdziwego znacznika**, nie uruchamiać i nie zatrzymywać prawdziwych usług. Próby systemd przeprowadzić w odizolowanej VM z syntetycznymi nazwami i markerem — po osobnej decyzji dotyczącej rzeczywistego cutover.
4. Na **syntetycznym stanie** zmierzyć i zapisać właścicieli, ACL/xattrs, uprawnienia i hashe. Wykonać backup i osobny test restore przed upgrade.
5. Przeprowadzić preflight cold-start/checkpoint i scenariusze fail-fast: brak klucza, niedostępna baza, zła wersja schematu, konflikt jednostek, błędny UID i przerwana instalacja. Nie zakładać dostępności atomowej migracji prywatnego stanu v1→v2.
6. Wstrzyknąć awarię między stagingiem i aktywacją; potwierdzić możliwość odzyskania starej binarki, usługi oraz stanu. Wykonać rollback po poprawnym syntetycznym upgrade. Zmierzyć RPO/RTO, wykluczyć równoległy start.

## 4. Monitoring i kryteria przerwania

Zbierać CPU, RSS, FD, gniazda, restarty, ostrzeżenia/błędy journald i dostępność backendu bez haseł, tokenów, kluczy, zawartości prywatnego stanu i identyfikatorów użytkowników. **STOP / NO-GO** przy nieznanej aktywnej usłudze, konflikcie starego/nowego huba, złych uprawnieniach, nieodtwarzalnym backupie, zmianie schematu/klucza, niewyjaśnionym FAIL albo wyjściu poza izolowane ścieżki.

## 5. Warunki ukończenia Etapu II

- [ ] Sprawdzona inwentaryzacja **rzeczywistego** VPS przez OWNER (bez mutacji).
- [ ] Powtarzalna czysta instalacja C++/WebAdmin z exact SHA i `--check-config` na izolowanym Debianie 13.
- [ ] Test systemd w odizolowanym środowisku: zgodny marker, odmowa bez niego, konflikt starej/nowej jednostki, brak dwóch writerów.
- [ ] Syntetyczny backup **z odtworzeniem**, cold start, upgrade, awaria, fail-fast, rollback i dowody UID/GID/ACL/xattrs.
- [ ] Monitoring, limity, zmierzone RPO/RTO i kryteria STOP/wycofania.
- [ ] CI końcowego SHA, testy negatywne, odrębne `OWNER STAGE II ACCEPTANCE` i `OWNER MERGE GO`.

**Wszystkie pola odbioru nadal otwarte.** Etap II nie autoryzuje rzeczywistej migracji ani publicznego huba/WebAdmin — wymagana osobna decyzja OWNER z [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10).
