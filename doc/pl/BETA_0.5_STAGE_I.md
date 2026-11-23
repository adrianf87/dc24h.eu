# Beta 0.5.1–0.5.4 — Etap I: implementacja i dowody

To **jednostka pracy 1** z [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234), na wspólnej gałęzi `beta-0.5.x-stage-full` z jednym roboczym PR. Wersja rozwojowa **nie oznacza** zgody na wydanie, wdrożenie ani zakończenia odbioru na sprzęcie docelowym.

## Cztery kolejne poprawki

| Poprawka | Zmiana | Granica dowodu |
| --- | --- | --- |
| 0.5.1 — CI i testy | Ograniczono równoległość kompilacji/testów, zachowano Ubuntu 22.04/24.04/26.04, WebAdmin i ASan/UBSan, dodano niezmienne artefakty statusów CTest bez logów i sekretów oraz testy ich walidacji. | Brak pliku JUnit to `not-run`, nigdy sukces. Artefakt nie jest niezależnym audytem. |
| 0.5.2 — Debian i Ubuntu | Dodano kontenerowe testy C++20, MariaDB, Apache2, ADC/ADCS, Node 22/TypeScript/Vite na Debianie 12/13 oraz wymaganie rejestracji integracji. Instalacja apt działa jako root lub przez sudo z limitami czasu. | Kontener **nie** potwierdza działania usługi pod prawdziwym systemd ani sprzętu Xeon. |
| 0.5.3 — Przygotowanie pomiarów | Odrzucane są niekompletne odpowiedzi API. Walidator wymaga czterech profili, progów p95/p99/błędów/przepustowości zatwierdzonych przed pomiarem, obserwacji CPU/RSS/MariaDB i cech hosta docelowego. | Testy na sztucznych danych **nie** są wynikami pomiarów na Xeonie. |
| 0.5.4 — Spójna para kopii | Dodano manifest SHA-256, wiążący zrzut MariaDB i pięć plików gatewaya, z kontrolą prywatnych uprawnień, symlinków, zmian zawartości i brakiem nadpisywania. | Narzędzie **nie** wykonuje backupu SQL/importu, restartu usług ani pomiaru RPO/RTO. |

## CI oraz systemd

Roboczy PR do `main` uruchamia `.github/workflows/build.yml`. Wymagane są zakończone z sukcesem: C++ Debug i Release na trzech Ubuntu, trzy zadania WebAdmin, ASan i UBSan, oba testy Debian oraz zbiorczy `CI (C++ and WebAdmin)`. Zapisz SHA ostatniego commita, linki do uruchomień i wyniki w Issue #234; stan oczekujący, pominięty, anulowany lub aktywny **nie** jest sukcesem. `scripts/ci_junit_summary.py` publikuje jedynie liczby testów/status/SHA, bez surowych logów, tokenów, odpowiedzi API czy prywatnych ścieżek; 14-dniowe artefakty są unikatowe dla próby CI.

Testy na Debianie uruchamiają się w kontenerach na publicznej infrastrukturze GitHub. Kontener bez działającego menedżera systemd nie potwierdza bezpieczeństwa `systemd-run`. Test `systemd.sandbox` trzeba przeprowadzić oddzielnie na autoryzowanym, nieprodukcyjnym hoście lub VM z rzeczywistym systemd. Zapisz wersję systemu, unit, ograniczenia procesu, restart, awarię i dziennik — nie uznawaj pominięcia testu w Dockerze za zaliczenie.

## Wydajność docelowa — do wykonania przez operatora

Wymagany host to **Debian 13 Trixie, Intel Xeon D-1521, osiem wątków, 31 997 MiB RAM**. AMD EPYC i kontener Debian nie są zamiennikami. Przygotuj izolowaną replikę ze sztucznymi kontami, loopback ADC/ADCS/HTTPS, zaufanym certyfikatem oraz osobną MariaDB i Apache2. Przed próbą uzgodnij i oznacz czasowo progi dla każdego profilu: max p95/p99, max procent błędów, min udane żądania/s; wspólne progi: max CPU %, szczyt RSS KiB, liczba wątków i błędów MariaDB. Plik progów pozostaje poza Git. `approved_before_utc` musi poprzedzać `measurement_started_utc`.

