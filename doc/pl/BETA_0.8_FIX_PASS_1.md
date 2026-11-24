# Beta-0.8.x — Fix Pass 1, poprawki Etapu I (`0.8.1–0.8.4`)

Zadanie [#240](https://github.com/adrianf87/dc24h.eu-high-starter/issues/240), wspólna gałąź `beta-0.8.x-stage-full`, draft [PR #241](https://github.com/adrianf87/dc24h.eu-high-starter/pull/241). Prace nie dotyczą Etapów II–V, nie są zgodą na wdrożenie i nie upoważniają do scalenia do `main`.

## Potwierdzony problem w testach

GitHub Actions [45465183203](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45465183203) zakończyły sześć zadań C++ niepowodzeniem na `accounts.adc_authentication.process`. Test odebrania dostępu szukał w SQL nazwy konta `RegisteredADC`, ale jest to **nick ADC** powiązany z kontem `AuthAccountOne` w odizolowanej bazie. Zmiana dotyczyła **zera** rekordów. Po restarcie konto słusznie nadal mogło się zalogować.

Poprawka:

1. Ustalenie konta przez tabelę tożsamości `dc24h_account_identities` i unikalny nick ADC.
2. Wymaganie zmiany dokładnie jednego wiersza przy odebraniu i przywróceniu statusu.
3. Odczyt rzeczywistego statusu po zmianie i przerwanie testu, jeśli nie jest zgodny.
4. Kontrola rozłączenia starej sesji oraz odmowy na ADC i ADCS po **kontrolowanym restarcie huba**.
5. Przywrócenie fikcyjnego konta do stanu aktywnego po teście; żadna baza produkcyjna nie jest modyfikowana.

Nie implementowano automatycznego odbierania uprawnień na żywo. Obecny serwer C++ odczytuje uprawnienia podczas uruchamiania i wymaga restartu po zmianie.

## Pozostałe kontrole

- **0.8.1:** niezależny `ncdc` musi mieć prawdziwą wersję i skróty SHA-256; fikcyjne raporty DC++/AirDC++, pominięcia, błędy i duplikaty są odrzucane. Dodano test odmowy także dla błędu ukrytego pod `status="run"`.
- **0.8.3:** przejrzano ochronę tokenów, TTH/SCH/RES, CTM/RCM, odmowę niewłaściwego odbiorcy i utrzymanie połączeń po błędach. Nie potwierdzono nowej usterki w kodzie serwera.
- **0.8.4:** po wymianie certyfikatu ADCS połączenie nieszyfrowane może zakończyć się tylko EOF/resetem albo poprawnym fatalnym alertem TLS. Dowolna odpowiedź aplikacyjna nie może przejść testu.

**Testy:** wymagane są rzeczywiste zadania C++, WebAdmin, CodeQL, sanitizery i VM na tym samym SHA na GitHub-hosted Ubuntu 22.04/24.04/26.04. Bez Debian, prywatnych runnerów i serwera produkcyjnego. Po wypchnięciu zmian oraz uruchomieniu CI **zakończyć działanie bez czekania na wyniki**.

**Stan:** poprawki przygotowane; wyniki nowego CI niezweryfikowane, PR pozostaje Draft. Dalsze Fix Passy wymagają osobnych poleceń właściciela.
