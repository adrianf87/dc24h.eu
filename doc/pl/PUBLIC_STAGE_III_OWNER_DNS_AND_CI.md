# Etap III — oświadczenie OWNER dotyczące DNS i dowody kwalifikacji CI

**Data zapisu:** 27 listopada 2026 r. (Europe/Warsaw).  
**Zadanie:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) i [PR #24](https://github.com/adrianf87/dc24h.eu-twilight/pull/24).  
**Publiczna usługa dla gości:** `adcs://server-747585786.dc24h.eu:1512`, IPv4 `62.171.156.32`.  
**Wynik dowodu kontroli strefy:** **POTWIERDZONA KONTROLA DNS — PASS** na podstawie nowego rekordu TXT odczytanego z dwóch niezależnych publicznych resolverów. Tożsamość abonenta domeny pozostaje **OWNER-ATTESTED** na podstawie prywatnych zrzutów ekranu rejestratora. Weryfikacja A/PTR należała do osobnego historycznego zestawu publicznego CI.

## Zmiana zakresu zakończenia etapu III przez OWNER — 27 listopada 2026 r.

OWNER **wycofał z warunków akceptacji etapu III**: niezależny publiczny test DNS A/PTR lub ADCS dla najnowszego SHA, interaktywny test prawdziwym klientem DC++ oraz dodatkowe sprawdzenie zapory, portów i trasowania Contabo. **Nie wykonuj ani nie uruchamiaj tych testów. Nie oznaczaj ich jako PASS.** Zachowaj dotychczasowe wyniki jedynie jako dowody historyczne. Jednorazowo potwierdzony rekord TXT nie oznacza konieczności kolejnego testu sieciowego.

Wycofany pozostaje również kontrolowany restart VPS oraz restart produkcyjnej usługi ADCS. **Formalna akceptacja `OWNER STAGE III ACCEPTANCE GO` i odrębna zgoda `OWNER MERGE GO` nadal są obowiązkowe.** Aktualnie oczekujemy tylko na wymagane CI na GitHub-hosted Ubuntu oraz przegląd jego wyników dla dokładnego SHA przed tymi dwiema decyzjami. Ten dokument nie upoważnia do scalenia.

## Oświadczenie OWNER i dostarczone obserwacje

Właściciel repozytorium (`adrianf87`) oświadcza, że domena `dc24h.eu` należy do niego. Prywatnie udostępnione zrzuty z panelu rejestratora oraz zarządzania strefą DNS wskazują domenę w koncie OWNER i nowy rekord TXT. Są to dowody przedstawione przez OWNER, **nie niezależna kontrola tożsamości prawnej abonenta**. Dane kontaktowe, numery kont i identyfikatory prywatne nie są publikowane.

1. **Zrzut terminala od OWNER:** polecenie odczytowe `host 62.171.156.32` pokazało rekord PTR prowadzący do `server-747585786.dc24h.eu`, a `host server-747585786.dc24h.eu` zwróciło adres A `62.171.156.32`.
2. **Zrzut panelu Contabo od OWNER:** sekcja **Network → DNS Management → Reverse DNS Management** pokazała w jednym wierszu `62.171.156.32` → `server-747585786.dc24h.eu`. To widok zarządzania odwrotnym DNS, a nie potwierdzenie administracji strefą forward ani dowód prawny abonamentu.
3. Wiersz poleceń widoczny na zrzucie wskazuje **wycofany serwer** `server-759505093`. Był on jedynie miejscem wykonania odczytowych poleceń DNS. **Nie jest celem wdrożenia** i nie wolno na nim wykonywać czynności wdrożeniowych.

Oryginalne zrzuty przekazano w rozmowie z OWNER. Nie umieszczono ich w publicznym repozytorium ani w niezależnie sumowanych artefaktach GitHub Actions. Dokument zapisuje konkretne ustalenia i ograniczenia dowodów bez ujawniania sesji panelu usługodawcy.

## Niezależne wyzwanie TXT — PASS (26 listopada 2026 r., 23:39:32 UTC)

OWNER utworzył nowy rekord TXT w zarządzanej strefie `dc24h.eu`. Wcześniejszy wynik terminala podczas przetwarzania zmiany wskazywał stary alias/SPF i **nie został zaliczony**. Późniejszy zrzut terminala OWNER zawierał już poprawną wartość TXT.

Zapytano dwa niezależne publiczne serwery rekurencyjne przez HTTPS:

| Pole | Potwierdzona wartość |
| --- | --- |
| Zapytanie | `_dc24h-stage3-verify.dc24h.eu` TXT |
| Oczekiwany token | `dc24h-stage3-owner=f9defa5ca27e018bec03296bc11f11c15324600c` |
| Google Public DNS | `Status: 0`; dokładny token; TTL 300 — **PASS** |
| Cloudflare DNS | `Status: 0`; dokładny token; TTL 300 — **PASS** |
| Późniejszy zrzut `host -t TXT` od OWNER | Zwrócono dokładny token — **PASS** |

Odpowiedź Cloudflare zawierała cudzysłowy formatu TXT, ale token ma tę samą wartość. To niezależnie zaobserwowany dowód możliwości publikacji rekordu pod `dc24h.eu` w chwili weryfikacji. **Nie potwierdza niezależnie prawnej tożsamości abonenta.** Jednorazowe wyzwanie nie może stać się stałą zależnością CI. Podczas tej weryfikacji nie edytowano DNS przez CI, nie restartowano usługi ani nie modyfikowano VPS.

## Granice dowodu: rekordy DNS a własność domeny

- **Dowody dostarczone przez OWNER:** forward DNS wskazanego hosta i reverse DNS wskazanego IP są spójne. OWNER pokazał także rekord PTR w swoim panelu Contabo i oświadczył, że jest właścicielem domeny.
- **Niezależnie potwierdzono:** poprawny, świeżo opublikowany token TXT wykazuje techniczną kontrolę wpisu w strefie `dc24h.eu`. Tożsamość abonenta pozostaje OWNER-ATTESTED. Nie zmieniano istniejącego rekordu A; uprawnienia zapory operatora nie wynikają z tego testu.
- **Historyczne możliwości rozszerzonej kontroli (obecnie wycofane z akceptacji etapu III):** porównanie autorytatywnych NS/A, ruchu przychodzącego i wyjątków portów. Nie uruchamiać tych testów z powodu niniejszej kwalifikacji. Nie publikować numerów kont, danych osobowych, rozliczeniowych, haseł ani cookies.
- **Oznaczenia dowodowe:** `DNS ZONE CONTROL VERIFIED — PASS` dotyczy wyłącznie TXT; `OWNER-ATTESTED` dotyczy tożsamości abonenta; `DNS FORWARD-REVERSE VERIFIED` wymaga osobnej historycznej obserwacji A/PTR i nie jest nowym wymogiem obecnego SHA.

## Historycznie dodane automatyczne regresje DNS i publiczna kwalifikacja

Istniejąca grupa `tls-public` zawierała następujące przypadki:

| Przypadek | Oczekiwany rezultat | Historyczny zakres wykonania |
| --- | --- | --- |
| `dns.a` | Wskazana FQDN rozwiązuje się wyłącznie na `62.171.156.32` | Niezależny publiczny runner Ubuntu |
| `dns.ptr.forward-confirmed` | PTR dla `62.171.156.32` wskazuje `server-747585786.dc24h.eu`, a ten host rozwiązuje się tylko na zatwierdzony IPv4 | Niezależny publiczny runner Ubuntu |
| `tls.identity` / `tls.fingerprint` | Publicznie zaufany CA, dokładna SAN/SNI i spójny odcisk certyfikatu | Niezależny publiczny runner Ubuntu |
| `exposure` | Dostępne są wyłącznie zatwierdzone publiczne porty i jawne wyjątki w ograniczonym skanowaniu TCP | Niezależny publiczny runner Ubuntu |

Przypadek `dns.ptr.forward-confirmed` zgłaszał błąd przy rozbieżnym PTR, złym adresie forward lub niejednoznacznym DNS. Izolowane regresje obejmują wariant poprawny, błędny i niedostępny. Odczyt DNS nie modyfikuje strefy ani VPS. Spójny PTR nadal nie dowodzi prawnej własności domeny.

W pull requestach zewnętrzne grupy publiczne są celowo **poza zakresem / NOT TESTED**. OWNER usunął obowiązek uruchamiania nowego snapshotu jako warunek zakończenia etapu III. **Nie uruchamiać `RUN_STAGE3_UBUNTU_EXTERNAL_ONLY` w ramach tej kwalifikacji.** Kod testów historycznych może pozostać w repozytorium, ale nie jest bramką akceptacji PR #24.

## Lista CI i akceptacji — dowody przypisane do dokładnego SHA

- [ ] **Aktualny head PR:** sprawdzić zakończone GitHub Actions na rzeczywistym bieżącym SHA, nie na starszym `0863f1bf820e34f64e7df0076fe51f98827ca81d`. Wyniki starszego commita nie są dowodem bieżącego stanu.
- [ ] **Izolowana macierz Ubuntu:** pełny CTest oraz izolowane fikstury Stage III dla dwóch klientów, TLS, stabilności i systemd na GitHub-hosted Ubuntu 22.04, 24.04 i 26.04. Test systemd na tymczasowym runnerze Ubuntu nie jest restartem produkcyjnego ADCS.
- [ ] **Pozostałe wymagane CI:** C++ Debug/Release, WebAdmin, ASan/UBSan, reguły bazowe, CodeQL, parser/fuzz oraz odpowiednie testy etapu IV mają zakończyć się pomyślnie; nie wolno ukrywać nieprzejrzanych błędów.
- [x] **Kontrola rekordu DNS:** świeży token TXT OWNER został odczytany przez Google DNS i Cloudflare DNS i potwierdzony później na zrzucie terminala.
- [ ] **Pochodzenie historycznego dowodu stabilności:** wcześniej zatwierdzony przez OWNER 30-minutowy test publiczny miał PASS na `7c961238066e3de2bef5ab9303fd3945a4b85157`. Zachować dokładny SHA i tożsamość binarki; nie przypisywać go nowemu commitowi. Jasno ustalić, czy historyczny wynik wystarcza przy niezmienionej produkcyjnej binarce.
- [ ] **Zgody OWNER pozostają:** po sprawdzeniu bieżącego CI, bez nowych zewnętrznych i ręcznych testów, zapisać wycofane przypadki jako `OUT OF SCOPE BY OWNER`, nigdy PASS. Uzyskać odrębnie formalne `OWNER STAGE III ACCEPTANCE GO` i `OWNER MERGE GO` przed scaleniem; PR #24 i Issue #10 w tym czasie pozostają otwarte.

### Historyczne CI — nie stanowi wyniku dla kolejnych commitów

Dla źródła `0863f1bf820e34f64e7df0076fe51f98827ca81d` pomyślnie zakończono [kwalifikację Stage III](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397299), [C++/WebAdmin z UBSan](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397330), [CodeQL](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397313), [TSAN/fuzz](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397306) i [audyt offline Stage IV](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397323). [Izolowana maszyna Ubuntu Stage IV](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/45909397303) w chwili dokumentowania nadal działała. Publiczne zadania zewnętrzne na PR są pomijane zgodnie z projektem. Nowe commity dokumentacji/kodu wymagają własnego bieżącego CI.

## Wyraźne wyłączenie OWNER: brak testu restartu produkcyjnego

Zgodnie z decyzją OWNER z 26 listopada **nie restartować kontrolowanie VPS Contabo ani celowo nie restartować działającej usługi `dc24h-stage3-adcs.service` na potrzeby kwalifikacji etapu III**. Te wycofane testy nie są wymagane, nie mogą blokować etapu i nie wolno oznaczać ich jako PASS. Przyszła wymiana certyfikatu lub obsługa awarii, gdy wymaga restartu, jest osobną zmianą operacyjną wymagającą odrębnej zgody OWNER. Niniejsza aktualizacja dowodów nie zmieniała usługi, VPS, DNS, zapory ani materiału TLS.
