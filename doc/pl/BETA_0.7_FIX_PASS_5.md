# Beta 0.7 — Fix Pass 5, Etap V (0.7.17–0.7.20)

**24.11.2026 — przekazanie poprawek, bez akceptacji.** [Issue #238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), jeden roboczy [PR #239](https://github.com/adrianf87/dc24h.eu-high-starter/pull/239), gałąź `beta-0.7.x-stage-full`. Dokładny SHA i linki do CI są zapisywane w komentarzu do zadania/PR po wypchnięciu zmian. **NO-GO** dla merge, wydania, wdrożenia i aktywacji operatorów.

## Przegląd i naprawy

1. **0.7.17 — benchmarki:** wcześniejsza bramka ufała liczbom p50/p95/p99 bez możliwości ponownego wyliczenia. Pomiary ADC, ADCS i prywatnego HTTPS przekazują teraz tylko posortowane czasy poprawnych żądań (bez danych klientów), a walidator wylicza percentyle od nowa, wymaga identycznych rozmiarów trzech okien i sprawdza pomiar CPU oddzielnie dla C++/Node/Apache. Testy negatywne odrzucają sfałszowane percentyle, brakujące próbki i sprzeczne CPU.
2. **0.7.18 — przeciążenie/drain:** istniejące realne testy sprawdzają półotwarte TLS, powolnych klientów, zaległy bufor, reconnect i SIGTERM. Nie stwierdzono nowej awarii w samym kodzie reaktora podczas przeglądu statycznego. Końcowa bramka wymaga teraz kompletnych nazw przypadków CTest i każdego czasu w granicy 240 sekund, nie tylko ogólnej flagi `passed`.
3. **0.7.19 — supply chain:** kontrola akcji GitHub pomijała `.yaml`, obejmowała tylko `.yml`; rozszerzono ją o obie postaci. Dodano weryfikację zgodności głównej wersji `package.json`/`package-lock.json`, sumy SHA-256 pliku lock i struktury inwentaryzacji npm.
4. **0.7.20 — kwalifikacja:** raport z pustymi profilami pomiarów i fikcyjną listą awarii mógł wcześniej przejść walidację pól. Bramka wymaga teraz konkretnych liczb, limitów, liczby próbek, zerowych błędów i spójnego `0.7.20`. Dołączono testy odmowy dla takich fałszywie zielonych raportów.

## Procedura dalsza

- Uruchamiać pełną macierz **wyłącznie na GitHub-hosted Ubuntu 22.04/24.04/26.04** z końcowego SHA, na odizolowanych sztucznych danych. Testy lokalne walidatorów nie dowodzą wyniku testów usługi, WebAuthn, ASan/UBSan, CodeQL, systemd, rollback `.deb` i odtworzenia SQL + pięciu plików.
- Porównać rzeczywiste zadania, run ID, ich SHA, wyniki i pochodzenie artefaktów z GitHub Actions. Nie zatwierdzać wyniku `pending`, `skipped`, `failed`, innego SHA ani ręcznie sporządzonego zielonego manifestu.
- Wyniki ADC/ADCS/HTTPS dotyczą różnych kosztów uwierzytelniania, nie są porównywalną przepustowością ani deklaracją SLA dla Debian/Xeon. Bramka 240 sekund nie dowodzi pełnej odporności na DoS.
- Przy awarii któregokolwiek testu CI kolejna naprawa wymaga nowego polecenia Ownera. Nie wykonywać merge, publikacji pakietu ani wdrożenia.

**Zasada Ownera:** po commicie/push i uruchomieniu CI zakończyć pracę od razu; nie czekać na wyniki. Status pozostaje **NO-GO** do osobnej akceptacji.
