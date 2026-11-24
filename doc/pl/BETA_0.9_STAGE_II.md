# Beta-0.9.5–0.9.8 — Etap II: autentyczność prywatnego stanu i odzyskiwanie dostępu

Zadanie [#242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242), gałąź `beta-0.9.x-stage-full`, jeden roboczy [PR #243](https://github.com/adrianf87/dc24h.eu-high-starter/pull/243). Jednostka 2/10. **Nie wdrożono nowego schematu plików, MAC uruchamianego przy starcie, kluczy produkcyjnych ani migracji.** Wymagają odrębnej zgody Ownera.

## Wykonany zakres

| Patch | Wynik | Granica |
| --- | --- | --- |
| **0.9.5** | Powstał czysto pamięciowy model projektowy HMAC-SHA256 dla pięciu plików, osobnego hash pary SQL/backup, generacji i zewnętrznego punktu zaufania. Testy odmowy: plik tej samej długości/mtime, błędny klucz, brak klucza, niepełny zestaw, cofnięcie generacji, odtworzony manifest. | Model nie zapisuje plików i nie jest uruchamiany przez bramę; nie ma zgody na aktywację v2. |
| **0.9.6** | Ponownie testujemy istniejące podpisywanie *offline* pełnej kopii pięciu plików i SQL przy zmienionej zawartości z przywróconym mtime; zachowano testy historii TOTP, licznika prób i rollbacku zegara. | **Brak akceptacji aktywnej ochrony po zimnym restarcie** bez trwałego checkpointu i migracji. |
| **0.9.7** | Naprawiono wyścig: po zapisie nowego passkey, lecz przed zwróceniem wyniku, wyjątek weryfikatora nie pozostawia już aktywnych wcześniejszych sesji konta. Dodatkowo dwie równoległe prawdziwe sygnatury ES256 mogą podnieść licznik tylko raz. | Wirtualny Chromium nie certyfikuje fizycznych/synchronizowanych urządzeń. |
| **0.9.8** | Test utraty urządzenia, zachowania TOTP replay, izolacji drugiego konta i odmowy starego klucza po otwarciu katalogu. Test Playwright sprawdza EN/PL, fokus/klawiaturę i formularz hasło+TOTP po odmowie passkey. | Test UI nie zastępuje prawdziwej reautoryzacji C++; narzędzie offline wymaga niezależnej weryfikacji Ownera. |

## Decyzje niezbędne przed 0.9.6

1. **Zarządzanie 256-bitowym kluczem:** niezależne od MariaDB i pięciu plików miejsce przechowywania, kopia awaryjna, uprawnienia i rotacja z dwoma zweryfikowanymi punktami zaufania. Nie wolno automatycznie tworzyć klucza po jego utracie.
2. **Trwały punkt antyrollbackowy:** `(generation, mac)` poza kopiowanym zestawem. Sam podpis HMAC w tym samym katalogu nie ochroni przed odtworzeniem starego, nadal poprawnie podpisanego stanu.
3. **Protokół spójności:** każdy zapis TOTP/replay, audytu, limitu i passkey wykonuje oddzielne fsync/rename. Manifest dopisywany po fakcie nie gwarantuje atomowości pięciu plików. Wymagany projekt dziennika/commitu i odmowy przy częściowym zapisie.
4. **Przejście v1 → v2 i rollback:** tylko po zatrzymaniu wszystkich writerów, zgodnej kopii MariaDB v11 + pięciu plików, próbie w izolacji oraz zatwierdzonej procedurze powrotu. Utrata checkpointu/klucza lub stan mieszany to **NO-GO**, nie reset zabezpieczeń.

## Bezpieczna procedura Ownera do czasu decyzji

- Zachowuj działający schemat v1; po incydencie zatrzymaj bramę i zabezpiecz kompletne, zweryfikowane kopie. Nie inicjuj pustego replay TOTP ani limitera, nie odtwarzaj samego pojedynczego pliku.
- Utracony passkey unieważniaj wyłącznie przez istniejący operator-only tryb offline, po niezależnym potwierdzeniu tożsamości i praw, z trwałym wpisem audytu i restartem. Dwie nazwy operatorów są oświadczeniami, nie podpisami dwóch osób.
- Po odzyskiwaniu sprawdź brak starego urządzenia, epoch, jednorazowość TOTP i nowy login z hasłem oraz aktywnym drugim czynnikiem. Nie ujawniaj sekretów, pełnych katalogów ani kopii SQL w GitHub Actions.
- **Nie uznawaj wszystkich czterech patchy za gotowe produkcyjnie.** Aktywacja 0.9.6 jest jawnie wstrzymana przez wymaganie osobnej akceptacji Ownera. Green CI z testów projektowych tej blokady nie usuwa.

CI jest uruchamiane wyłącznie na GitHub-hosted Ubuntu 22.04/24.04/26.04; wyniki wymagają kontroli dla dokładnego SHA po zakończeniu. Zgodnie z poleceniem po push/uruchomieniu CI kończymy bez czekania. Bez merge do `main`, wydania, migracji lub deploymentu.
