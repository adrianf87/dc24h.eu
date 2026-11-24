# Beta-0.8.x — sprawdzenie etapów i poprawka CI

Zadanie [#240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), wspólna gałąź `beta-0.8.x-stage-full`, roboczy [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241).

**Wszystkie 10 jednostek pracy jest w historii gałęzi:** Etapy I–V (`beta-0.8.1`–`beta-0.8.20`) oraz Fix Passy 1–5. Jest to zakończenie kodowania zaplanowanych etapów, **nie potwierdzenie odbioru**: wymagany jest komplet zielonych testów dla dokładnego, ostatecznego SHA i późniejsza osobna decyzja Ownera. Nie zamykamy teraz zadania i nie scalamy PR do `main`.

## Zdiagnozowany błąd

W [GitHub Actions 45471721340](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45471721340) dla `c3c939d` Ubuntu 22.04 i 24.04 przeszły walidację VM, ale Ubuntu 26.04 zakończył się błędem `client_error:restored_options_429`. Testy bazodanowe, CTest, przeglądarkowe i pomiar 24/24 HTTPS zakończyły się przed tym punktem powodzeniem. Pozostałe workflow C++/WebAdmin, CodeQL i TSAN/fuzz były zielone dla tego SHA.

Odizolowany test odtwarzania wykonuje próbę powtórzenia zużytego kodu TOTP (prawidłowo odrzuconą), ponowne logowanie hasłem i TOTP oraz niezależne logowanie hasłem i passkey. Node zachowuje w kopii prywatny limit **czterech prób logowania na konto w ciągu 60 sekund**. Na szybszym obrazie Ubuntu kolejna prawidłowa próba trafiała w HTTP 429, zanim test mógł ocenić odtworzony klucz WebAuthn.

Poprawiono wyłącznie test CI: odczytuje on własny odizolowany, odtworzony katalog limitera i czeka tylko wtedy, gdy pierwotne 60-sekundowe okno jeszcze blokuje nową próbę. Kontroluje również globalny limit, brakujące wpisy i błędne znaczniki czasu; w takich przypadkach przerywa test. **Nie resetuje katalogu, nie zwiększa limitów i nie obchodzi haseł, MFA ani audytu**. Dodano 5 testów tej logiki do workflow Ubuntu.

Testy lokalne: 5/5 pomocniczych, składnia Python, dokumentacja i metadane `beta-0.8.20` — OK. Rzeczywisty test C++/Node/Apache/MariaDB v11/Chromium musi zostać wykonany przez kolejny workflow na GitHub-hosted Ubuntu 22.04/24.04/26.04.

**Po push/uruchomieniu CI zakończyć pracę bez oczekiwania na wyniki.** Zostawić PR jako Draft i zadanie otwarte, dopóki późniejsza kontrola dokładnego SHA oraz decyzja Ownera nie pozwolą na dalsze działania. Bez wdrożenia i scalania do `main`.
