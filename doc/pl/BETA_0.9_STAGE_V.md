# Beta 0.9.17–0.9.20 — Etap V: dowody i procedura Ownera

**Status: NO-GO.** Zmiany znajdują się na `beta-0.9.x-stage-full` w jednym roboczym [PR #243](https://github.com/adrianf87/dc24h.eu-high-starter/pull/243) dla [Issue #242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242). Kod i testy syntetyczne nie są akceptacją wydania. Nie scalać do chronionego `main`, nie tagować, nie publikować paczek, nie włączać operatorów ani nie wdrażać produkcyjnie bez osobnej zgody Właściciela.

Osobne poprawki na gałęzi `beta-0.9.x-stage-full-fix-pass`, w tym przegląd CI, wydajności, zależności i bramek RC, opisuje [Fix Pass CI/RC](BETA_0.9_CI_RC_FIX_PASS.md).

## Zakres

1. **0.9.17 — rzeczywiste artefakty ZIP:** skrypt `beta09_artifact_evidence.py` czyta z API GitHuba numer próby, aktualny SHA gałęzi/PR, zlecenia GitHub-hosted oraz metadane artefaktów. **Pobiera bajty ZIP** bez przekazania tokenu do serwera plików i porównuje ich SHA-256 z digestem GitHuba. Bez rozpakowania na dysk sprawdza dozwolone nazwy JSON, strukturę, SHA źródła, limity rozmiaru, symlinki, zip-slip, duplikaty i podejrzane pola prywatne. Stare raporty bez własnego SHA są jawnie liczone, nie certyfikowane osobno. Sprawdzanie `main` dopiero po osobno zatwierdzonym merge i nowych workflow.
2. **0.9.18 — Ubuntu i pakiety:** audyt zapisuje SHA-256 pełnej, ograniczonej listy zainstalowanych publicznych pakietów APT oraz wersje ośmiu krytycznych pakietów, GCC/Node/npm/CMake, sumę npm lock i przypięte Action SHA. Obowiązuje audyt npm produkcyjny **i dev**, CodeQL oraz odmowa przy high/critical. **To nie jest niezmienny snapshot repozytoriów APT ani pełny SBOM systemu.**
3. **0.9.19 — granice testowe:** TSAN rozszerzony do 8 zestawów, ASan/UBSan obejmuje więcej przypadków parsera/routingu/tokenów, dwa niezależne fuzzery Clang/ASan po minimum 1024 przebiegi każdy. Na trzech obrazach GitHub-hosted Ubuntu 22.04/24.04/26.04 powtarzane są trzy osobne okna ADC/ADCS/prywatnego HTTPS i testy powolnych odbiorców, TLS, drenażu/restartu. Odmowa przy brakujących, pominiętych, błędnych lub nieograniczonych wynikach. **Nie są to pomiary SLA ani wydajności produkcyjnego Debiana/Xeona.**
4. **0.9.20 — kandydat:** wersje C++/Node/React/npm/aktywnych testów wskazują `beta-0.9.20`; nowa bramka sprawdza raporty każdego Ubuntu i TSAN/fuzz, ale zawsze pozostaje `NO-GO` bez osobnej zgody Ownera. Historyczne testy poprzednich wersji zachowują swoje oryginalne oczekiwania.

## Bezpieczne przekazanie / przyszły rollback

1. Po push/uruchomieniu CI **zakończyć pracę bez czekania i bez odpytywania wyników**. W PR #243 zapisać finalny SHA. `pending`, `skipped`, `failed` i niezgodny SHA to blokada.
2. Dopiero później, osobnym poleceniem Właściciela, odczytać z API GitHuba kompletne workflow z bieżącego SHA i potwierdzić ZIP, raporty liczbowe oraz narzędzia. Nie dodawać tokenu ani prywatnych danych do Git.
3. Rollback wymaga zatrzymania **wszystkich** procesów zapisujących; zachować zgodną parę SQL MariaDB v11 i pięć plików prywatnych (`mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`) z pełnym łańcuchem audytu. Sprawdzić manifest, HMAC/ledger jeśli zatwierdzone oraz prawa własności w **nowym pustym katalogu**. Nie nadpisywać oryginałów, nie resetować licznika MFA ani audytu.
4. Przed jakąkolwiek zmianą produkcyjną sprawdzić role/granty C++, wycofane konta po kontrolowanym restarcie, uwierzytelnianie TOTP/passkey, niezgodność starszej binarki i utrzymanie dziennika. Tymczasowy pakiet `.deb` i lokalny serwis nie dowodzą RPO/RTO ani gotowości produkcyjnej.

## Ryzyka wymagające decyzji Ownera

- Brak natychmiastowego live-refresh kont C++ bez zatwierdzonej architektury; restart nadal wyznacza granicę unieważnienia sesji.
- Brak zatwierdzonej trwałej autentyczności danych prywatnych po cold-start i migracji pięciu plików; eksperymentalny HMAC nie jest automatyczną ochroną produkcyjną.
- Brak fizycznego sprawdzenia DC++/AirDC++ i zsynchronizowanych kluczy WebAuthn; nie raportować ich jako certyfikowanych.
- Archiwizacja 8 MiB audytu i awaria ENOSPC blokują uwierzytelnianie; nie wolno usuwać historii dla przywrócenia zielonego statusu.
- Brak gwarancji atomowego backupu, produkcyjnego RPO/RTO, odporności na wszystkie ataki, stałości APT ani SLA.
- **Fix Passy 1–5 i niezależna walidacja GitHub-hosted Ubuntu na jednym końcowym SHA są nadal konieczne.** Wszystkie operacje merge, tagowania, wydania, migracji i wdrożenia wymagają osobnego polecenia Właściciela.
