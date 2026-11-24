# Beta 0.7.x — Fix Pass 1: przegląd interoperacyjności ADC/ADCS (0.7.1–0.7.4)

**Status: poprawka i testy lokalne zakończone; końcowe CI na nowym SHA jeszcze niezweryfikowane. NO-GO dla merge i wdrożenia.** Dotyczy wyłącznie Etapu I, wspólnej gałęzi `beta-0.7.x-stage-full`, roboczego [PR #239](https://github.com/adrianf87/dc24h.eu-high-starter/pull/239) i [zadania #238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238). Następne Fix Pass 2–5 wymagają osobnych poleceń.

## Naprawiona niezgodność pochodzenia dowodów

Skrypt `scripts/beta07_client_evidence.py` wcześniej przyjmował prawidłowo zapisany 40-znakowy SHA bez porównania z rzeczywistym `git rev-parse HEAD`. Nie sprawdzał też bezpośrednio, czy zadeklarowany obraz Ubuntu odpowiada obrazowi uruchomionego hostowanego runnera. Mogło to prowadzić do mylącego raportu z wyniku CTest dla innego commita.

Obecnie przed oceną JUnit i ncdc wymagane są: **ten sam SHA wejściowy i HEAD**, GitHub Actions, `RUNNER_ENVIRONMENT=github-hosted`, Linux i dopasowany Ubuntu 22.04/24.04/26.04. Nowe testy odrzucają zły SHA, prywatnie zarządzany runner, błędny system oraz niezgodności wersji i sum. To kontrola spójności w środowisku CI, a **nie** samodzielne kryptograficzne uwierzytelnienie API GitHub. Owner musi porównać prawdziwe ID i wyniki zadań oraz pochodzenie raportów.

## Cztery sprawdzone patche

| Patch | Przegląd i wynik | Pozostałe ograniczenia |
| --- | --- | --- |
| **0.7.1** | Weryfikacja manifestu ncdc, wersji z apt, sum SHA-256 pakietu i pliku, `dpkg --verify` oraz 25 prawdziwych przypadków CTest. **Naprawiono powiązanie SHA i obrazu Ubuntu** | Tylko ncdc jest uruchamiany. DC++ i AirDC++ pozostają `untested` |
| **0.7.2** | SUP/SID/INF, CID/PID, Unicode, hasło, odrzucenie spoofingu, powtórzonego SUP i ramek przed logowaniem | Bez potwierdzenia zgodności wszystkich klientów innych producentów |
| **0.7.3** | SCH/RES/TTH, CTM/RCM, aktywny/pasywny transfer, odmowy, limity i kolizje portów | Testy ncdc korzystają z izolowanego loopback IPv4; osobne testy procesu obejmują odpowiednie ścieżki IPv6 |
| **0.7.4** | Weryfikacja nazwy hosta i zaufania TLS 1.2/1.3, odrzucenie starych certyfikatów, błędnego klucza, słabego szyfru i downgrade | P2P ncdc z certyfikatem testowym nie dowodzi publicznej zgodności z CA/hostname |

## Kontrola przed decyzją Ownera

Poprzedni SHA `abdd0d27665523a69bfd134e2cdc7998a942c092` miał zielone przebiegi [C++/WebAdmin](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45449048235), [Ubuntu VM](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45449048397) i [CodeQL](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45449048232). **Nie oznacza to przejścia CI na commicie Fix Pass 1.** Wymagana pozostaje kompletna macierz GitHub-hosted Ubuntu 22.04/24.04/26.04 (C++, WebAdmin, Chromium, CodeQL, ASan/UBSan, systemd, ADC/ADCS/TLS, MariaDB, pięć prywatnych plików, pakiety i pomiary) na identycznym końcowym SHA.

Nie zmieniono serwera produkcyjnego, polityki połączeń, schematu MariaDB v11, praw C++ ani konfiguracji operatora. Pomiary CI nie są gwarancją wydajności Debian/Xeon. Nie wykonuj merge, wdrożenia ani aktywacji operatora bez późniejszej, wyraźnej zgody. Po wypchnięciu poprawki kończymy pracę **bez czekania na wyniki CI**.
