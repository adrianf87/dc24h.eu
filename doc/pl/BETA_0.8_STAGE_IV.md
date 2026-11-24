# Beta 0.8.13–0.8.16 — Etap IV: prywatna diagnostyka operatora

Zakres: [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), gałąź `beta-0.8.x-stage-full`, roboczy PR #241. **OWNER wyraził 24.11.2026 zgodę na Etap IV przed zakończeniem Etapów II i III.** To odstępstwo od kolejności nie oznacza akceptacji produkcyjnej.

## Świeżość i interpretacja stanu

- Tylko uwierzytelniony odczyt C++ potwierdza nową próbkę. Node odrzuca błędną, przyszłą i starszą niż 10 sekund próbkę. Przeglądarka wygasza dane po 45 sekundach łącznego wieku. Starsze API bez znacznika ma stan źródła **Nieznane**.
- Po restarcie bramki aktualna historia przejść jest **nieznana** do pierwszego świeżego odczytu. Zachowane zdarzenia są oznaczone „nie na żywo” i nie potwierdzają sprawności huba ani kompletności dowodowej.
- Historia nie zawiera identyfikatorów osób, kont, adresów IP, haseł, kodów, ciasteczek ani surowych rekordów audytu. Widok pozostaje tylko do odczytu, po lokalnym HTTPS.

## Opcjonalna prywatna historia zdarzeń

1. W odizolowanym środowisku testowym przygotuj katalog prywatny należący do użytkownika usługi (`0700`). Przed inicjalizacją zatrzymaj bramkę. Nie nadpisuj istniejącego pliku.
2. Jawnie zainicjuj **nowy** plik historii (testowa komenda z katalogu `webadmin`):

   ```bash
   node --import tsx server/remote-incident-journal.ts --initialize /path/to/private/incidents.json --offline-ack
   ```

3. Dla lokalnej próby ustaw `DC24H_REMOTE_INCIDENT_FILE` na tę samą bezwzględną ścieżkę i uruchom tylko lokalną bramkę. Domyślnie historia trwała jest wyłączona. Utrata albo uszkodzenie jawnie skonfigurowanego pliku uniemożliwia start bramki — nie inicjalizuj go ponownie automatycznie.
4. Limit: **12 zdarzeń, 10 minut, 4 KiB**; wymagana jest prywatna własność, tryb `0600`, brak dowolnego symlinka w ścieżce. Plik stanowi nieautorytatywną telemetrię i **nie wchodzi** do sześcioskładnikowej kopii MariaDB v11 + pięć stanów uwierzytelnienia.

## Audyt 8 MiB — ostrzeganie i bezpieczna archiwizacja

- Po osiągnięciu **75%** limitu 8 MiB dziennik systemowy otrzymuje zbiorczy komunikat `audit_near_capacity` z ograniczeniem powtórek. Nie wysyłamy do WebAdmin surowych rekordów ani identyfikatorów. Przy braku miejsca, uszkodzeniu lub przekroczeniu limitu logowanie i operacje wymagające trwałego audytu są blokowane.
- **OWNER musi przed archiwizacją sprawdzić i zatrzymać usługę zapisującą audyt** (`systemctl stop` w odpowiednim izolowanym środowisku); sama flaga programu tego nie udowadnia. Wykonaj tylko na testowej, prywatnej kopii:

  ```bash
  node --import tsx server/remote-audit-archive.ts --source /path/to/private/audit.jsonl --archive /path/to/private/audit-archive.jsonl --owner-stopped-writer-ack
  ```

- Program odmawia nadpisania archiwum, sprawdza cały łańcuch i zachowuje oryginał bajt po bajcie. **Nie usuwa ani nie zeruje audytu** i nie przywraca miejsca w aktywnym limicie 8 MiB. Dalsza retencja/rotacja wymaga osobnej decyzji właściciela oraz projektu ciągłości łańcucha. Nie publikuj archiwum w Git ani artefaktach CI.

## Dostępność i testy

- Ekran EN/PL rozróżnia stan nieznany, bieżące incydenty i zachowaną historię. Klawiatura oraz lokalne potwierdzenie działają bez POST do huba, zmiany ról, start/stop lub uprawnień.
- Testy obejmują restart, cofnięcie zegara, nieprawidłowe pliki, symlinki, uprawnienia, ograniczenia historii, odmowę zapisu audytu, zachowanie łańcucha i prywatność pól. To nie jest certyfikat produkcyjny, test fizycznego passkey ani gwarancja odzyskiwania.
- Pełne CI na dokładnym SHA musi obejmować GitHub-hosted Ubuntu **22.04/24.04/26.04**. Zgodnie z OWNER **zakończyć po uruchomieniu CI i nie czekać na wyniki**. Nie wykonywać merge ani wdrożenia w tym etapie.
