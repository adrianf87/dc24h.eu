# Beta 0.8.9–0.8.12 — Etap III: spójna kopia, autentyczność i próbny rollback

Dotyczy wyłącznie Etapu III z [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), tej samej gałęzi `beta-0.8.x-stage-full` i szkicu PR #241. **Nie jest to procedura wdrożenia produkcyjnego ani gwarancja RPO/RTO.** MariaDB pozostaje w schemacie v11, a C++ zachowuje wyłączną władzę nad hasłami, kontami i uprawnieniami.

## 0.8.9 — kopia sześciu elementów bez symlinków

1. Zatrzymaj wszystkie procesy zapisujące do C++/MariaDB i Node gateway. Blokada SQL nie zatrzymuje zapisów do pięciu prywatnych plików.
2. Utrzymaj blokadę odczytu MariaDB przez eksport SQL, kopię kompletu `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json` i powiązanie manifestów z dokładnym SHA. Żadnych kopii z aktywnego systemu ani do istniejącego celu.
3. Node sprawdza teraz **wszystkie składniki ścieżki** prywatnego magazynu, nie tylko ostatni katalog. Dowiązanie symboliczne w katalogu pośrednim, jego zamiana po uruchomieniu lub niedozwolone uprawnienia powodują odmowę. Nadal wymagane jest fizyczne odizolowanie zapisu; kontrole użytkowe nie zastępują atomowego `openat2`.

## 0.8.10 — dodatkowy podpis HMAC i ochrona przed ponownym użyciem

Same sumy SHA-256 dowodzą spójności, **nie tożsamości autora**. Nowy opcjonalny `scripts/beta08_recovery_auth.py` przyjmuje oddzielny 256-bitowy klucz HMAC i niezależnie potwierdzony identyfikator klucza. Tworzy plik `recovery-auth.json`, sprawdza manifest i prowadzi osobny rejestr już zatwierdzonych kopii. Klucz i rejestr trzymaj **poza backupem, repozytorium, artefaktami CI i katalogiem przywracanym**. Nie używaj klucza TOTP, tokenu API ani hasła jako klucza HMAC.

Kolejność operatora na odizolowanym hoście:

1. Utwórz raz pusty rejestr przez `init-ledger --ledger /secure/trust/accepted-pairs --ack-isolated --ack-stopped`. Nie inicjalizuj go ponownie po awarii; odtwórz niezależny, prawdziwy rejestr.
2. Po sparowaniu SQL i pięciu plików użyj `sign --pair-directory /secure/offline/pair --auth-directory /secure/auth --trusted-key /secure/trust/key --ack-isolated --ack-stopped`.
3. Wykonaj niezmieniający danych `verify` z `--mariadb-dump`, `--private-snapshot`, `--pair-directory`, `--auth-directory`, `--trusted-key`, `--expected-key-id`, `--commit-sha`, `--ledger` oraz `--ack-isolated`.
4. Dopiero po zgodzie OWNER i zatrzymaniu zapisów zastosuj `accept` z tymi samymi argumentami oraz `--ack-stopped`. Rejestr zostanie zapisany i zsynchronizowany; **ta sama kopia nie może być ponownie zatwierdzona**.

HMAC jest współdzielonym kluczem symetrycznym, a nie niezaprzeczalnym podpisem konkretnego operatora. Rozwiązanie z osobnym kluczem publicznym wymagałoby odrębnie zatwierdzonego zarządzania kluczami i ich unieważnianiem.

## 0.8.11 — pakiet i uruchamianie gatewaya bez `tsx`

- Testowy pakiet `.deb` ma dodatkową kontrolowaną awarię `postinst` **po rozpakowaniu**. GitHub-hosted Ubuntu musi pokazać rzeczywisty nieukończony stan `dpkg`, a potem przywrócić wcześniejszy pakiet z właściwą binarką. Nie włączamy usługi ani nie publikujemy pakietu produkcyjnego.
- `npm run build:remote:ci` buduje `dist/remote-main.mjs`. Próbne środowisko CI kopiuje skompilowany plik, wykonuje `npm ci --omit=dev` i sprawdza rozruch oraz restart lokalnego gatewaya na fikcyjnym gnieździe Unix i pięciu syntetycznych plikach. W instalacji nie powinno być `tsx`, TypeScript ani esbuild. Przykładowa jednostka systemd wymaga osobnego przeglądu przed prawdziwą instalacją.

## 0.8.12 — sprawdzenie po cofnięciu pakietu

Po rzeczywistym downgrade `dpkg` wykonaj odtworzenie do **nowej**, odizolowanej bazy i katalogu. Sprawdź C++ v11, status zablokowanego konta po restarcie, TOTP i replay, odwołany passkey, audyt, limity prób i zakaz ponownego przyjęcia użytego manifestu. Nie przywracaj starej sesji przeglądarki ani uprawnień ze snapshotu sprzed cofnięcia. Testy przeglądarkowego logowania do poprzedniej binarki i natychmiastowego odwołania aktywnej sesji nie są tu dowiedzione i wymagają osobnego zatwierdzonego scenariusza.

## Zasada zakończenia

CI wyłącznie **GitHub-hosted Ubuntu 22.04/24.04/26.04** na jednym dokładnym SHA, w tym C++, WebAdmin/Chromium, bezpieczeństwo, CodeQL, MariaDB, pakiety i rollback. Po commit/push i wystartowaniu CI **natychmiast zakończ działanie bez oczekiwania na wyniki**. `CI pending` nie oznacza akceptacji. Bez zmiany `main`, wdrożenia, tagu, publikacji pakietu i Etapu IV.
