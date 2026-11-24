# Beta 0.8.17–0.8.20 — Etap V: dowody CI i instrukcja Ownera

**Status: NO-GO.** Wspólna gałąź `beta-0.8.x-stage-full`, roboczy [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241), [zadanie #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240). Zakończenie kodowania nie jest akceptacją bezpieczeństwa, wydania, produkcji ani merge. Osobno pozostaje pięć Fix Passów i decyzja Właściciela.

## Etap V — zakres i ograniczenia

1. **0.8.17 — autentyczne CI:** nowy `beta08_github_evidence.py` pobiera wyniki bezpośrednio z API GitHub przy użyciu tokenu tylko do odczytu. Wymaga jednego końcowego SHA, poprawnej próby wykonania, kompletnej macierzy zadań i zgodnych sum SHA-256 artefaktów. Odrzuca wcześniejsze zielone wyniki, gdy najnowszy przebieg jest nieudany/oczekujący. Testy syntetyczne nie dowodzą zakończenia faktycznych workflow.
2. **0.8.18 — benchmarki:** trzy okna na obraz, ADC/ADCS 32 żądania przy współbieżności 4; HTTPS hasło+TOTP 8 przy współbieżności 2. Ponownie wyliczane są p50/p95/p99/RPS z liczbowych próbek, a SHA źródła, kompilator/Node, wersja Ubuntu i lockfile są związane skrótami. Limity p99: ADC 500 ms, ADCS 1000 ms, HTTPS 2000 ms. To **nie są porównywalne** wydajnościowo protokoły ani produkcyjne SLA.
3. **0.8.19 — testy negatywne:** na GitHub-hosted Ubuntu 24.04 dodano TSAN dla parsera/sieci/zabezpieczeń oraz Clang/libFuzzer parsera ADC (seed 424242, 1024 przebiegi, maks. 4096 bajtów, 5 s na wejście, 512 MiB RSS, 180 s łącznie). Niepowodzenie, brak kompilatora, problem środowiska TSAN lub pominięty przypadek to **brak zaliczenia**, nie wynik zielony. Dotychczasowe ASan/UBSan, TLS half-open, slow-reader, SIGTERM/drain i recovery nadal obowiązują.
4. **0.8.20 — kandydat:** C++/Node/WebAdmin/npm wskazują `beta-0.8.20`; dochodzi audyt **wszystkich** zależności npm — również TypeScript/Vite/testowych — oraz weryfikacja 26 raportów z trzech Ubuntu i TSAN/fuzz. Nie tworzymy wyjątków dla nieprzejrzanych podatności high/critical. Bramka offline wciąż wymaga późniejszej autentykacji GitHub API i zgody Ownera.

## Bezpieczna procedura operatora po zakończeniu CI

1. Sprawdź, że wszystkie workflow na **GitHub-hosted Ubuntu 22.04/24.04/26.04** dotyczą tego samego finalnego SHA. Nie zastępuj ich Debianem, prywatnym runnerem/Xeon ani fikcyjnym manifestem.
2. W wydzielonym katalogu, przy użyciu tokenu GitHub **tylko do odczytu Actions**, uruchom `beta08_github_evidence.py` z `--root`, `--sha` i `--output`. Skrypt czyta API i nie zapisuje tokenu. Nie dodawaj tokenów ani raportów zawierających wrażliwe dane do Git.
3. Potwierdź `completed/success` każdego wymaganego zadania, numer ostatniej próby, nazwę systemu Ubuntu oraz hash artefaktu. `pending`, `skipped`, `failed`, niezgodny SHA, brak digestu lub niemożliwy do uruchomienia TSAN/fuzz blokuje akceptację.
4. Pobierz wyłącznie oczyszczone artefakty i zestaw raportów dla wszystkich trzech Ubuntu. Zweryfikuj lokalnie `beta08_release_candidate_gate.py`, a także źródła i sumy npm, pomiary, czasy faultów, pakiet `.deb`, MariaDB v11 + pięć prywatnych plików oraz przebieg CodeQL/ASan/UBSan.
5. Przy przyszłym, osobno zatwierdzonym rollbacku: zatrzymaj wszystkich autorów zapisu, zachowaj sparowany SQL v11 i pięć plików (`mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`) z manifestem i sumami. Zweryfikuj preflight w **nowym** katalogu, nie nadpisuj istniejącego stanu, nie publikuj sekretów. Najpierw sprawdź odtwarzanie i autoryzację na odizolowanym hostowanym Ubuntu.

## Ryzyka do decyzji Ownera

- **C++ / konta:** wycofanie konta wymaga kontrolowanego restartu huba; brak gwarancji natychmiastowego unieważnienia sesji bez restartu.
- **Passkeys:** wirtualny Chromium nie certyfikuje prawdziwych kluczy i synchronizacji urządzeń.
- **Audyt:** limit 8 MiB i archiwizacja przy zatrzymanych zapisach muszą zachować łańcuch; brak audytu oznacza odmowę uwierzytelnienia, nie automatyczny reset.
- **Pakiet i recovery:** testowy `.deb`, lokalny runtime Node i offline SQL + pięć plików nie są sprawdzonym produkcyjnym rollbackiem ani gwarancją RPO/RTO.
- **Wydajność i bezpieczeństwo:** krótkie testy na Ubuntu nie dowodzą odporności na wszystkie ataki ani wydajności produkcyjnego Debian 13/Xeon. Nie ma zgody na publiczny WebAdmin, nowe uprawnienia ani wdrożenie.

**Instrukcja Ownera:** po zapisie commita i uruchomieniu CI **zakończyć działanie natychmiast, bez czekania na wyniki**. Etap V pozostaje `CI pending / NO-GO`, dopóki osobny przegląd nie potwierdzi pełnego finalnego SHA i wszystkich Fix Passów. Nie wykonywać merge, tagu ani wdrożenia.

**Uzupełnienie Fix Pass 5:** kontrola odrzuca stare zielone wyniki po zmianie HEAD, sprawdza próbę i czas pochodzenia artefaktów, uruchamia audyt npm z jawnym zakresem produkcyjnym i deweloperskim oraz ogranicza logi/ziarna fuzzingu. **Suma SHA-256 artefaktu pochodzi z metadanych API GitHub; sam ZIP nie jest tu pobierany i ponownie haszowany.** Osobny przegląd zawartości jest wymagany. [Pełny opis napraw i ryzyk](BETA_0.8_FIX_PASS_5.md).
