# Beta 0.5.1–0.5.4 — Etap I: implementacja i dowody

To **jednostka pracy 1** z [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234), na wspólnej gałęzi `beta-0.5.x-stage-full` z jednym roboczym PR. Wersja rozwojowa **nie oznacza** zgody na wydanie, wdrożenie ani zakończenia odbioru na sprzęcie docelowym.

**Aktualny zakres Issue #234 (23.11.2026):** Nie wykonywać testów na Debianie 12/13. Wszystkie testy i pomiary prowadzić wyłącznie na hostowanych runnerach GitHub Ubuntu 22.04/24.04/26.04. Nie testować Intel Xeon D-1521 ani prywatnego runnera. Odbiór na sprzęcie docelowym i niezależny przegląd nie są warunkami zamknięcia tego zadania.

## Cztery kolejne poprawki

| Poprawka | Zmiana | Granica dowodu |
| --- | --- | --- |
| 0.5.1 — CI i testy | Ograniczono równoległość kompilacji/testów, zachowano Ubuntu 22.04/24.04/26.04, WebAdmin i ASan/UBSan, dodano niezmienne artefakty statusów CTest bez logów i sekretów oraz testy ich walidacji. | Brak pliku JUnit to `not-run`, nigdy sukces. Artefakt nie jest niezależnym audytem. |
| 0.5.2 — Zgodność | Wcześniejszy patch dodał testy kontenerowe Debiana, lecz po decyzji właściciela usunięto job i jego testy pomocnicze. C++20, MariaDB, Apache2, ADC/ADCS, Node 22/TypeScript/Vite są sprawdzane wyłącznie na trzech Ubuntu, z limitami czasu apt. | Brak nowych testów Debiana. Test systemd wymaga rzeczywistego menedżera i root/sudo; brak środowiska oznacza błąd, a nie pominięcie. |
| 0.5.3 — Przygotowanie pomiarów | Odrzucane są niekompletne odpowiedzi API. Walidator wymaga czterech profili, progów p95/p99/błędów/przepustowości zatwierdzonych przed pomiarem, obserwacji CPU/RSS/MariaDB i cech hosta docelowego. | Testy na sztucznych danych **nie** są wynikami pomiarów na Xeonie. |
| 0.5.4 — Odtwarzanie spójnej pary | Dodano manifest SHA-256, wiążący zrzut MariaDB i pięć plików gatewaya, oraz test CI `beta.05.stage1.matched.recovery`: zrzut/import MariaDB v11, odtworzenie plików, awaria źródłowej bazy, weryfikacja poświadczeń/roli/odwołania, ponowny start i rollback na nowych instancjach z pomiarem RPO/RTO. | Test obejmuje **syntetyczne** dane i restart procesu bazy, ale nie logowanie do uruchomionego gatewaya Node ani odzyskanie produkcji. |

## CI oraz systemd

Roboczy PR do `main` uruchamia `.github/workflows/build.yml`. Wymagane są zakończone z sukcesem: C++ Debug i Release na trzech Ubuntu, trzy zadania WebAdmin, ASan i UBSan oraz zbiorczy `CI (C++ and WebAdmin)`. Brak jobów Debiana i prywatnych runnerów. Zapisz SHA ostatniego commita, linki do uruchomień i wyniki w Issue #234; stan oczekujący, pominięty, anulowany lub aktywny **nie** jest sukcesem. `scripts/ci_junit_summary.py` publikuje wyłącznie liczby testów/status/SHA, bez surowych logów, tokenów, odpowiedzi API i prywatnych ścieżek.

Zmienna `DC24H_REQUIRE_SYSTEMD_HOST=1` sprawia, że test Ubuntu kończy się błędem, jeżeli nie ma prawdziwego menedżera systemd lub uprawnień root/sudo. Testy używają `systemd-run` i jednostki testowej, nie symulacji kontenera.

