# Beta-0.8.1–0.8.4 — Etap I, interoperacyjność ADC/ADCS

Zadanie [#240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), wspólna gałąź `beta-0.8.x-stage-full`. Bazą jest chroniony `main` o SHA `20eae07ada1645e7caa11e0b1c3cd0d7c15b8a1f`.

Etap I obejmuje wyłącznie cztery patche, bez wdrożenia produkcyjnego, publikacji pakietu i przechodzenia do Etapu II. Po wypchnięciu zmian oraz uruchomieniu CI **nie czekamy na wyniki**. Weryfikacja i odbiór wymagają osobnej decyzji właściciela.

| Patch | Zakres |
| --- | --- |
| **0.8.1** | Przypięte wersje i skróty SHA-256 rzeczywiście uruchamianego klienta `ncdc`, dodatkowa bramka odrzucająca niepełne dowody. DC++/AirDC++: **nieprzetestowane**, dopóki nie ma wersji, binariów i faktycznych uruchomień. |
| **0.8.2** | SUP/SID/INF, hasło GPA/PAS, role oraz test odmowy dostępu po zatrzymaniu huba, odebraniu statusu konta w odizolowanej MariaDB i ponownym starcie. |
| **0.8.3** | Wyszukiwanie TTH, odpowiedź, przejścia aktywny/pasywny, CTM/RCM, odmowy nieprawidłowego odbiorcy i kontynuacja po błędzie. |
| **0.8.4** | TLS 1.2/1.3, zmiana certyfikatu po restarcie, odmowa dla starego zaufania i próby nieszyfrowanego połączenia z ADCS. |

## Zmiana uprawnień i unieważnianie sesji

Statusy i uprawnienia w C++ są migawką pobieraną podczas startu. **Sama modyfikacja konta w bazie nie jest obietnicą natychmiastowego odebrania dostępu**. W przyszłej, osobno zatwierdzonej procedurze operator powinien: (1) sprawdzić tożsamość i uprawnienia do zmiany, (2) zaplanować przerwę i bezpiecznie zatrzymać hub, (3) dokonać zmiany zatwierdzonym mechanizmem, (4) uruchomić hub, (5) potwierdzić rozłączenie starych sesji i odmowę nowego logowania, (6) zachować redagowany wynik i przygotować plan powrotu. To opis kroków, **nie polecenie wykonania na produkcji**.

Test korzysta wyłącznie z odizolowanej instancji MariaDB uruchamianej przez CI, a następnie przywraca stan fikcyjnego konta. Nie dotyka bazy, użytkowników ani usług produkcyjnych.

## Warunki odbioru

- Wszystkie wymagane testy procesu C++, `ncdc`, WebAdmin, TLS i bezpieczeństwa muszą przejść na tym samym SHA na GitHub-hosted Ubuntu 22.04, 24.04 i 26.04.
- Nowy raport `beta08-stage1-interoperability.json` musi odrzucać testy pominięte, nieuruchomione, błędne, podwójne, wyniki ze złego commita oraz fikcyjne dowody innych klientów.
- Bez Debian 12/13, prywatnych runnerów, Xeona D-1521, fizycznych urządzeń i gwarancji wydajności produkcyjnej.
- Po rozpoczęciu CI zakończyć pracę. Wynik **pending nie oznacza akceptacji**; odrębnie następuje Fix Pass 1 i dalsze etapy.

**Status: zaimplementowano wymagania Etapu I; wyniki hostowanego CI jeszcze niezweryfikowane.**
