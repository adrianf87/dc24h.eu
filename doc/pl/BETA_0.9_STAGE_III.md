# beta-0.9.x — Etap III (0.9.9–0.9.12): stara autoryzacja, dirfd, restore, systemd

Status: **implementacja do przeglądu; wynik hostowanego CI nie jest jeszcze
znany ani zatwierdzony**. Zgłoszenie Ownera:
[#242](https://github.com/adrianf87/dc24h.eu-high-starter/issues/242).
Jedna gałąź `beta-0.9.x-stage-full`, istniejący szkic PR #243 do chronionego
`main`. Ten etap nie obejmuje Etapu IV, mergowania ani wydania.

## 0.9.9 — Rzeczywiście zainstalowany historyczny C++

Na jednorazowych runnerach Ubuntu 22.04/24.04/26.04 kompilujemy osobno
`dc24h` i `dc24h-account-admin` z przypiętego źródła beta-0.5.16. Oba pliki
są w testowej paczce dpkg. Przeprowadzamy rzeczywistą instalację starej
wersji, aktualizację, nieudaną aktualizację przed i po rozpakowaniu oraz
**downgrade do starej paczki**. Sprawdzamy bazę dpkg, sumę SHA-256 i wersję
zainstalowanych binariów oraz brak aktywacji testowej jednostki.

Po odtworzeniu osobnej MariaDB v11 i pięciu prywatnych plików (para sześciu
składników, manifest, niezależny HMAC, księga jednokrotnego użycia) wykonujemy
**drugi, niezależny test odtworzenia**. Wykorzystuje on program
`dc24h-account-admin` *wprost z katalogu zainstalowanej starej paczki*,
rzeczywisty Node, HTTPS/Apache i Chromium z wirtualnym WebAuthn. Weryfikuje
logowanie hasło+TOTP, hasło+passkey, granty, odmowę starej sesji, ponowne
logowanie po restore i odmowę cofniętego klucza po restarcie. Nie zastępujemy
starego C++ aktualnym programem administracyjnym. Niezgodny format/protokół
to błąd CI, nie sukces zastępczy. Pary SQL/prywatnych plików obu prób są
oddzielnymi danymi testowymi.

## 0.9.10 — Prywatne pliki Node przez deskryptor katalogu

Otwieranie, inspekcja, podmiana i fsync plików MFA, passkey, audytu i limitów
działają przez przypięty deskryptor katalogu w Linuksie:
`/proc/self/fd/<dirfd>/<nazwa>`, `O_DIRECTORY`, `O_NOFOLLOW`, sprawdzanie
tożsamości katalogu przed i po operacji, własność oraz prawa `0700`/`0600`.
Test celowo zamienia katalog nadrzędny na symlink w trakcie operacji;
zamiennik nie może zostać otwarty, a wykryta podmiana kończy się odmową.
To technika Linuksa oparta na procfs, **nie** przenośne `openat2` ani pełna
obrona przed złośliwym procesem tego samego UID. Brak obsługi ma kończyć
się odmową (fail closed).

## 0.9.11 — Spójne sześć składników i blokada powtórnego restore

Analiza wierszy migracji SQL v11 używa przypiętego katalogu i wykrywa
podmianę pliku nawet na bajtowo identyczny w trakcie odczytu. Testy obejmują
wszystkie sześć składników, manifest, odrębny klucz HMAC i podpis, księgę
jednorazową, uszkodzony plik replay, inną generację źródeł i pusty katalog
docelowy. Preflight **nie** wykonuje importu SQL: robi to odrębna, izolowana
MariaDB runnera po zatrzymaniu writerów i pełnej autoryzacji pary. Nie
uznajemy surowych plików za samodzielnie uwierzytelnione.

## 0.9.12 — Prawdziwy systemd i dpkg, ale tylko w CI

Stary program administracyjny z paczki jest uruchamiany jako tymczasowa
jednostka systemd, na uprawnieniach runnera, z odizolowaną siecią i wyłącznie
gniazdem Unix; sprawdzamy start, restart, odpowiedź API, stop i właściwości
sandboxu. Skompilowany Node z zależnościami **wyłącznie produkcyjnymi**
przechodzi osobny start/restart/stop w tymczasowej jednostce systemd. Jego
jedyny port TCP musi być przypięty do `127.0.0.1`, pliki stanu są prywatne,
a konfiguracja znajduje się w prywatnym pliku środowiska. Nie uruchamiamy
usług produkcyjnych ani nie włączamy nieaktywnej jednostki paczki CI.

## Wymagane bramki przed dalszym etapem

1. Trzy pełne macierze GitHub-hosted Ubuntu 22.04, 24.04, 26.04 dla
   **dokładnego końcowego SHA**; bez pominiętych/przerwanych testów.
2. Dane `beta07-dpkg-rollback.json`, `beta09-systemd-node.json` oraz
   `beta09-stage3-gate.json`, powiązane z tym samym SHA/obrazem; zero haseł,
   tokenów, ciasteczek, zrzutów prywatnej bazy i surowych logów.
3. Niezależna akceptacja Ownera po inspekcji wyniku; **nie merge**, nie
   publikacja, nie migracja produkcyjna i nie publiczny listener.

Na moment przekazania lokalne regresje przechodzą, natomiast wynik
GitHub-hosted CI jest **oczekujący**, nie „zielony”.
