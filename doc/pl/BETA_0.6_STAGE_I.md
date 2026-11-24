# beta-0.6.1–beta-0.6.16 — instrukcja operatora i bezpieczne odtwarzanie

Gałąź: `beta-0.6.x-stage-full` · [Issue #236](https://github.com/adrianf87/dc24h.eu-high-starter/issues/236) · [roboczy PR #237](https://github.com/adrianf87/dc24h.eu-high-starter/pull/237). **Bez wdrożenia produkcyjnego, bez automatycznego scalenia.** Etap I kończy się dopiero po zielonym CI dla najnowszego SHA. Etap II to osobne, późniejsze sprawdzenie i naprawa wszystkich 16 patchy.

## Przed jakąkolwiek operacją

1. Potwierdź, że to **izolowane, testowe środowisko** z fikcyjnymi kontami. Nie wykonuj testów awarii, limitów ani odtwarzania na prawdziwym hubie lub danych użytkowników. Do pomiarów służą tylko GitHub-hosted Ubuntu 22.04, 24.04 i 26.04. Brak testów na Debian/Xeon i brak twierdzeń o pojemności produkcyjnej.
2. Zatrzymaj wszystkich zapisujących: serwer C++, lokalną bramę Node i usługi mogące zmieniać MariaDB lub pięć plików bramy. Sprawdź brak aktywnych połączeń i procesów zapisujących. Nie próbuj internetowego przełączenia usług.
3. Stwórz kopię testowej MariaDB v11 i **komplet** plików `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`. Zachowaj spójny manifest i integralność, właściciela oraz prawa `0700` katalogów / `0600` plików. Odmów odtwarzania, jeśli brakuje choć jednego elementu, istnieje dowiązanie symboliczne lub zgadza się tylko część sum.

## Odtwarzanie i weryfikacja logowania

1. W osobnym, pustym katalogu odtwórz SQL i pięć plików z **tej samej zatrzymanej migawki**. Nie nadpisuj istniejących katalogów ani działających kont. Uruchom testową MariaDB i usługi C++/Node/Apache wyłącznie na loopback.
2. Potwierdź odczyt `/api/v1/status` z uruchomionego huba, **zatrzymaj hub**, a dopiero potem uruchom oddzielny `dc24h-account-admin` na prywatnym gnieździe API. Hub nie realizuje logowania administratora; test hasła + TOTP oraz 24 odczyty HTTPS wykonuj z uprawnioną usługą administracyjną i oczekuj `server.running=false` podczas konserwacji. Uruchamiaj **dwa równoczesne odczyty co 2200 ms** (średnio jeden co 1100 ms), aby podwójna weryfikacja grantu nie przekroczyła limitu 24 autoryzacji na 10 sekund; zmierzony RPS zawiera te odstępy i nie jest przepustowością maksymalną. Nie uruchamiaj obu usług równolegle na tej samej bazie. Sprawdź odmowę starej sesji/cookie, zużytego kodu TOTP, cofniętych uprawnień oraz unieważnionych kluczy passkey. Obecna próba automatyczna potwierdza odmowę opcji unieważnionego klucza, ale **nie jest pełnym testem przeglądarkowej ceremonii WebAuthn**; nie traktuj jej jak zakończonego testu logowania passkey po odtworzeniu.
3. Ćwiczenie historycznego rollbacku kompiluje oddzielnie przypięte źródła beta-0.5.16 (`5cd7b571fd974cbb7d59d8e4478185ff819e248a`) i uruchamia poprzednią binarkę na **nowej** testowej instancji MariaDB + pięciu plikach. To nie jest wycofanie paczki przez menedżer systemowy ani instrukcja downgrade produkcyjnej bazy.

## Rotacja tokenu i certyfikatu

- Rotacja tokenu API: najpierw zatrzymaj zapisy, odczekaj ponad 60 s od końca ostatniego wiadra limitera i sprawdź integralność `rate-limits.json`. Skrypt `scripts/beta06_gateway_token_rotation.py` wymaga `--ack-stopped --ack-isolated`, jawnych bezwzględnych ścieżek `--token`, `--candidate`, `--backup`, `--rate-limits` i akcji `prepare`, następnie `commit`; po restarcie testuj uwierzytelniony, prywatny odczyt. W razie błędu ponownie zatrzymaj usługi i użyj `rollback`, o ile kopia poprzedniego tokenu jest prawidłowa. **Nie** zapisuj sekretu, cookie, TOTP ani hasła do logów/PR/Issue.
- Certyfikat ADCS i prywatnego Apache: wygeneruj poprawny certyfikat i klucz do testowej nazwy hosta; sprawdź okres ważności, łańcuch, nazwę i zgodność klucza. Zachowaj poprzedni komplet poza katalogiem wdrożenia; wymień go wyłącznie w izolacji i wykonaj kontrolowany restart. Żądaj weryfikacji nowego certyfikatu i odmowy starego zaufania, nie pozwalaj na downgrade do HTTP ani publiczne nasłuchiwanie.
- **Utrata tajnego tokenu lub katalogu MFA/replay/audit/passkeys/limitera:** nie generuj automatycznie pustych plików i nie omijaj limitów. Utrzymaj usługę wyłączoną, odtwórz kompletną, poprawną kopię i ponownie uwierzytelnij administratora.

## Ekran incydentów i uprawnienia

WebAdmin udostępnia jedynie zweryfikowane liczniki zbiorcze, informację czy hub deklaruje pracę oraz wiek ostatniego **odebranego przez przeglądarkę** odczytu. Brak danych o czynnościach odtwarzania jest pokazany jako „niedostępne”, nie jako „OK”. Przy błędzie 401/403, przerwanej dostępności lub przeterminowanych danych nie wolno korzystać z wcześniejszych liczników. Przycisk „Potwierdź lokalnie” zmienia tylko stan ekranu; nie wysyła żadnej akcji do huba, nie przywraca usług i nie nadaje uprawnień. Logowanie nadal wymaga **hasło + TOTP** lub **hasło + passkey**; decyzje o grantach pozostają w C++.

## Kryterium NO-GO / GO dla prac programistycznych

- **NO-GO:** choćby jeden wymagany job GitHub CI niepowodzenie/pominięty/w toku, niezgodny SHA, niepełna dokumentacja, brak dowodu pełnej reautoryzacji albo uszkodzona migawka. Nie oznaczaj Etapu I jako ukończonego, nie zmieniaj statusu PR na gotowy, nie scalaj i nie wdrażaj.
- **Potencjalne GO wyłącznie do ukończenia Etapu I:** całe CI na **tym samym końcowym SHA** przechodzi na Ubuntu 22.04/24.04/26.04: C++ Debug/Release, React/Playwright, ASan/UBSan, CodeQL/audyt npm, faktyczny systemd, ADC/ADCS/TLS, uwierzytelniony HTTPS, MariaDB v11 + pięć plików, poprzednia binarka i testy awarii. Zapisz SHA, linki do workflow, numery próbek, p50/p95/p99, RPS, błędy, CPU/RSS i ograniczenia bez danych osobowych. Później, na osobne polecenie, wykonaj jeden pełny Etap II napraw. **Żaden z tych etapów nie uruchamia produkcji.**

Źródło techniczne i lista ryzyk: [EN/US Stage I](../us/BETA_0.6_STAGE_I.md).
