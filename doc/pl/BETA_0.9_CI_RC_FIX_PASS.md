# Beta-0.9.20 — Fix Pass: CI, wydajność, zależności, przygotowanie RC

**Stan: NO-GO.** Gałąź robocza `beta-0.9.x-stage-full-fix-pass` nie jest wydaniem RC. Sam commit, zielony CI ani poprawny raport offline nie stanowią zgody Ownera na merge, tag, wydanie, migrację stanu lub wdrożenie.

## Zamknięte kontrole w kodzie

1. **CI / dowody dokładnego HEAD:** `beta09_artifact_evidence.py --mode fix-pass` sprawdza najnowszą próbę zdarzenia `push` na tej gałęzi bez przypisywania jej cudzego PR. GitHub API musi potwierdzić bieżący HEAD, zakończone zadania i archiwa JSON opatrzone skrótem SHA-256. Wymagane są workflow: `build.yml`, `stage4-ubuntu-vm-validation.yml`, `beta07-codeql.yml`, `beta08-hardening.yml` i `beta09-stage4-storage.yml`. Trzy zadania Stage IV storage muszą zakończyć się pomyślnie na **GitHub-hosted Ubuntu 22.04, 24.04 i 26.04**; nie publikuje się kluczy, dzienników ani danych fault-injection. Po autoryzowanym merge to samo wymaganie obowiązuje dla osobnego skanu `push main`.
2. **Wydajność:** agregaty ADC, ADCS i uwierzytelnionego HTTPS muszą zawierać trzy okna, dokładne liczby prób i zero błędów. Niezależna inspekcja archiwów oraz bramka RC sprawdzają nie tylko p99, lecz także minimalny udany RPS, maksymalne użycie CPU i przyrost RSS względem *wstępnie zadeklarowanej* polityki. Dane niefinitywne, przekroczony budżet, brak prób lub podrobiony stan `pass` są odrzucane. Testy na maszynach wirtualnych nie są certyfikatem przepustowości produkcyjnej.
3. **Zależności:** raport `npm audit` obejmuje równocześnie produkcję i narzędzia budowania (dev); high/critical bez zatwierdzonego wyjątku blokuje bramkę. `package.json` i lockfile v3 muszą się zgadzać co do nazwy, wersji, engines i bezpośrednich zależności; bezpośrednie wersje są dokładne, pakiety transytywne wymagają adresów rejestru npm po HTTPS i `sha512` integrity. Bramka nie aktualizuje automatycznie zależności, nie obniża progów i nie ukrywa nowych CVE.
4. **RC:** offline `beta09_release_candidate_gate.py` wymaga kompletu spójnych raportów wszystkich trzech systemów, TSAN/fuzz oraz pięciu niezależnie zweryfikowanych workflow. Zawsze zwraca `NO-GO`: potwierdzenie pochodzenia dowodów to nie zgoda na wydanie.

## Przekazanie do ręcznego przeglądu

Po zakończeniu workflow na **ostatnim** SHA (bez oczekiwania i odpytywania przez agenta):

```bash
# Wymaga oddzielnie dostarczonego tokenu GitHub tylko do odczytu.
python3 scripts/beta09_artifact_evidence.py \
  --root . --sha <dokladny_40_znakowy_SHA> --mode fix-pass \
  --output <prywatny_raport_GitHub.json>

# Raporty należy pobrać wyłącznie z powyżej zweryfikowanych archiwów.
python3 scripts/beta09_release_candidate_gate.py \
  --root . --reports <odizolowane_raporty_Ubuntu> \
  --hardening <odizolowane_raporty_TSAN_fuzz> \
  --github <prywatny_raport_GitHub.json> \
  --output <prywatny_raport_NO_GO.json>
```

Nie zapisuj tokenu, surowych wyników audytu npm, danych uwierzytelniania ani zawartości prywatnego stanu w repozytorium. Workflow `beta09-stage4-storage.yml` wykonuje próby EIO, ENOSPC i crash-after-fsync na **jednorazowym** tmpfs, bez kontaktu z produkcją.

**Nadal otwarte:** potwierdzenie *rzeczywistych* wyników GitHub na ostatecznym SHA, decyzje o v1→v2 i spójnym sześcioskładnikowym backupie, retencja i odzyskiwanie po utracie nośnika, testy fizycznych klientów/kluczy, pomiary na produkcyjnej maszynie, osobna zgoda Ownera i późniejszy osobny skan `main`. Bez tych zgód nie wydajemy RC ani nie uruchamiamy wdrożenia.
