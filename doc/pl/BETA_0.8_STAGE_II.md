# Beta-0.8.5–0.8.8 — Etap II, WebAuthn, MFA i bezpieczeństwo sesji

Zadanie [#240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), wspólna gałąź `beta-0.8.x-stage-full` i roboczy [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241). Bez zmian na produkcji, w MariaDB v11, pięciu plikach prywatnych i uprawnieniach C++.

## Fix Pass 2 — niezależna weryfikacja (jednostka 7/10)

Potwierdzono i naprawiono dwa błędy:

1. **Podmiana sekretu TOTP przy zachowanym mtime:** Weryfikator posiadał skopiowany sekret i sprawdzał metadane pliku, ale nie jego bieżącą treść. Teraz przy uruchomieniu wiąże sumę SHA-256 z zawartością, a przed każdą próbą weryfikuje treść i `ctime`. Test podmienia poprawny sekret na inny o tej samej długości, przywraca `mtime`, wymaga odmowy oraz braku dopisania do historii zużytych kodów. To kontrola aktywnego procesu, nie podpis pliku po restarcie.
2. **Zapis passkey przed odmową końcowego audytu:** Jeśli klucz został już zapisany, ale końcowy audyt lub kontrola sesji odmawia zakończenia, wszystkie wcześniejsze sesje konta są teraz unieważniane nawet przy odpowiedzi HTTP 403. Test dwóch sesji wymusza awarię audytu `passkey_enrolled` i sprawdza odmowę obu ciasteczek; zapisany klucz nie jest pozornie cofany.

Ponownie sprawdzono: podpisane ES256 WebAuthn, Origin/RP/UP/UV i challenge; licznik i backup, cofnięcie klucza/epoch oraz uprawnienia C++; replay TOTP/rotację/cofnięcie zegara; podmianę licznika passkey i limitu prób przy tej samej długości/mtime; jednorazowe challenge, CSRF i stare sesje. **Wirtualny uwierzytelniacz Chromium nie certyfikuje fizycznego ani synchronizowanego urządzenia.**

**Nadal otwarte:** Pełny restart po poprawnie sfałszowanej offline zawartości katalogu passkey/limitów nie jest uwierzytelniany przez hash trzymany tylko w pamięci. Wymaga osobnej zgody Ownera na trwały MAC/anchor, migrację i rollback; tego formatu nie zmieniano. Lokalnie przeszło 26 celowanych testów Node i TypeScript. Wyniki przeglądarki, pełnego CI Ubuntu 22.04/24.04/26.04, C++/Apache/MariaDB oraz odtwarzania pozostają do niezależnej kontroli dla dokładnego SHA po push. Bez merge, publikacji ani wdrożenia.

| Patch | Zmiana |
| --- | --- |
| **0.8.5** | Wirtualny Chromium podpisuje próbę logowania, a restart bramy i zgodne odtworzenie pięciu plików odrzuca stary challenge. Ucięty katalog jest odrzucany; nowe logowanie wymaga świeżej ceremonii i hasła. Pełny C++/Apache/Node/MariaDB dowodzi tego wyłącznie w hostowanym CI. |
| **0.8.6** | Otwarte katalogi passkey sprawdzają również SHA-256 zawartości, aby wykryć fałszywe cofnięcie licznika przy tej samej długości i mtime. Równoległe cofnięcie klucza wygrywa ze starszą zmianą nazwy. |
| **0.8.7** | Analogiczna ochrona wykrywa podmianę liczby prób w otwartym pliku limitera. Zachowano testy replay TOTP, rotacji offline, cofnięcia zegara i trwałych limitów po zwykłym restarcie. |
| **0.8.8** | Nowe logowanie hasłem+TOTP lub hasłem+passkey przekazuje do innych kart wyłącznie stały sygnał utraty poprzedniej sesji. Stare metryki znikają, formularz ma fokus, komunikaty EN/PL nie ujawniają sekretów. |

## Procedura bezpieczeństwa przy uszkodzeniu pliku

1. Zatrzymaj i odizoluj bramę, nie nadpisuj uszkodzonego pliku ani nie twórz pustej historii TOTP, passkey, audytu lub limitów.
2. Zabezpiecz zgodną, zweryfikowaną kopię MariaDB v11 i wszystkich pięciu prywatnych plików, ze ścisłymi prawami właściciela i bez symlinków.
3. Przeprowadź kontrolę kopii i ewentualną próbę odtworzenia w **świeżym, odizolowanym** środowisku. Nie podmieniaj aktywnej bazy produkcyjnej.
4. Restart produkcyjny i decyzja o przywróceniu dostępu należą wyłącznie do Ownera.

**Ograniczenie 0.8.7:** SHA-256 porównywany ze stanem w pamięci wykrywa podmianę w działającym procesie, nawet przy niezmienionym mtime. Po całkowitym wyłączeniu wszystkich procesów nie można wiarygodnie uwierzytelnić poprawnie wyglądającego pliku zmienionego offline bez osobnego trwałego podpisu/MAC. Wymaga to jawnie zatwierdzonego projektu migracji i rollbacku — **nie deklarujemy odporności na restartową podmianę**.

Lokalnie: **59** testów bezpieczeństwa Node, **4** testy bramki Etapu II i **4** Etapu I oraz build TypeScript/Vite przeszły. Chromium tutaj nie uruchomiło się (`socket() EPERM`); nie zaliczamy przeglądarki na podstawie tego przebiegu. Nowa bramka wymaga realnego wyniku Playwright i zgodnego C++/Node/Apache/MariaDB na GitHub-hosted **Ubuntu 22.04, 24.04 i 26.04**.

Po push kończymy **bez oczekiwania na CI**. Pending to nie akceptacja. Bez merge, tagu, publikacji pakietu i deploymentu; Etap III wymaga osobnego polecenia Ownera.
