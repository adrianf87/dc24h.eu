# Beta 0.5.1–0.5.4 — Etap I: implementacja i dowody

To **jednostka pracy 1** z [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234), na wspólnej gałęzi `beta-0.5.x-stage-full` z jednym roboczym PR. Wersja rozwojowa **nie oznacza** zgody na wydanie, wdrożenie ani zakończenia odbioru na sprzęcie docelowym.

**Decyzja właściciela (23.11.2026):** Nie wykonywać testów na Debianie 12/13. Wszystkie nowe testy i pomiary prowadzić wyłącznie na hostowanych runnerach GitHub Ubuntu 22.04/24.04/26.04. Nie testować procesora Intel Xeon D-1521 ani prywatnego runnera. Ta decyzja zmienia zakres bieżących działań, ale **nie tworzy dowodów** spełnienia oryginalnych kryteriów Debiana i sprzętu docelowego. Do ich formalnego pominięcia w odbiorze wydania potrzebna byłaby osobna zmiana kryteriów przez właściciela.

## Cztery kolejne poprawki

| Poprawka | Zmiana | Granica dowodu |
| --- | --- | --- |
| 0.5.1 — CI i testy | Ograniczono równoległość kompilacji/testów, zachowano Ubuntu 22.04/24.04/26.04, WebAdmin i ASan/UBSan, dodano niezmienne artefakty statusów CTest bez logów i sekretów oraz testy ich walidacji. | Brak pliku JUnit to `not-run`, nigdy sukces. Artefakt nie jest niezależnym audytem. |
| 0.5.2 — Zgodność | Wcześniejszy patch dodał testy kontenerowe Debiana, lecz po decyzji właściciela usunięto job i jego testy pomocnicze. C++20, MariaDB, Apache2, ADC/ADCS, Node 22/TypeScript/Vite są sprawdzane wyłącznie na trzech Ubuntu, z limitami czasu apt. | Brak nowych testów Debiana. Test systemd wymaga rzeczywistego menedżera i root/sudo; brak środowiska oznacza błąd, a nie pominięcie. |
| 0.5.3 — Przygotowanie pomiarów | Odrzucane są niekompletne odpowiedzi API. Walidator wymaga czterech profili, progów p95/p99/błędów/przepustowości zatwierdzonych przed pomiarem, obserwacji CPU/RSS/MariaDB i cech hosta docelowego. | Testy na sztucznych danych **nie** są wynikami pomiarów na Xeonie. |
| 0.5.4 — Spójna para kopii | Dodano manifest SHA-256, wiążący zrzut MariaDB i pięć plików gatewaya, z kontrolą prywatnych uprawnień, symlinków, zmian zawartości i brakiem nadpisywania. | Narzędzie **nie** wykonuje backupu SQL/importu, restartu usług ani pomiaru RPO/RTO. |

## CI oraz systemd

Roboczy PR do `main` uruchamia `.github/workflows/build.yml`. Wymagane są zakończone z sukcesem: C++ Debug i Release na trzech Ubuntu, trzy zadania WebAdmin, ASan i UBSan oraz zbiorczy `CI (C++ and WebAdmin)`. Brak jobów Debiana i prywatnych runnerów. Zapisz SHA ostatniego commita, linki do uruchomień i wyniki w Issue #234; stan oczekujący, pominięty, anulowany lub aktywny **nie** jest sukcesem. `scripts/ci_junit_summary.py` publikuje wyłącznie liczby testów/status/SHA, bez surowych logów, tokenów, odpowiedzi API i prywatnych ścieżek.

Zmienna `DC24H_REQUIRE_SYSTEMD_HOST=1` sprawia, że test Ubuntu kończy się błędem, jeżeli nie ma prawdziwego menedżera systemd lub uprawnień root/sudo. Testy używają `systemd-run` i jednostki testowej, nie symulacji kontenera.

## Pomiary Ubuntu w GitHub CI (nie odbiór sprzętu docelowego)

Na każdej z trzech wersji Ubuntu workflow zbiera kontrolowane próbki ADC/ADCS na loopback: p50/p95/p99, udane żądania/s, błędy, RSS przed/po oraz przybliżony procent czasu CPU procesu względem jednego wątku. Jednorazowa baza MariaDB w teście CLI dostarcza czas kopii i odtwarzania oraz kontrolę schematu/poświadczeń. Raporty CTest i Chromium dają p50/p95/p99 **czasu wykonania testów**, nie latencji żądań HTTP/SQL. Do artefaktu trafiają tylko agregaty, bez surowego JUnit i danych prywatnych.

Narzędzie czterech profili i walidator danych docelowych pozostają w repozytorium na przyszłość, ale **nie wykonujemy ich** na Debianie ani Xeonie w obecnej jednostce:

```sh
python3 scripts/stage1_perf_gate.py --policy /private/stage1-policy.json --evidence /private/stage1-observations.json
```

Ta komenda wymaga zatwierdzonych wcześniej realnych danych docelowych i **nie jest uruchamiana przez bieżące CI**. Pomiary Ubuntu nie dowodzą wydajności Debiana 13/Xeona ani RPO/RTO. Raporty zachowują `capacity_claim: false`.

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
| Pełne najnowsze GitHub Actions na Ubuntu 22.04/24.04/26.04 | **PENDING** |
| Prawdziwy systemd na trzech runnerach Ubuntu | **PENDING** |
| Zgodność Debian 12/13 | **ODROCZONE PRZEZ WŁAŚCICIELA — bez testów** |
| Pomiary wydajności Xeona i zatwierdzone progi | **NOT MEASURED — właściciel zabrania testów docelowych w tej jednostce** |
| Odtworzenie MariaDB + plików, restart, rollback i RPO/RTO | **NOT MEASURED** |
| Niezależna akceptacja operatora | **PENDING — NO-GO** |

Nie zaznaczaj jednostki 1 jako wykonanej do uzyskania wszystkich dowodów i zielonego CI dla ostatniego SHA. Nie rozpoczynaj etapu II na to samo polecenie, nie scalaj, nie zamykaj Issue #234 i nie wdrażaj automatycznie.
