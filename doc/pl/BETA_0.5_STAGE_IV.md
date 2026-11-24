# Beta 0.5.13–0.5.16 — Etap IV, testy bezpieczeństwa i kandydat do wydania

Jednostka 4 z [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234) na jednej gałęzi `beta-0.5.x-stage-full` w istniejącym draft PR #235. Kandydat jest **wyłącznie rozwojowy**: bez scalenia do `main`, wdrożenia, uruchomienia publicznego WebAdmin i zdalnych poleceń sterowania hubem. Nie ma dodatkowej bramki podpisu właściciela ani audytu zewnętrznego, ale cały końcowy CI musi być zielony. Cztery przeglądy/poprawki po etapach są **osobnymi późniejszymi zadaniami**.

## 0.5.13 — Bezpieczeństwo i przypadki atakujące

- Test realnego procesu C++ z ograniczonym zbiorem uszkodzonych i fragmentowanych ramek ADC, NUL, błędami UTF-8 i nadmierną długością; po atakach poprawny klient musi nadal działać. Zachowano testy błędnego TLS ADCS, shutdown/restart, limitów połączeń.
- Testy Node/Apache blokują kodowane, podwójnie kodowane, przechodzące do katalogu nadrzędnego i rozszerzane ścieżki. Żaden nieautoryzowany adres nie może trafić do prywatnego backendu. Playwright i testy WebAuthn kontrolują Origin/RP ID, obecność/weryfikację użytkownika, jednokrotne challenge, CSRF, własność i odwołanie klucza, utratę sesji i brak operacji sterujących hubem.
- C++ ASan/UBSan (Ubuntu 24.04), CodeQL, `npm audit` i testy prywatnych danych pozostają obowiązkowe. To **nie** jest pełny fuzzing pokryciowy ani pentest. Wcześniejszy błąd CodeQL z SHA-1/plikiem testowego hasła usunięto w `f2c7c4c`.

## 0.5.14 — Obciążenie i porównanie z Etapem I

