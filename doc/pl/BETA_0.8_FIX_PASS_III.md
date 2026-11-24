# Beta 0.8.x — Fix Pass 3, przegląd Etapu III (0.8.9–0.8.12)

**Zakres:** jednostka pracy 8 z [Issue #240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240). Ta sama gałąź `beta-0.8.x-stage-full`, jeden szkic PR #241, bez scalenia do chronionego `main`. Zachowujemy Etapy IV i V, nie dodajemy uprawnień ani nie zmieniamy schematu MariaDB v11 / formatu pięciu plików.

## Potwierdzone poprawki

1. **Podmiana ścieżki podczas kopii.** Pliki prywatne, wejście SQL/manifestu i rejestr zużytych manifestów są otwierane względem sprawdzonego deskryptora katalogu (`dir_fd`). Narzędzie sprawdza dwukrotnie wszystkie katalogi nadrzędne oraz tożsamość inode. Zmiana źródła nawet na plik o takim samym rozmiarze i treści podczas odczytu jest odrzucana. Zapis do innego katalogu wskutek podmiany dowiązania także jest blokowany.
2. **Niekanoniczne aliasy Node.** Odrzucone są dodatkowe ukośniki, końcowy ukośnik oraz `.`/`..` w katalogu prywatnym. Nadal trzeba zatrzymać procesy zapisujące przed odtwarzaniem; to nie jest atomowy mechanizm jądra `openat2`.
3. **Autentyczność i replay.** Zewnętrzny klucz HMAC i niezależnie zachowany rejestr pozostają poza backupem. Zmiana inode rejestru przed zaakceptowaniem kopii powoduje odmowę, bez dopisania zużycia lub resetowania historii.
4. **Gateway bez `tsx`.** Poprawiono wyścig w skrypcie CI przy szybkim zakończeniu procesu po SIGTERM. Wymagany pozostaje rozruch skompilowanego JS tylko z zależnościami produkcyjnymi na GitHub-hosted Ubuntu.
5. **Pakiet `.deb`.** Pozostają kontrolowane awarie `preinst` i `postinst`, test rzeczywistego downgrade i weryfikacja starej binarki. Wersja fikcyjnej awarii po rozpakowaniu po Etapie V to `0.8.20+ci3`.

## Granice nieudowodnione — nie zatwierdzać jako ukończone

- Weryfikacja SQL v11 i stanu TOTP/replay/passkey po downgrade **nie jest pełnym logowaniem hasło+TOTP ani hasło+passkey przez poprzednią binarkę**. Ten warunek akceptacji nadal wymaga osobnego rzeczywistego testu historycznego C++ z bramą HTTP/przeglądarką.
- Rozruch procesu gatewaya w CI nie oznacza instalacji i restartu przykładowej jednostki systemd. Nie ma zgody na instalację produkcyjną.
- Blokada SQL nie zatrzymuje zapisów w plikach Node. Do odzyskiwania należy zatrzymać wszystkie procesy zapisujące, wykonać wspólną kopię **MariaDB + pięć plików**, zachować oddzielny klucz i rejestr HMAC, sprawdzić kompletny manifest i przywracać do nowych katalogów.
- Nie ma gwarancji RPO/RTO, atomowej kopii na żywo, ochrony przed wrogimi jednoczesnymi zmianami katalogów poza izolacją ani certyfikacji fizycznych passkeys.

## Kontrola i przekazanie

Nowe testy negatywne: `tests/beta08_fix_pass3_tests.py` i `webadmin/tests/remote-private-init.test.ts`. Uruchamiaj wyłącznie GitHub-hosted Ubuntu **22.04/24.04/26.04** dla pełnej macierzy C++/Node/Apache/MariaDB/pakiet/Chromium/CodeQL. Dokładny commit SHA i odnośnik do nowego CI trafiają do PR #241. `pending` ani anulowany poprzedni przebieg nie oznacza sukcesu. Po push i potwierdzeniu uruchomienia CI zakończyć bez oczekiwania. Bez Debian, prywatnych runnerów, Xeon, merge, tagu, publikacji pakietu i wdrożenia.