Każda macierz C++ Debug/Release wykonuje też na Ubuntu test wspólnego odtworzenia. Artefakt `stage1-matched-recovery.json` zapisuje tylko SHA, system, schemat v11, liczbę plików, wynik kontroli integralności, syntetyczne RPO, czas RTO i rollback oraz ograniczenia. Nie wysyła zrzutów SQL, prywatnych plików ani danych uwierzytelniających. Brak `mariadb-dump` blokuje wymaganą konfigurację CI.

## Pomiary Ubuntu w GitHub CI (nie odbiór sprzętu docelowego)

Na każdej z trzech wersji Ubuntu workflow zbiera kontrolowane próbki ADC/ADCS na loopback: p50/p95/p99, udane żądania/s, błędy, RSS przed/po oraz przybliżony procent czasu CPU procesu względem jednego wątku. Jednorazowa baza MariaDB w teście CLI dostarcza czas kopii i odtwarzania oraz kontrolę schematu/poświadczeń. Raporty CTest i Chromium dają p50/p95/p99 **czasu wykonania testów**, nie latencji żądań HTTP/SQL. Do artefaktu trafiają tylko agregaty, bez surowego JUnit i danych prywatnych.

Narzędzie czterech profili i walidator danych docelowych pozostają w repozytorium na przyszłość, ale **nie wykonujemy ich** na Debianie ani Xeonie w obecnej jednostce:

```sh
python3 scripts/stage1_perf_gate.py --policy /private/stage1-policy.json --evidence /private/stage1-observations.json
```

Ta komenda wymaga osobno przygotowanych danych i **nie jest uruchamiana przez bieżące CI**. Pomiary Ubuntu nie dowodzą wydajności Debiana 13/Xeona ani produkcyjnego RPO/RTO. Raporty zachowują `capacity_claim: false`.

## Granice testu odtwarzania na Ubuntu

CI tworzy trzy osobne katalogi danych MariaDB z komunikacją wyłącznie przez lokalny Unix socket. Pierwsza baza otrzymuje migracje do v11 i sztuczne wiersze poświadczeń/uprawnień. W tym samym kontrolowanym oknie test tworzy pięć poprawnych plików prywatnego stanu, ich migawkę, zrzut SQL i manifest pary. Po zatrzymaniu bazy źródłowej odtwarza SQL i pliki do **nowych** katalogów, sprawdza wersję schematu, bajty poświadczeń, role, odwołanie klucza passkey, licznik replay i łańcuch audytu. Następnie wywołuje zmianę roli i drugą awarię, po czym wraca do pierwotnego punktu w **trzeciej** bazie.

Nie ma zatwierdzonych zapisów po migawce, więc utrata w kontrolowanym scenariuszu wynosi 0 zapisów, a syntetyczne `RPO = 0 s`. RTO oznacza czas od celowego zatrzymania do weryfikacji odtworzonego stanu. Limit testowy to 120 sekund na odtworzenie i rollback; **nie jest to SLA produkcyjne**. Test nie uruchamia gatewaya Node i nie dowodzi logowania hasło + TOTP/passkey; raport jawnie oznacza `gateway_runtime_login_retested: false`.

## Dodatkowa próba operatora (poza bramką Issue #234)

Na **jednorazowym, autoryzowanym** pilotażu zatrzymaj gateway i wstrzymaj zapisy huba; potwierdź stan obu usług. Utwórz transakcyjnie spójny zrzut `mariadb-dump` przez prywatne gniazdo Unix, z osobnym plikiem uwierzytelniającym `0600`; sprawdź status, liczby rekordów i datę. Zrzut i pięć plików gatewaya muszą pochodzić z *jednego okna spójności po zatrzymaniu zapisów*, według [procedury odzyskiwania](BETA_0.4_STAGE_IV_RECOVERY.md). Wszystkie katalogi prywatne mają uprawnienia `0700`.

Po wykonaniu migawki i przygotowaniu pustego prywatnego katalogu `pair`, powiąż i zweryfikuj pliki (podmień przykładowe ścieżki i tekst SHA na rzeczywiste, dokładnie 40 małych znaków szesnastkowych):