Wyniki referencyjne: commit `96c3f873c2c94ea5e252b7db7e9b007b470330c2`, [CI #45283779224](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45283779224), 12 połączeń ADC/ADCS na Ubuntu 22.04/24.04/26.04, 4/s i 2 równoległe. Etap IV wykonuje po trzy okna po 32 połączenia każdego protokołu, 8/s i 4 równoległe; łącznie z krótkimi próbkami jest to 216 połączeń w oknie, poniżej limitu zabezpieczenia 256 przyjęć na minutę z jednego IP. Limitu **nie zwiększamy** na potrzeby testu. Zapisuje p50/p95/p99, przepustowość, błędy, CPU/RSS. Dopuszczalne granice regresji CI: p99 ≤ większa z (200 ms ADC/500 ms ADCS, 10× p99 Stage I), przepustowość ≥ 40% wartości referencyjnej, wzrost RSS ≤ 32 MiB, błędy = 0. To **nie** jest gwarancja wydajności produkcyjnej — obciążenia Stage I/IV się różnią, runner może mieć inne parametry.

Osobno 48 równoległych odczytów warstwy Apache HTTPS na trasach API i HTML WebAdmin wykorzystuje **fikcyjny backend API**. Stage I nie mierzył rzeczywistego p99 ani RPS HTTP, tylko czasy uruchamiania testów; dlatego nie wolno przeliczać tych czasów na opóźnienia żądania. Testowe MariaDB wykonuje 40 odczytów SQL przez socket Unix (z czasem startu klienta CLI); brak identycznego bazowego pomiaru SQL ze Stage I. Bez badań na maszynie docelowej i bez twierdzeń o pojemności produkcyjnej.

## 0.5.15 — Awaria, przywrócenie, rollback

CI tworzy tylko prywatne tymczasowe katalogi i sockety MariaDB, stan syntetyczny schematu v11 i pięć plików bramy (`mfa.json`, `passkeys.json`, `rate-limits.json`, `replay.jsonl`, `audit.jsonl`). Przy zatrzymanych zapisach powstaje para SQL dump/stan prywatny z manifestem SHA-256 i SHA wersji. Test celowo zatrzymuje bazę, uszkadza **wyłącznie testową kopię** pliku pakietu binarnego i konfiguracji, potwierdza odmowę dostępu i przywraca identyczne bajty. Do **nowej** bazy importuje dump i przywraca prywatne pliki do pustego katalogu, następnie weryfikuje granty, unieważnienia passkey, replay, audyt i restart. Rejestruje zmierzony czas RTO i syntetyczne RPO=0 utraconych potwierdzonych zapisów — po checkpointcie nie dopuszczono dalszych zmian. Osobny wcześniejszy test odtwarza także zmieniony grant.

**Granice:** nie przetestowano rzeczywistego cofnięcia pakietów `apt/dpkg` do wcześniejszej wersji, logowania prawdziwej sesji po awarii ani wdrożenia na serwerze produkcyjnym. Wyniki CI nie są SLA i nie dowodzą odtwarzania danych użytkowników.

## 0.5.16 — Rejestr ryzyk i decyzja techniczna CI

| ID | Ryzyko / obserwacja | Status |
| --- | --- | --- |
| S4-01 | Ręczny SHA-1 w teście bazy, chwilowo źle chroniony plik | Naprawiony w `f2c7c4c`; CodeQL Python i pełne CI do ponownej kontroli. |
| S4-02 | Nieprawidłowe ramki i obejście adresów proxy | Rozszerzone testy fail-closed C++/Node/Apache/Playwright, ASan/UBSan. |
| S4-03 | Krótka baza pomiarowa Stage I; brak porównywalnego HTTP i SQL | Ryzyko pozostaje, ograniczenia są jawne; żadnej deklaracji SLA ani przepustowości produkcyjnej. |
| S4-04 | Rollback tylko w izolacji i do tej samej wersji | Pozostaje ryzyko zgodności historii pakietów, konfiguracji i rzeczywistych sesji po aktualizacji. |
| S4-05 | Katalog passkey jako jeden prywatny plik; MariaDB v11 | Utrzymany model pojedynczej bramy; migracja/multi-writer i publiczne wystawienie nieobjęte pracą. |

**Kandydat do wydania oznacza jedynie gotowość kolejnego etapu prac**, kiedy najnowszy commit przejdzie GitHub-hosted Ubuntu 22.04/24.04/26.04: C++ Debug/Release, WebAdmin/Playwright, CodeQL, ASan/UBSan, systemd, Apache HTTPS, MariaDB, odbudowę i rollback. PR pozostaje draftem, issue otwarte, a kolejne cztery przeglądy kodu nie są jeszcze ukończone.

## Instrukcja operatora: bezpieczny test offline (wyłącznie dane fikcyjne)

1. Potwierdź izolowany katalog testowy, wersję SHA oraz to, że brama i MariaDB są **zatrzymane**. Nie rób częściowego snapshotu podczas zapisów i nie używaj danych produkcyjnych. Zabezpiecz pliki przed odczytem innych użytkowników.
2. Utwórz snapshot pięciu plików: `python3 scripts/stage4_private_recovery_drill.py snapshot --source KATALOG_BRAMA --destination NOWY_SNAPSHOT --ack-isolated --ack-stopped`. Następnie `verify --source NOWY_SNAPSHOT --ack-isolated`; przy pierwszym błędzie **przerwij**.
3. Utwórz parę: `python3 scripts/stage1_recovery_bundle.py pair --mariadb-dump DUMP_SQL --private-snapshot NOWY_SNAPSHOT --pair-directory NOWY_KATALOG_PARY --commit-sha SHA40 --ack-isolated --ack-stopped`. Weryfikuj `verify` z identycznymi ścieżkami oraz `--commit-sha SHA40 --ack-isolated`. Nie dopuszczaj rozbieżnych wersji lub sum plików.
4. Przywracaj bazę wyłącznie do **pustego datadir i socketu**; pliki bramy wyłącznie do **nowego katalogu** (`stage4_private_recovery_drill.py restore --source NOWY_SNAPSHOT --destination NOWY_KATALOG --ack-isolated --ack-stopped`). Przed podłączeniem klientów sprawdź schemat v11, hasła i granty kont, odwołane passkeys/epoki, historię TOTP, limity, integralność łańcucha audytu i replay. Nigdy nie kopiuj starej historii na działającą bramę.
5. W razie niepowodzenia zatrzymaj zapisy, nie nadpisuj już zmienionej bazy. Wybierz **drugi** pusty zestaw katalogów, skopiuj sprawdzoną kopię pakietu i konfiguracji, zaimportuj oryginalny dopasowany dump SQL i stan prywatny, sprawdź SHA, granty, audit i logowanie. Zmierz utracone potwierdzone zapisy (RPO) oraz czas do ponownego zweryfikowania działania (RTO); wynik nie jest automatycznie celem SLA.
6. Jeżeli manifest, uprawnienia, stan audytu, granty, hasła lub test sesji nie przejdą — **nie uruchamiaj publicznej usługi**. Zachowaj wyłącznie zanonimizowane dowody i ustal przyczynę. Rzeczywisty plan powrotu do starszej wersji binarnej wymaga dodatkowej zgodności schematu i pakietów, której ten test nie potwierdza.

Zobacz [Etap I](BETA_0.5_STAGE_I.md), [Etap II](BETA_0.5_STAGE_II.md), [Etap III](BETA_0.5_STAGE_III.md) i [EN/US Stage IV](../us/BETA_0.5_STAGE_IV.md).

## Fix Pass 4 — ponowny przegląd Etapu IV (2026-11-24)

Ponownie sprawdzono testy atakujące parser ADC i Apache/Node, bramkę
obciążeniową z referencją Stage I, próbę odzyskania MariaDB/pakietu/konfiguracji,
identyfikację wersji oraz ryzyka wydania. Naprawiono dwa potwierdzone błędy
dotyczące wiarygodności dowodów CI, bez zmian konfiguracji produkcyjnej:

- **S4-F1 — akceptowanie sprzecznych wyników obciążenia.** Bramka Stage IV
  pozwalała podać sztucznie zawyżony RPS niezależny od czasu i liczby żądań,
  ignorowała brak danych CPU oraz najniższe RSS przed późniejszymi oknami.
  Teraz sprawdza komplet metryk, percentyle, czas/RPS z tolerancją zaokrągleń,
  obserwacje CPU/RSS, wzrost RSS we wszystkich oknach i pochodzenie referencji.
  Dodano negatywne testy nieprawdziwych pomiarów oraz przekroczenia limitów.
- **S4-F2 — niepewna identyfikacja kodu w raportach.** Test rollbacku zastępował
  brakujące/błędne `SOURCE_SHA` ciągiem 40 zer, a raporty ADC/ADCS i Apache
  mogły zawierać pusty SHA. Teraz wymagają niezerowego, dokładnie zapisanego
  identyfikatora rewizji i odmawiają publikacji pozornie pełnego dowodu bez
  niego. Sprawdzenie formatu nie zastępuje podpisanej atestacji kompilacji.

W sprawdzonych testach atakujących proxy/parser i rollbacku syntetycznym nie
potwierdzono innych usterek. Nadal nie wykazano cofnięcia pakietów w `apt/dpkg`,
przywrócenia danych produkcyjnych ani logowania prawdziwej sesji po odtwarzaniu.
Fix Pass 4 można zaznaczyć dopiero po pełnym zielonym CI najnowszego SHA na
GitHub-hosted Ubuntu 22.04/24.04/26.04 (CodeQL, ASan/UBSan, systemd, Apache,
przeglądarka, endurance i odtwarzanie). PR #235 pozostaje draftem, a Issue #234
otwarte do osobnej bramki końcowego scalenia.
