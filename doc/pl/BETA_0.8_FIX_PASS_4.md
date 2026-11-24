# Beta 0.8 Fix Pass 4 — przegląd Etapu IV (0.8.13–0.8.16)

Zakres: [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), wspólna gałąź `beta-0.8.x-stage-full` i roboczy PR #241. Dokument potwierdza naprawy i lokalne regresje; **nie stanowi** akceptacji Etapu IV, zgody na produkcję ani merge.

## Usunięte błędy

1. **Podmiana dziennika incydentów:** samo porównanie skrótu pliku pozwalało nie wykryć zastąpienia pliku nowym inode z identyczną treścią. Odczyt i zapis wiążą teraz urządzenie, inode oraz SHA-256. Zamiana zewnętrzna oznacza `unknown`, a zapis nie zastąpi podstawionego pliku. Własny zapis atomowy aktualizuje oczekiwaną generację.
2. **Archiwum audytu po zatrzymaniu zapisujących procesów:** dotychczas po weryfikacji łańcucha program ponownie otwierał źródło po nazwie, co tworzyło okno podmiany ścieżki. Teraz zachowuje otwarty deskryptor z `O_NOFOLLOW`, sprawdza tożsamość inode i uprawnienia, porównuje zawartość przed/po weryfikacji i kopiowaniu, ponownie sprawdza łańcuch przed opublikowaniem archiwum. Źródło i ciągłość numeracji pozostają bez zmian.
3. **Awaria opcjonalnego dziennika:** błędy jego odczytu/zapisu/oznaczenia niedostępności nie mogą przerywać poprawnego odczytu C++ ani obsługi utraty audytu. Historia zachowana ma wtedy stan `unknown` bez zdarzeń i bez ujawniania danych prywatnych.

## Wyniki i ograniczenia

- Lokalne regresje Node: **58/58 zaliczone**; obejmują świeżość po restarcie, równoległe cofanie uprawnień, prywatność, limit 8 MiB (symulacja obniżonym progiem), uszkodzenie plików, podmianę inode, zachowanie archiwum, awarię telemetrii i walidację danych ekranu EN/PL.
- `npm run build` (TypeScript/Vite) i `npm run build:remote:ci` (bramka Node) zakończone poprawnie.
- Audyt nadal zatrzymuje autoryzowane zapisy przy limicie **8 MiB**; ostrzeżenie pojawia się od **75%**. Opcjonalna historia ma najwyżej **12** zdarzeń z **10 minut**, do **4 KiB**, tylko stałe kategorie. Nie jest szóstym magazynem uwierzytelniania ani elementem kopii odtwarzania. Widok EN/PL pozostaje tylko do odczytu.
- **OWNER/operator musi zatrzymać wszystkie procesy zapisujące audyt przed archiwizacją.** Program nie potwierdza stanu usługi systemowej; wyścig po ostatniej kontroli nadal wymaga tego warunku. Nie wykonuje rotacji, kasowania, resetu łańcucha ani zwalniania limitu. Skrót SHA-256 nie jest podpisem zabezpieczającym przed uprawnionym atakującym.
- Zredukowane testowe limity nie potwierdzają rzeczywistego `ENOSPC`, produkcyjnej retencji ani przejścia pełnego CI. Konieczne są wyniki na dokładnym SHA z GitHub-hosted Ubuntu **22.04/24.04/26.04**, integracją C++, przeglądarką i narzędziami bezpieczeństwa. PR zostaje **draft**, bez merge; zgodnie z instrukcją OWNER kończymy po wyzwoleniu CI, **bez oczekiwania ani odpytywania o wynik**.
