# Beta 0.4.13–0.4.16 — etap IV: przegląd kandydata do wydania

Zakres [Issue #223](https://github.com/adrianf87/dc24h.eu-high-starter/issues/223): jedna gałąź `beta-0.4.13-0.4.16-stage4`, cztery kolejne patche i jeden PR do `main`. Kandydat **nie jest zgodą na wdrożenie**. Nie scalać automatycznie, nie włączać publicznego WebAdmin i nie zmieniać produkcyjnego serwera.

| Patch | Zmiana | Granica weryfikacji |
| --- | --- | --- |
| beta-0.4.13 | Negatywne testy WebAuthn, własności klucza, replay, CSRF, Origin oraz uprawnień. | Automatyczne testy nie zastępują niezależnego audytu. |
| beta-0.4.14 | Ograniczony, ręcznie uruchamiany pomiar ADC/ADCS, WebAdmin HTTPS i prywatnego API. | [Procedura EN/US](../us/BETA_0.4_STAGE_IV_PERFORMANCE.md). Na Debianie 13: **NOT MEASURED**. |
| beta-0.4.15 | Bezpieczna kopia, kontrola integralności i odtworzenie prywatnych plików do nowego katalogu. | [Procedura EN/US](../us/BETA_0.4_STAGE_IV_RECOVERY.md). MariaDB, awarie i RPO/RTO na serwerze docelowym: **NOT MEASURED**. |
| beta-0.4.16 | Spójne wersje, kontrole wydania, dokumentacja, lista zagrożeń i ręczna decyzja. | Niezależny przegląd: **PENDING**. |

Nie zmieniono C++20/ADC/ADCS, schematu MariaDB **v11**, publicznego Apache ani granicy `127.0.0.1` prywatnej bramy. WebAdmin to React + TypeScript + Vite; logowanie passkey nadal wymaga hasła. Bez nowej migracji SQL i bez zdalnych poleceń zarządzania hubem.

## Kolejność akceptacji

1. Zanotuj SHA PR. **Czekaj na zakończenie wszystkich zadań CI dla aktualnego SHA**: C++ Debug/Release Ubuntu 22.04, 24.04, 26.04, WebAdmin, sanitizery oraz wymagany test zbiorczy. Uwzględnij CodeQL. Po poprawce błędu licz wyniki od nowego SHA.
2. Sprawdź testy negatywne uwierzytelniania, uprawnień, sesji, HTTPS i integralności audytu. Zapisz linki do wyników bez danych wrażliwych.
3. Zleć niezależny przegląd bezpieczeństwa. Odnotuj uwagi i ich usunięcie.
4. Na **odizolowanym serwerze testowym** Debian 13 (Intel Xeon D-1521, 8 × 2,7 GHz, RAM 31 997 MiB) z osobną bazą testową wykonaj pomiary wydajności oraz próbę awarii MariaDB, restartów Apache2/bramy/huba, odtworzenia i wycofania aktualizacji. Kontroluj sumy, logi i integralność bazy; zanotuj RPO i RTO. Przed dotknięciem produkcji wymagana jest osobna zgoda.
5. Drugi operator i właściciel projektu muszą zaakceptować dowody oraz ryzyka. Zielone CI samo w sobie **nie** uprawnia do scalenia ani publicznego wdrożenia.

## Rejestr decyzji

- Pełne CI najnowszego SHA: **PENDING**
- Audyt niezależny: **PENDING**
- Debian 12/13 — zgodność i procedury operacyjne: **PENDING**
- Wydajność CPU/RAM, opóźnienia, błędy, MariaDB: **NOT MEASURED**
- Odtwarzanie MariaDB, spójna kopia prywatna, RPO/RTO i rollback: **NOT MEASURED**
- Decyzja: **NO-GO** do chwili dostarczenia dowodów i ręcznego zatwierdzenia.

Dokumentacja techniczna i lista ryzyk: [EN/US](../us/BETA_0.4_STAGE_IV.md).
