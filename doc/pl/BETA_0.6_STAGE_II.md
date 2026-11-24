# beta-0.6.1–beta-0.6.16 — Etap II: wspólny przegląd i poprawki

Jedna gałąź: `beta-0.6.x-stage-full` · [Issue #236](https://github.com/adrianf87/dc24h.eu-high-starter/issues/236) · [roboczy PR #237](https://github.com/adrianf87/dc24h.eu-high-starter/pull/237). Etap I przeszedł CI dla `ea3cd114`. **Etap II czeka na osobne, pełne testy GitHub CI dla nowego SHA.** Bez wdrożenia, scalenia i zatwierdzenia produkcyjnego.

## Wyniki przeglądu wszystkich 16 zadań

| Zakres | Co sprawdzono | Wynik i poprawka |
| --- | --- | --- |
| 0.6.1–0.6.4 | Pochodzenie wyników GitHub CI, rzeczywiste HTTPS hasło+TOTP, korelację diagnostyczną, p99/RPS/CPU/RSS | Nie odtworzono kolejnego błędu w regułach walidacji i testach odmowy. Pozostaje limitowany pomiar 24 żądań w trybie konserwacji, bez obietnic wydajności produkcyjnej. |
| 0.6.5–0.6.8 | Spójność MariaDB v11 + pięciu plików, logowanie po odtworzeniu, poprzednią binarkę i rotację tokenu | **Poprawiono:** podczas awarii zapisu katalog częściowej kopii lub odtworzenia mógł utracić bezpieczne uprawnienia `0700`. Próba posprzątania kończyła się wtedy `ValueError`, zasłaniając właściwy `OSError`. Teraz zachowuje niepewny katalog do analizy i zgłasza pierwotną awarię. Dwa testy regresji obejmują kopię i odtwarzanie. |
| 0.6.9–0.6.12 | ADC/ADCS, negocjację TLS, obciążenie wolnymi klientami, certyfikaty, zamykanie i restart | Nie odtworzono kolejnego błędu w testach procesu. Minimalny TLS po stronie klienta testowego to 1.2; stare szyfry, niewłaściwe certyfikaty i klucze są odrzucane. Pełne testy C++/TLS/sanitizerów muszą ponownie przejść na Ubuntu. |
| 0.6.13–0.6.16 | Przegląd incydentów, lokalne potwierdzenie, ważność odczytu, bramkę awarii, dokumentację | **Poprawiono:** licznikom wolno ufać przez 45 sekund, a zegar ekranu przeliczał ich ważność co 30 sekund. Przy zawieszonym odczycie mogły być pokazywane zbyt długo. Odświeżanie zegara następuje teraz co sekundę; test Playwright po 46 sekundach bez nowego odczytu wymaga ukrycia liczników. Bez nowych akcji administracyjnych. |

## Warunki zakończenia Etapu II

1. Pełne CI na **identycznym końcowym SHA**: C++ Debug/Release, WebAdmin, Playwright i audyt npm na Ubuntu 22.04/24.04/26.04; ASan/UBSan, CodeQL, rzeczywisty systemd, ADC/ADCS/TLS, oddzielne próby awarii, uwierzytelniony prywatny HTTPS, MariaDB v11 + pięć plików, świeże logowanie i historyczny rollback. Żaden wymagany job nie może być pominięty, anulowany ani niezaliczony.
2. W Issue/PR zapisać SHA, linki do przebiegów oraz zanonimizowane wyniki: 24/24 żądania HTTPS z latencją/RPS/CPU/RSS, po 12/12 ADC i ADCS, trzy okna obciążeniowe dla każdego profilu, sześć prób awarii, wynik kopii/odtworzenia i rollbacku. Wyniki testowe **nie są** pomiarem produkcyjnej pojemności ani RPO/RTO.
3. Dopiero po tych kontrolach odhaczyć Etap II. **Nie** oznaczać PR jako gotowego, nie scalać z `main`, nie zamykać zadania i nie wdrażać — to osobna końcowa bramka.

Zachowany niepewny katalog po przerwaniu odtwarzania nie jest poprawną kopią: należy go odizolować i wyjaśnić przyczynę, nie używać do logowania. Nadal brakuje produkcyjnego testu na Debian/Xeon, pełnej ceremonii passkey po odtworzeniu, zgodności z zewnętrznymi klientami i atomowości zapisu SQL+plików. Pozostają granice zaufania z [Etapu I](BETA_0.6_STAGE_I.md) oraz [angielski opis przeglądu](../us/BETA_0.6_STAGE_II.md).
