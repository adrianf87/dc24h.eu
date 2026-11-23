# Poprawki historycznej alpha-0.1 — przegląd na podstawie beta-0.3.12 main

Ta gałąź naprawia dwa nadal aktualne *problemy operacyjne w kodzie* wykryte
podczas powtórnego przeglądu 11 etapów alpha-0.1. Nie cofa projektu do
alpha-0.1, nie zastępuje niezależnego pentestu i nie otwiera dostępu
publicznego. Zakres opisuje [historyczny audyt](AUDIT_ALPHA_0.1.md).

## Poprawki

1. **A11 — blokujące logowanie:** wątek obsługi klienta formatuje ograniczony
   rekord, a zapis wykonuje osobny wątek. Kolejka mieści najwyżej 256 wpisów
   i po odzyskaniu odbiornika raportuje liczbę pominiętych rekordów. Trwale
   zablokowane wyjście może nadal zatrzymać destruktor loggera; SIGKILL może
   utracić kolejkę. Nie jest to trwały audyt zgodności. Sygnały zakończenia
   są blokowane przed utworzeniem wątku piszącego, zachowując dotychczasową
   obsługę SIGINT/SIGTERM.
2. **A12 — myląca gotowość:** `/api/v1/health` zachowuje dotychczasowe znaczenie
   dostępności wątku API. Chronione tokenem `/api/v1/ready` zwraca 200 tylko
   po uruchomieniu Core i całego skonfigurowanego zestawu nasłuchów ADC/ADCS;
   w innych przypadkach 503. Nie ujawnia sekretów, nie korzysta z proxy WWW
   ani SQL. Bieżąca dostępność bazy, gotowość kont i pojemność są osobne.

Testy obejmują zablokowany odbiornik logów, limit kolejki i odmowę 503,
jak również kontrolę praw dostępu do lokalnego API. Istnieją już osobne
testy wolnego odbiornika ADC, ograniczony fuzzing parsera i testy obciążenia.

## Ustalenia z alpha-0.1 naprawione już wcześniej

- **A01–A04:** dokumentacja, metadane i wymagane integracje CI w alpha-0.2.1.
- **A10:** wysyłanie ADC oraz protokół sesji/logowania w alpha-0.2–0.3.
- **A14:** konta, hasła i role w alpha-0.7–0.8.
- **A13:** osobny portal tylko do odczytu, lokalny, z hasłem i TOTP w
  beta-0.2–0.3. Nie oznacza to gotowego panelu publicznego.
- **A15:** późniejsza macierz CI z Ubuntu 26.04, sanitizery, ograniczony
  fuzzing i obciążenie. Osobne systemy nadal wymagają realnej weryfikacji.

## Świadome granice

Przeładowywanie INI/certyfikatów podczas pracy, automatyczna rotacja i
ponowne otwieranie plików logów, kontrola bieżącej bazy danych, publiczny
WebAdmin, nowe migracje i zdalne operacje zapisu wymagają osobnych projektów
oraz decyzji operatora. Nie scalać ani nie wdrażać automatycznie; wymagane
są pełne CI C++/integracji dla nowego commita i przegląd bezpieczeństwa.