Uruchom cztery profile ograniczonego testu z [procedury pomiarowej](BETA_0.4_STAGE_IV_PERFORMANCE.md) z `--ack-isolated`, certyfikatem CA i prywatnym plikiem sesji dla API. W pliku obserwacji umieść raporty `adc`, `adcs`, `webadmin`, `api` w `profiles`, rzeczywiście zmierzone CPU/RSS/MariaDB w `resources`, a dokładne cechy hosta i SHA w `target`. Następnie:

```sh
python3 scripts/stage1_perf_gate.py --policy /private/stage1-policy.json --evidence /private/stage1-observations.json
```

Wynik może potwierdzić wyłącznie spełnienie liczbowych progów; zawsze ma `capacity_claim: false` i `release_approved: false`. Dowody i zgody przechowuj poza repozytorium. Ocenę obciążenia i błędów zatwierdza niezależny operator.

## Kopia MariaDB i gatewaya — do wykonania przez operatora

Na **jednorazowym, autoryzowanym** pilotażu zatrzymaj gateway i wstrzymaj zapisy huba; potwierdź stan obu usług. Utwórz transakcyjnie spójny zrzut `mariadb-dump` przez prywatne gniazdo Unix, z osobnym plikiem uwierzytelniającym `0600`; sprawdź status, liczby rekordów i datę. Zrzut i pięć plików gatewaya muszą pochodzić z *jednego okna spójności po zatrzymaniu zapisów*, według [procedury odzyskiwania](BETA_0.4_STAGE_IV_RECOVERY.md). Wszystkie katalogi prywatne mają uprawnienia `0700`.

Po wykonaniu migawki i przygotowaniu pustego prywatnego katalogu `pair`, powiąż i zweryfikuj pliki (podmień przykładowe ścieżki i tekst SHA na rzeczywiste, dokładnie 40 małych znaków szesnastkowych):

```sh
python3 scripts/stage1_recovery_bundle.py pair --mariadb-dump /private/pilot.sql --private-snapshot /private/gateway-snapshot --pair-directory /private/pair --commit-sha FULL40CHARSHa --ack-stopped --ack-isolated
python3 scripts/stage1_recovery_bundle.py verify --mariadb-dump /private/pilot.sql --private-snapshot /private/gateway-snapshot --pair-directory /private/pair --ack-isolated
```

Manifest zawiera wyłącznie hashe, rozmiary, znacznik czasu i SHA; nie jest podpisany i sam nie dowodzi równoczesności kopii. Na świeżej, oddzielnej bazie odtwórz zrzut, sprawdź schemat v11, rekordy i uprawnienia, odtwórz prywatne pliki **do nowego katalogu**, uruchom pilotażowe usługi i sprawdź hasło + TOTP/passkey, odwołanie sesji/uprawnień i ciąg audytu. Zasymuluj awarię, zmierz RPO/RTO oraz wycofanie wersji. Nie importuj zrzutu z `CREATE/DROP DATABASE` do produkcji i nie nadpisuj aktywnego katalogu. Drugi operator musi zweryfikować wyniki.

## Bramka etapu — bez automatycznego zatwierdzenia

| Wymaganie | Stan bez autoryzowanych dowodów |
| --- | --- |
| Pełne najnowsze GitHub Actions z Debianem | **PENDING** |
| Zgodność i rzeczywisty menedżer systemd na Debianie/Ubuntu | **PENDING** |
| Pomiary wydajności Xeona i zatwierdzone progi | **NOT MEASURED** |
| Odtworzenie MariaDB + plików, restart, rollback i RPO/RTO | **NOT MEASURED** |
| Niezależna akceptacja operatora | **PENDING — NO-GO** |

Nie zaznaczaj jednostki 1 jako wykonanej do uzyskania wszystkich dowodów i zielonego CI dla ostatniego SHA. Nie rozpoczynaj etapu II na to samo polecenie, nie scalaj, nie zamykaj Issue #234 i nie wdrażaj automatycznie.
