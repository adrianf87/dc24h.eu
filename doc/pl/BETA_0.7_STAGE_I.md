# Beta 0.7.x — Etap I: procedura operatora (w trakcie)

Zadanie [#238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), gałąź `beta-0.7.x-stage-full`. Wdrożenie, zamknięcie zadania i scalenie z `main` są zabronione przed ukończeniem wszystkich 20 patchy, pełnym pozytywnym CI i osobną zgodą właściciela na merge.

## Zasady pracy i odtwarzania

1. Testuj wyłącznie na jednorazowych maszynach **GitHub-hosted Ubuntu 22.04, 24.04 i 26.04** z syntetycznymi kontami. Nie wykorzystuj produkcji, Debiana, prywatnych runnerów ani Xeona D-1521.
2. Przed kopią i przywracaniem **zatrzymaj wszystkie procesy zapisujące** do MariaDB i prywatnej bramy. Potwierdź bezczynność, wersję schematu SQL v11 i komplet pięciu plików `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`. Odrzuć częściowe, zmienione lub niezgodne generacje.
3. Przeprowadź wyłącznie dry-run przywracania do **nowego, pustego katalogu** o uprawnieniach właściciela. Nie nadpisuj działającej bazy lub prywatnego stanu. Zapisz tylko oczyszczone wyniki weryfikacji i skróty SHA-256, nigdy treść sekretów.
4. Przełączenie pakietu przez `dpkg` oraz rollback wymagają osobnego testu na izolowanej maszynie. Przy błędzie przerwij, zatrzymaj usługę, odtwórz pasujący zestaw SQL + pięć plików i potwierdź autoryzację przez C++ przed ponownym startem.
5. Utrzymaj logowanie **hasło + TOTP albo hasło + passkey**. Nie ujawniaj tokenów, kodów, identyfikatorów kont/IP, surowego audytu ani stanu prywatnego w PR, issue, logach i artefaktach. Brak dowodu oznacza *niezaliczone*, nie „prawdopodobnie działa”.

## Aktualna realizacja

Wprowadzono kontrolę wersji i integralności pakietu `ncdc` oraz agregację rzeczywistych testów ADC/ADCS (patch `0.7.1`); wymagane są jeszcze wyniki GitHub CI. Dla DC++ i AirDC++ status pozostaje *nietestowane*. Patche `0.7.2–0.7.20` i końcowa walidacja całego Etapu I są nadal **niezakończone**. Szczegóły i lista dowodów: [EN/US Stage I](../us/BETA_0.7_STAGE_I.md).
