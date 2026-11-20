# Odwrotne proxy HTTPS Apache2 — aktualna lista beta-0.1.4

Dostarczany [przykład proxy](../../config/apache2-webadmin-https-proxy.conf.example) jest osobną witryną HTTPS wobec [przykładu statycznego HTTPS](APACHE2_HTTPS.md). Zachowuje TLS 1.2/1.3, stałe przekierowanie HTTP na HTTPS, dostęp lokalny, blokadę plików sekretów i te same nagłówki ochronne. Dodatkowo przekazuje dozwoloną listę API WebAdmin do adaptera HTTP na loopbacku.

Ten plik nie aktywuje witryny hosta, nie uzyskuje certyfikatu, nie uruchamia adaptera, nie przeładowuje Apache i nie zmienia huba C++. Przykład statyczny nadal blokuje `/admin-api` i `/api`. Hub nadal nie ufa nagłówkom proxy przy przyjmowaniu połączeń ani w limitach tempa.

## Co jest chronione

- Port HTTP 80 tylko przekierowuje na stały adres `https://dc24h.eu/`. Nie prowadzi proxy.
- `ProxyRequests Off` wyłącza użycie jako proxy przekazujące. `TraceEnable Off` wyłącza TRACE.
- Upstream to `http://127.0.0.1:4173`. Podmień ten port na adapter loopback. Nie kieruj go na gniazdo Unix C++ i nie wstawiaj do pliku tokena usługi ani grantu administratora. Adapter sam dodaje bearer.
- Nagłówki klienta `Authorization`, `Cookie`, `Forwarded`, `Proxy` oraz `X-Forwarded-*` / `X-Real-IP` są usuwane. `ProxyPreserveHost` i `ProxyAddHeaders` pozostają wyłączone, więc adapter widzi `Host: 127.0.0.1:4173`, a nie nazwę podaną przez klienta.
- Kanoniczne `Origin: https://dc24h.eu` jest przepisywane na `http://127.0.0.1:4173`, czyli origin, który adapter porównuje z tym Host. Każdy inny origin jest odrzucany przed proxy. Odczyt może pominąć Origin. Zapis nie może.
- `X-DC24H-Admin` jest przekazywany tylko wtedy, gdy wartością jest dokładnie `1`. `X-DC24H-Admin-Grant` jest przekazywany tylko wtedy, gdy ma 64 znaki szesnastkowe. `X-DC24H-Session` jest przekazywany tylko wtedy, gdy ma 32 małe znaki szesnastkowe. `X-DC24H-Csrf` jest przekazywany tylko wtedy, gdy ma 64 znaki szesnastkowe. Apache żadnego z tych nagłówków nie wymyśla. Odczyt albo zapis utrzymaniowy nadal musi spełnić sprawdzenie sesji opisane w [Bezpieczeństwie API](API_SECURITY.md).
- `Sec-Fetch-Site` musi być nieobecny, równy `same-origin` albo `none`. `cross-site` jest odrzucany.
- `Host` API musi być dokładnie `dc24h.eu`. Kawałkowe `Transfer-Encoding` jest odrzucane. Treść API jest ograniczona do 7000 bajtów. Odpowiedzi proxy mają `Cache-Control: no-store`.
- Ścieżki spoza listy, w tym `/api/`, nieznane `/admin-api/` i zła metoda, są odrzucane i nie trafiają do proxy. Ukryte pliki, sufiksy sekretów i symlinki pozostają zablokowane.

## Lista dozwolona

GET z pustym body: `/admin-api/v1/status`, `/health`, `/users`, `/admin`, `/management`, `/management/configuration`, `/accounts`, `/accounts/{id}` i `/bans`. `{id}` to zapis dziesiętny od 1 do 19 cyfr. Odczyt konfiguracji wymaga istniejącego uprawnienia `configuration.manage`; pozostałe odczyty utrzymaniowe zachowują własne kontrole uprawnień. Istniejące filtry list kont i banów mogą używać opisanych parametrów zapytania; ścieżki zarządzania i konfiguracji wymagają dokładnych celów bez parametrów query.

POST z `Content-Type: application/x-www-form-urlencoded`, `X-DC24H-Admin: 1`, kanonicznym origin i treścią od 1 do 7000 bajtów: `/admin-api/v1/admin/access`, `/audit`, `/management/authorize`, `/management/configuration`, `/management/configuration/write`, `/management/configuration/restore`, `/accounts`, `/accounts/{id}/{action}`, `/bans` i `/bans/{id}/revoke`. `{action}` to `activate`, `disable`, `reset`, `password` albo `role`. Wszystkie ścieżki w tym akapicie mają prefiks `/admin-api/v1`.

`/accounts/{id}` bez akcji jest odczytem, nie zapisem. `/bans/{id}` bez `/revoke` nie jest przekazywane. Proces huba nadal publikuje tylko health, status i users. Trasy kont, banów, audytu, odkrywania i zarządzania pozostają w procesie utrzymania. Status i health wewnątrz adaptera nadal wymagają bearer. Trasy konfiguracji udostępniają tylko cztery dozwolone ustawienia bez sekretów i wymagają grantu Administratora/Ownera, jego sesji i sekretu CSRF; atomowy zapis wymaga późniejszego kontrolowanego restartu huba. Starszy kontrakt `/management/authorize` nadal zwraca `501` dla dozwolonych operacji i nigdy nie wykonuje poleceń usługi. Ten przykład nie dodaje sesji w ciasteczku, zdalnego dostępu administratorów, uruchamiania/zatrzymywania/restartowania huba ani kick/mute w przeglądarce. Sesje utrzymaniowe w pamięci strony opisują [Sesje bezpieczeństwa](SECURITY_SESSIONS.md) i nadal wymagają `Require local`.

`Require local` zostaje na katalogu statycznym i na bramie API. Usunięcie go pozwoliłoby zdalnemu klientowi użyć tokena usługi adaptera, bo adapter traktuje połączenie Apache jako loopback. Nie rób tego. Publiczna uwierzytelniona brama pozostaje osobnym zadaniem.

Przykład nie konfiguruje HTTP/2, OCSP stapling, automatycznego odnawiania ani produkcyjnego audytu. `apache2ctl configtest` nie dowodzi zgodności certyfikatu z kluczem; regresja proxy uruchamia Apache z niezgodną parą i oczekuje błędu. Witryna hosta, magazyn zaufania i usługa nie są zmieniane.

## Dostosowanie

Zainstaluj przykład obok szablonu statycznego, włącz `ssl`, `headers`, `proxy` i `proxy_http`, a następnie popraw nazwę hosta, ścieżkę zasobów, ścieżki certyfikatu i wszystkie trzy wystąpienia `127.0.0.1:4173`. Adapter musi już nasłuchiwać na tym porcie loopback i być jedynym procesem, który czyta token usługi. Zostaw `Require local`.

```sh
ctest --test-dir build -R '^apache2.https.proxy$' --output-on-failure
```

Test ładuje dostarczany plik pod tymczasowym ServerRoot, na efemerycznych portach loopback, z jednorazowym certyfikatem i jednorazowym upstreamem. Podmieniane są tylko ścieżki, porty i adres upstreamu. Sprawdzana jest lista dozwolona, usuwanie nagłówków, przepisanie origin, blokada `/api` i nieznanych tras, limity treści, odrzucenie starego TLS i start z niezgodnym kluczem. Nie jest to zewnętrzny skan ani wdrożenie produkcyjne.
