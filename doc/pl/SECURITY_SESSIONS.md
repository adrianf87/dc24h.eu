# Sesje bezpieczeństwa — alpha-0.8.15

Dostęp utrzymaniowy zachowuje cztery granty w pamięci z [API administracyjnego](ADMIN_API.md). Każdy grant ma teraz publiczny identyfikator sesji i osobny sekret CSRF. Ta gałąź powstaje z main `6175458`. Nazwa gałęzi to `alpha-0.8.15`.

Schemat v9 pozostaje bez zmian. Ten etap nic nie zapisuje do SQL ani do tabeli audytu. `configuration.manage` i `hub.manage` pozostają niewdrożone. Przeglądarka nadal nie wysyła kicka ani wyciszenia. Hub nadal nie ufa nagłówkom proxy ani `X-Forwarded-For`.

## Co zwraca dowód

`POST /api/v1/admin/access` nadal wymaga aktywnego konta i jego hasła. JSON dodaje `session` i `csrf` obok istniejącego `grant`.

- `grant` to 64 małe znaki szesnastkowe. Nadal jest poświadczeniem kolejnych wywołań utrzymaniowych.
- `session` to 32 małe znaki szesnastkowe. Nazywa slot. Nie jest poświadczeniem.
- `csrf` to 64 małe znaki szesnastkowe. To drugi sekret, inny niż grant.
- `expires_in` pozostaje 600. Czas życia na zegarze monotonicznym jest wspólny. Restart usuwa slot. Nowy dowód tego samego aktora zastępuje jego slot, łącznie z identyfikatorem sesji i sekretem CSRF. Pełna tabela nadal zastępuje najbliższe wygaśnięcie.

Dowód hasłem nie wymaga nagłówka CSRF. Ta odpowiedź jest jedynym ciałem utrzymaniowym, które zawiera te sekrety.

## Zapisy

Każdy inny POST utrzymaniowy wymaga trzech nagłówków z tego samego slotu:

- `X-DC24H-Admin-Grant`: 64 znaki szesnastkowe
- `X-DC24H-Session`: 32 małe znaki szesnastkowe
- `X-DC24H-Csrf`: 64 znaki szesnastkowe

Grant jest porównywany w czasie stałym. Sekret CSRF jest potem porównywany w czasie stałym z tym samym slotem. Brakujący albo inny identyfikator sesji lub sekret CSRF zwraca 403 `csrf_required`. Odczyty (`GET`) nadal wymagają tylko grantu i nie wymagają sekretu CSRF. Nagłówek o złym kształcie jest błędnym żądaniem, zanim kod zajrzy do slotu.

WebAdmin trzyma grant, identyfikator sesji i sekret CSRF w pamięci strony. Nie zapisuje ich do `localStorage`, `sessionStorage` ani ciasteczka. Zapis wysyła nagłówki sesji i CSRF. Odczyt wysyła tylko grant. Zapomnienie czyści wszystkie trzy naraz.

Adapter loopback przekazuje poprawnie ukształtowaną sesję i sekret CSRF. Każdy inny zapis utrzymaniowy jest odrzucany z `csrf_required` przed gniazdem Unix. Wyjątkiem jest dowód dostępu. Adapter nadal wstawia własny bearer usługi i nadal ignoruje przeglądarkowy nagłówek `Authorization`. Tych sekretów nie zapisuje w dzienniku.

[Przykład proxy Apache](APACHE2_PROXY.md) przekazuje sesję tylko wtedy, gdy ma 32 małe znaki szesnastkowe, a sekret CSRF tylko wtedy, gdy ma 64 znaki szesnastkowe. Każda inna wartość jest usuwana. Przykład nie zawiera grantu ani sekretu CSRF. `Require local` zostaje.

## Limity żądań

Działają dwa limity naraz.

- Proces nadal przyjmuje co najwyżej jeden POST utrzymaniowy na sekundę. Szybszy POST zwraca 429 `rate_limited`.
- Każda żywa sesja przyjmuje 24 żądania w ciągu 10 sekund. Licznik obejmuje odczyty i zapisy, które przedstawiają ten grant. Kolejne żądanie w oknie zwraca 429 `rate_limited`. Brakujący grant nie zużywa budżetu. Zastąpienie sesji zaczyna nowy budżet.

Gniazdo Unix nie ma adresu IP klienta. Budżet nie pochodzi z `X-Forwarded-For` ani z żadnego innego nagłówka proxy. To nie jest ochrona huba przed odmową usługi. Status i health nadal wymagają tylko bearer i nie należą do sesji utrzymaniowej.

## Czego ten etap nie robi

Nie ma sesji w ciasteczku, sesji w pamięci przeglądarki ani publicznego zdalnego logowania administratora. HTTP/2, OCSP, automatyczne odnawianie certyfikatu i produkcyjny audyt tych sprawdzeń nie są wdrożone. Usunięcie `Require local` nadal pozwoliłoby zdalnemu klientowi użyć tokena usługi adaptera. Nie należy tego robić.
