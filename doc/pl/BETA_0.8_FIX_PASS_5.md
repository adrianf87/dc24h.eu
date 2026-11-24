# Beta 0.8 — Fix Pass 5 / Etap V (0.8.17–0.8.20)

**Status: NO-GO / CI jeszcze niepotwierdzone.** Jedna gałąź `beta-0.8.x-stage-full`, jeden roboczy [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241), [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240). Dokładny SHA po zapisie znajduje się w komentarzu przekazania PR. Bez zgody Ownera nie wykonuje się merge do `main`, tagu, wydania, migracji ani wdrożenia.

## Wykryte i naprawione w kodzie

1. **Zielone CI ze starego HEAD:** weryfikator odczytuje aktualny SHA gałęzi na początku i na końcu; odrzuca wcześniejsze, choć poprawne wyniki oraz zmianę gałęzi w trakcie kontroli.
2. **Pomylenie artefaktów z innej próby:** wymagana jest tożsamość gałęzi i SHA, unikalny identyfikator artefaktu, suma GitHub SHA-256 i czas utworzenia w przedziale bieżącej próby zadań. Negatywne przypadki są w testach.
3. **Pominięty audyt devDependencies:** skrypt sam uruchamia npm z `--include=dev --include=prod`; nie przyjmuje dostarczonego raportu udającego pełny audyt. Sprawdza obecność obu rodzajów pakietów w lockfile, ogranicza prywatne dane audytu i nie eksportuje surowych poradników podatności.
4. **Niepełna wersja kandydata:** aktywny test dziennika incydentów wskazuje teraz `beta-0.8.20`; osobna bramka wychwyci powrót do `beta-0.7.20` i brak dowodu pełnego audytu.
5. **Zasoby fuzzingu:** wejściowe ziarna są kopiowane do katalogu tymczasowego; odrzucane są dowiązania i nadmierne pliki, limit diagnostyki wynosi 1 MiB, a surowe dane awarii nie pozostają w repo. Test z nadmiernym wyjściem kończy się odmową.

## Granice i ryzyka Ownera

- API GitHub potwierdza metadane SHA-256, ale skrypt **nie pobiera archiwum ZIP i nie haszuje jego zawartości**. Wymagana jest osobna kontrola oczyszczonych raportów na końcowym SHA. Próba ingerencji po ostatnim odczycie gałęzi jest ryzykiem czasu kontroli.
- Wewnętrznie spójne fałszywe próbki wydajności nadal wymagają zaufania do faktycznego procesu pomiarowego w CI. Nie obiecywać wydajności, odporności DoS ani SLA dla produkcyjnego Debian/Xeon.
- TSAN/libFuzzer nie są zaliczone przez samo dodanie konfiguracji. Nieudane, niedostępne, pominięte i niezgodne środowiskowo testy oznaczają **blokadę**, nie sukces. Pozostałe obowiązkowe zadania na GitHub-hosted Ubuntu 22.04/24.04/26.04 również muszą zakończyć się pozytywnie na identycznym SHA.
- Zewnętrzne natywne DC++/AirDC++, prawdziwe/synchronizowane klucze WebAuthn, natychmiastowe wycofanie kont bez restartu, żywa atomowa kopia MariaDB v11 + pięciu prywatnych plików, limit audytu 8 MiB, pełny APT SBOM i produkcyjne RPO/RTO pozostają otwarte.

**Instrukcja:** po push/uruchomieniu CI zakończyć tę jednostkę pracy **bez czekania i bez odpytywania wyniku**. Zostawić PR #241 jako draft i osobno przeprowadzić akceptację Ownera dopiero po kompletnej walidacji finalnego HEAD. Nie zmieniać środowiska produkcyjnego ani oryginalnych prywatnych zasobów.
