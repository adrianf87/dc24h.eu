# Beta 0.7.x — Fix Pass 2: WebAuthn, MFA i sesje (0.7.5–0.7.8)

**Status: wykrytą wadę wyścigu uprawnień naprawiono; testy ukierunkowane lokalnie przeszły. Końcowe CI na nowym SHA pozostaje niezweryfikowane. NO-GO dla merge i wdrożenia.** Dotyczy tylko Fix Pass 2 z [#238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238) na istniejącej gałęzi `beta-0.7.x-stage-full` i w roboczym [PR #239](https://github.com/adrianf87/dc24h.eu-high-starter/pull/239).

## Naprawa

W operacjach `passkey/device/rename` i `passkey/device/revoke` prywatna brama najpierw sprawdzała uprawnienia C++, a następnie asynchronicznie odczytywała treść żądania. Jeżeli w tym czasie Owner cofnął uprawnienia, operacja mogła zmienić stan passkey na podstawie już nieaktualnej decyzji. Po poprawce brama **ponownie pyta C++ o aktualne uprawnienia po odczytaniu całej treści, bezpośrednio przed zapisem**. Sprawdza też, czy sesja nie została wylogowana, wymieniona, przeterminowana albo cofnięta przez zmianę epoch. Brak odpowiedzi C++ oznacza odmowę, nie zgodę.

Test wymusza dwie sytuacje: cofnięcie uprawnień podczas przesyłania żądania zmiany nazwy oraz podczas cofania passkey. Obie operacje są odrzucone (`403`), klucz pozostaje niezmieniony, a stara sesja traci dostęp.

## Przegląd czterech patchy

| Patch | Zakres | Wynik |
| --- | --- | --- |
| **0.7.5** | Rzeczywisty WebAuthn, zgodny restore, Origin/RP ID, jednorazowe challenge | Testy kryptograficzne przeszły; odtworzenie w prawdziwym Chromium i MariaDB wymaga CI Ubuntu |
| **0.7.6** | Dwa passkeys, liczniki, backup, własność kont, unieważnienie i epoch | Testy lokalne przeszły, bez dodatkowej potwierdzonej usterki |
| **0.7.7** | TOTP, cofnięcie zegara, replay, rotacja offline, zachowanie plików | Testy lokalne przeszły, bez dodatkowej potwierdzonej usterki |
| **0.7.8** | Wiele sesji, CSRF, opóźnione odpowiedzi, wylogowanie i zmiana uprawnień | **Naprawiono wyścig cofnięcia grantów przy zmianie/odwołaniu passkey** |

## Kontrola i działania operatora

- Testy ukierunkowane: **54/54**; TypeScript i `git diff --check`: bez błędów.
- Cały zestaw Node: 147 zaliczonych; trzy próby gniazd Unix zakończyły się `listen EPERM` z powodu ograniczeń lokalnego środowiska.
- Playwright: brak lokalnego Chromium; pobranie zakończyło się HTTP 403. Nie przypisujemy sukcesu testowi przeglądarkowemu.
- Wymagana pełna macierz **GitHub-hosted Ubuntu 22.04/24.04/26.04 na jednym końcowym SHA**, wraz z C++, WebAdmin/Chromium, CodeQL, sanitizerami, systemd, ADC/ADCS/TLS, sparowanym SQL v11 i pięcioma plikami, pakietami oraz pomiarami.

Po wypchnięciu commita sprawdzenie ostatecznego CI i formalna akceptacja pozostają osobnym krokiem. Zgodnie z decyzją Ownera **nie czekamy na wyniki uruchomionego CI**. Nie wykonujemy merge, deploymentu, aktualizacji produkcji ani zmian grantów. Dalsze Fix Pass 3–5 wymagają osobnych poleceń.