```sh
python3 scripts/stage1_recovery_bundle.py pair --mariadb-dump /private/pilot.sql --private-snapshot /private/gateway-snapshot --pair-directory /private/pair --commit-sha FULL40CHARSHa --ack-stopped --ack-isolated
python3 scripts/stage1_recovery_bundle.py verify --mariadb-dump /private/pilot.sql --private-snapshot /private/gateway-snapshot --pair-directory /private/pair --ack-isolated
```

Manifest zawiera wyłącznie hashe, rozmiary, znacznik czasu i SHA; nie jest podpisany i sam nie dowodzi równoczesności kopii. Na świeżej, oddzielnej bazie odtwórz zrzut, sprawdź schemat v11, rekordy i uprawnienia, odtwórz prywatne pliki **do nowego katalogu**, uruchom pilotażowe usługi i sprawdź hasło + TOTP/passkey, odwołanie sesji/uprawnień i ciąg audytu. Zasymuluj awarię, zmierz RPO/RTO oraz wycofanie wersji. Nie importuj zrzutu z `CREATE/DROP DATABASE` do produkcji i nie nadpisuj aktywnego katalogu. Drugi operator musi zweryfikować wyniki.

## Bramka etapu — do potwierdzenia po CI nowego commita

| Wymaganie | Stan bez autoryzowanych dowodów |
| --- | --- |
| Pełne najnowsze GitHub Actions na Ubuntu 22.04/24.04/26.04 | **PENDING** |
| Prawdziwy systemd na trzech runnerach Ubuntu | **PENDING** |
| Agregaty wydajności ADC/ADCS i WebAdmin/API na Ubuntu | **PENDING** |
| Wspólne odtworzenie MariaDB + plików, restart, rollback i syntetyczne RPO/RTO | **PENDING** |
| Debian 12/13, Xeon, niezależna akceptacja | **NIEWYMAGANE według Issue #234 — nie testowano** |

Nie zaznaczaj jednostki 1 jako wykonanej do uzyskania zielonego CI dla ostatniego SHA. Nie rozpoczynaj etapu II na to samo polecenie, nie scalaj, nie zamykaj Issue #234 i nie wdrażaj automatycznie.

## Fix Pass 1 — ponowny przegląd etapu I (24.11.2026)

Jednostka pracy 5 objęła ponowną kontrolę raportowania CI, agregatów czasów testów na Ubuntu, walidatora progów pomiarowych oraz odtwarzania MariaDB z pięcioma prywatnymi plikami. Usunięto trzy konkretne usterki:

1. Skrócony raport JUnit błędnie oznaczał zestaw z **pominiętymi testami** jako `passed`. Teraz taki zestaw ma stan `failed`, a polecenie zwraca kod błędu również przy `not-run`. Nadal tworzy zanonimizowany raport i odrzuca zbyt duże pliki XML.
2. Raport czasu testów CTest/Chromium również traktował pominięte testy jako sukces i nie propagował błędnego/brakującego zestawu do kodu wyjścia. Teraz działa w trybie fail-closed, bez kopiowania nazw przypadków testowych ani treści błędów.
3. Walidator czterech profili mógł zaakceptować **zawyżoną przepustowość**, niezgodną z liczbą udanych żądań i czasem próby, a także zerowy czas lub same nieudane żądania. Dodano kontrolę spójności z tolerancją zaokrąglenia sondy do trzech miejsc po przecinku. Narzędzie pozostaje historycznym walidatorem offline; nie oznacza odbioru infrastruktury ani zgody na wdrożenie.

Nowe testy regresyjne obejmują pomijanie testów JUnit, kody wyjścia, sfałszowane wskaźniki oraz błędne okna pomiarów. Ponadto wykonywana jest ponowna walidacja integralności pary kopii oraz odtwarzania MariaDB v11 i stanu gatewaya na jednorazowych hostowanych Ubuntu. Dopiero pełne zielone CI bieżącego SHA pozwala zamknąć punkt 5 w Issue #234. Nie używano danych rzeczywistych ani usług produkcyjnych.
