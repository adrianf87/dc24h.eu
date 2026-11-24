# Beta 0.7.17–0.7.20 — Etap V: kwalifikacja wydania i instrukcja operatora

**Status: NO-GO — bez merge, wdrożenia i aktywacji operatora.** Kandydat deweloperski `beta-0.7.20` na wspólnej gałęzi `beta-0.7.x-stage-full`, [Issue #238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), roboczy [PR #239](https://github.com/adrianf87/dc24h.eu-high-starter/pull/239). Sam commit lub uruchomienie CI nie oznacza akceptacji. Wszystkie pięć późniejszych etapów napraw i decyzja Ownera pozostają odrębne.

## 0.7.17 — porównywalne pomiary

Wyłącznie GitHub-hosted Ubuntu 22.04/24.04/26.04. Trzy okna w każdym profilu: ADC i ADCS — po **32 żądania**, współbieżność 4, tempo docelowe 8/s; uwierzytelniony HTTPS C++→Node→Apache — po **8 żądań** hasło+TOTP, współbieżność 2 i odstępy zgodne z limiterem. Raporty muszą podawać p50/p95/p99, RPS poprawnych żądań, błędy, czas oraz obserwacje CPU/RSS. Obowiązują wcześniej ustalone szerokie limity regresji CI. **Nie porównuj bezpośrednio RPS różnych protokołów**, ich koszt uwierzytelnienia i obciążenie się różnią. Żaden wynik nie jest pomiarem wydajności produkcyjnego serwera Debian/Xeon ani gwarancją SLA.

**Fix Pass 5:** walidator ponownie wylicza percentyle z posortowanych czasów syntetycznych żądań, sprawdza oddzielne CPU C++/Node/Apache, liczbę próbek i limity, a końcowa bramka sprawdza czasy przypadków przeciążeniowych i zgodność rzeczywistego lockfile. Szczegóły: [raport Fix Pass 5](BETA_0.7_FIX_PASS_5.md). Nadal wymagana jest niezależna weryfikacja pochodzenia wyników GitHub CI.

## 0.7.18 — przeciążenie i zakończenie pracy

Izolowane testy procesu sprawdzają półotwarte TLS, powolne czytanie ADC/ADCS, zaległy bufor, odmowy i limity kolejek, równoległego zdrowego klienta, reconnect oraz SIGTERM/drain. Dodatkowa bramka `beta07_bounded_fault_gate.py` wymaga przejścia pięciu zestawów CTest w limicie **240 sekund na przypadek**. Wewnętrzne testy mają własne ostrzejsze granice kolejek, pamięci i zamknięcia. ASan/UBSan nadal są obowiązkowe. Raport zawiera tylko stałe nazwy przypadków i czas — nigdy surowe logi ani dane klientów.

## 0.7.19 — zależności i pochodzenie kodu

- Każdy commit i wynik musi odpowiadać dokładnemu SHA i dozwolonemu obrazowi Ubuntu. Akcje GitHub mają pełne przypięte SHA.
- Lockfile npm zawiera wersje i sumy integralności. Inwentaryzacja SBOM wypisuje tylko publiczne nazwy pakietów, wersje i skróty integralności; brak wpisu lub nieznany rejestr skutkuje odmową.
- `npm audit` musi zakończyć się poprawnie, a liczba krytycznych/wysokich podatności zależności produkcyjnych wynosić zero. Prywatny JSON audytu jest tymczasowy; publikowane są tylko zbiorcze liczniki. CodeQL C++/TypeScript i ASan/UBSan sprawdzane są oddzielnie na tym samym końcowym SHA.

## 0.7.20 — kandydująca wersja

C++ (hub, API, kontroler), Node/React WebAdmin, testy, CMake, npm manifest/lockfile, README i changelog wskazują `beta-0.7.20`. **To nie jest zgoda na wydanie, tag ani wdrożenie.** MariaDB pozostaje przy schemacie v11; WebAdmin zdalny jest lokalny, tylko do odczytu danych zbiorczych, bez zdalnej zmiany uprawnień, uruchamiania huba czy logowania bez hasła.

Bramka `beta07_stage1_completion_gate.py` sprawdza deklaracje 20 patchy, **17 wymaganych zakończonych sukcesem zadań CI** i dziesięciu kategorii dowodów na jednym SHA. Nowa `beta07_release_candidate_gate.py` dodatkowo odmawia przy braku lub niespójności **21 raportów** (7 typów × 3 Ubuntu): SBOM, npm audit, pomiary, awarie, dpkg, MariaDB+5 plików i chaos.

**Ważne:** bramka offline nie potwierdza samodzielnie autentyczności deklaracji z GitHub Actions. Owner/operator musi osobno porównać prawdziwe wyniki API GitHub, nazwy zadań, run ID/attempt, SHA i pochodzenie artefaktów. Nawet pozytywny test struktury raportów zwraca `merge_authorized=false`. Gdy CI jest w toku, status pozostaje `NO-GO`.

## Procedura izolowanego odtwarzania i cofania wersji

1. Po osobnej zgodzie i wyłącznie na odizolowanym hoście zatrzymaj wszystkich autorów zapisu MariaDB i WebAdmin. Blokada SQL nie zastępuje zatrzymania zapisu pięciu plików.
2. Zabezpiecz jeden sparowany zestaw: eksport SQL v11, `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`, manifest prywatny i `recovery-pair.json`. Zapis wyłącznie dla właściciela, bez dowiązań i częściowych kopii. Nie publikuj kopii ani sekretów.
3. Zweryfikuj SHA źródła i wszystkie sumy, schematy prywatne oraz 11 migracji narzędziem `beta07_recovery_preflight.py`, wskazując **nieistniejące jeszcze** katalogi docelowe. Przy odmowie zachowaj oryginały bez zmian.
4. Wykonaj import SQL do **świeżej, testowej** instancji MariaDB i odtworzenie pięciu plików do nowego prywatnego katalogu. Sprawdź `--database-status`, rolę i uprawnienia kont, weryfikatory, cofnięte passkeys i epoch, audyt, replay oraz limity prób. W razie niezgodności nie uruchamiaj usług.
5. W teście pakietów `.deb` potwierdź instalację poprzedniej i nowej wersji, odmowę uszkodzonego pakietu i celowo nieudanego `preinst`, zachowanie stanu dpkg, **rzeczywisty downgrade** i ponowną kontrolę zestawu po restarcie MariaDB. Pakiet CI nie jest pakietem produkcyjnym; jego jednostka systemd ma nieaktywny warunek.
6. Odtworzenie produkcyjne, rzeczywisty czas niedostępności/RPO/RTO i zarządzanie usługami wymagają odrębnej zgody i próby operacyjnej Ownera. Nie przenoś wniosków ze środowiska CI na działający hub.

## Rejestr ryzyk do decyzji Ownera

- DC++ i AirDC++: bez niezależnego uruchomienia nie ma certyfikacji; dowody dotyczą przypiętego `ncdc`.
- MariaDB / pięć plików: zgodność tylko w próbie izolowanej; wspólna generacja wymaga faktycznego zatrzymania zapisu; brak produkcyjnego RPO/RTO.
- dpkg: przetestowano awarię **przed rozpakowaniem**; awaria `postinst`, migracja produkcyjna i dostępność w czasie aktualizacji pozostają ryzykiem.
- Hasła, TOTP, passkeys: zawsze hasło + drugi czynnik, bez możliwości obejścia uprawnień C++; po cofnięciu passkey wymagane nowe logowanie.
- Wydajność/DoS: krótkie okna na GitHub-hosted Ubuntu nie potwierdzają parametrów serwera Debian 13 ani odporności na wszystkie ataki.
- Audyt/obserwowalność: utrata lub brak świeżych danych oznacza `unknown`, nie stan zdrowy; historia zbiorcza nie jest trwałym dziennikiem śledczym.
- Zależności i CI: nowe podatności, awarie CodeQL lub brak wyniku konkretnego obrazu blokują kwalifikację.

## Przekazanie do kolejnych etapów

Po pełnym CI na **tym samym dokładnym SHA** należy pobrać i zweryfikować z GitHub Actions rzeczywiste wyniki i wyłącznie oczyszczone artefakty. Następnie wykonać osobno **Fix Pass 1–5**, ponowić końcowe CI i uzyskać decyzję Ownera. Nie wypełniaj ręcznie pozytywnych pól dla wyników pending/skipped. **Nie wykonuj merge ani wdrożenia bez wyraźnej dyspozycji.**

Zgodnie z poleceniem Ownera po wypchnięciu commita i uruchomieniu testów **kończymy działanie, nie czekając na wyniki**. Status pozostaje **NO-GO** do osobnej akceptacji.
