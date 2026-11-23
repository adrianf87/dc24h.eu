# beta-0.3.12 — przegląd kandydata wydania (bez wdrożenia)

To **ręczna** lista kontroli końcowej dla gałęzi `beta-0.3.1-12-full`
(Etapy I–IV, 0.3.1–0.3.12). Nowy numer wersji nie oznacza zaliczenia CI,
zgody na scalenie, utworzenia tagu ani instalacji. PR powinien pozostać
Draft do osobnej decyzji po przeglądzie.

## beta-0.3.11 — testy obciążenia, awarii i audytu

- Deterministyczne testy Node bez gniazd wysyłają 16 równoległych logowań
  i 128 anonimowych żądań. W budżecie jednej minuty najwyżej cztery
  logowania na konto mogą sprawdzić hasło w prywatnym API; anonimowe odczyty
  nie mogą dotrzeć do C++. To **nie** jest test pojemności ani benchmark.
- Osobny test zatrzymuje 64 równoległe odczyty statystyk podczas sztucznej
  awarii API, potwierdza ogólny 503 bez danych prywatnych/starych oraz
  poprawny odczyt po odzyskaniu dostępu. Pozostają testy wycofania grantu,
  wygaśnięcia, wylogowania, replay i wyścigów uprawnień.
- Ujednolicony format audytu zapisuje wyłącznie schemat, nazwę komponentu,
  stały typ zdarzenia i czas UTC ISO. Testy blokują sfałszowane zdarzenia
  i błędny zegar; bez identyfikatorów kont, IP, haseł, TOTP, grantów czy
  cookie. Trwałość i retencja journald nadal zależy od operatora. Awaria
  audytu nie przyznaje uprawnień, ale poprawne działanie może się odbywać
  bez trwałego wpisu — **to nie jest audyt zgodności**.

## beta-0.3.12 — warunki zakończenia serii

1. **CI GitHub:** pełny sukces dla dokładnego końcowego commita, a nie
   poprzedniej wersji. Sprawdź `Check status` na Ubuntu 22.04/24.04/26.04:
   C++ Debug/Release, CTest, TypeScript, Node, Chromium, integrację API C++,
   kontrolę zależności npm oraz ASan/UBSan. Sprawdź zadania pominięte.
2. **Brama kontroli wydania:** uruchom
   `python3 tests/beta_release_gate_tests.py`,
   `python3 tests/beta_baseline_tests.py`,
   `python3 tests/remote_gateway_config_tests.py`,
   `python3 tests/project_checks.py` i testy Node. Wersja huba, API,
   kontrolera, WebAdmin i fixtures musi wynosić `beta-0.3.12`.
3. **Zakres bezpieczeństwa:** pięć tras `/remote/v1` tylko do odczytu,
   bieżąca kontrola uprawnień C++, brak zdalnych operacji zapisu, brak
   publicznego portu, niezmieniony publiczny Apache, schemat MariaDB v11,
   numery pakietów `0.1.0` i protokół ADC/ADCS. Sprawdź różnice w kodzie.
4. **MFA i sesje:** unikatowe sekrety na konto, pliki 0600, katalog 0700,
   utrwalanie zużycia kodu po restarcie, zegar, limity prób, cookie/CSRF,
   wycofanie grantów, 2 minuty bezczynności i 10 minut życia sesji.
   Wyłudzenie hasła i kodu TOTP nadal jest możliwe.
5. **HTTPS, izolacja i pilotaż:** na serwerze testowym sprawdź certyfikat
   (SAN, ważność, zaufanie), szyfry TLS, nagłówki, systemd i gniazdo C++.
   Jeśli operator uruchomi prywatny pilotaż, zweryfikuj klucz SSH i
   certyfikat HTTPS, wykonaj
   `python3 scripts/remote_pilot_check.py --ca-file ...` oraz sprawdź,
   że lokalny tunel na stacji słucha tylko na `127.0.0.1`.
6. **Audyt operacyjny:** zapewnij uprawnienia dostępu, trwały journald,
   retencję, alerty i kopie poza hostem. Obecne wpisy nie przypisują
   użytkowników i nie są odporne na modyfikację; brak gwarancji trwałości.
   Nie przedstawiaj systemu jako gotowego do audytu zgodności.
7. **Decyzja ręczna:** zapisz zgody, SHA commita, zielone CI, ryzyka i plan
   wycofania przed tagowaniem, scaleniem albo instalacją. Ten patch nie
   publikuje wydania, nie tworzy tagu i nie scala z `main`. Nie uruchamiaj
   publicznej bramy tylko z TOTP.

## Awaria i wycofanie

Przy problemach z HTTPS, prywatnym API, dziennikiem MFA lub audytem
zakończ testy bez wdrożenia. Wyłącz tunel SSH, wyloguj użytkownika i
zatrzymaj osobną bramę. W razie wycofania testowego wdrożenia przywróć
razem poprzednią zatwierdzoną lokalną konfigurację i binaria. **Zachowaj
sekrety i dziennik anty-replay MFA** — nie przywracaj starszej wersji
dziennika z tym samym sekretem. Ponownie sprawdź TLS i uprawnienia.

Przed dostępem internetowym produkcyjnym nadal potrzebne są WebAuthn/
passkeys odporne na phishing, rejestracja i odzyskiwanie urządzeń,
niezależne limity/ochrona nadużyć, trwały i przypisany audyt, architektura
publicznego brzegu, pomiary wydajności oraz odrębny audyt bezpieczeństwa.
