# Bezpieczeństwo API — alpha-0.9.7

Uprawnienia, sesje, CSRF i WebAdmin. Gałąź powstaje z main `14792f7`. Nazwa gałęzi to `alpha-0.9.7`.

Proces utrzymaniowy już przeładowuje zapisaną rolę przy każdym przyjętym wywołaniu. Każdy grant w pamięci ma już publiczny identyfikator sesji i osobny sekret CSRF, a zapis już wymagał obu. Ten przyrost dodaje jedną stałą regułę. Nie jest opcjonalna i nie jest kluczem INI.

## Związane odczyty

Każde wywołanie utrzymaniowe poza `POST /api/v1/admin/access` wymaga trzech wartości z tego samego slotu:

- `X-DC24H-Admin-Grant`: 64 znaki szesnastkowe
- `X-DC24H-Session`: 32 małe znaki szesnastkowe
- `X-DC24H-Csrf`: 64 znaki szesnastkowe

Grant, identyfikator sesji i sekret CSRF są porównywane w czasie stałym. Brakujący albo inny identyfikator sesji lub sekret CSRF zwraca 403 `csrf_required`. Ta odmowa nie przeładowuje roli i nie zwraca wierszy utrzymaniowych. Nagłówek o złym kształcie nadal jest błędnym żądaniem, zanim kod zajrzy do slotu. Dowód hasła nie wymaga tych nagłówków, bo ta odpowiedź wydaje te trzy sekrety.

Adapter loopback stosuje tę samą regułę, zanim otworzy gniazdo Unix. Odczyt albo zapis, który podaje grant bez obu wartości, jest odrzucany z `csrf_required` i nie jest przekazywany. Odczyt bez grantu nadal jest przekazywany, żeby proces mógł odpowiedzieć `admin_grant_required`. Dowód dostępu pozostaje wyjątkiem. Adapter nadal wstawia własny bearer usługi i nadal ignoruje przeglądarkowy nagłówek `Authorization`. Tych sekretów nie zapisuje w dzienniku.

WebAdmin wysyła wszystkie trzy nagłówki przy każdym wywołaniu utrzymaniowym. Zostają w pamięci strony. Nie są zapisywane do `localStorage`, `sessionStorage` ani ciasteczka. Zapomnienie nadal czyści wszystkie trzy naraz.

Status i health nadal wymagają tylko bearer. Każda żywa sesja nadal przyjmuje 24 żądania w ciągu 10 sekund, a odmowa wiązania nadal się liczy. Kolejne żądanie w tym oknie zwraca 429 `rate_limited`. Proces nadal przyjmuje co najwyżej jeden POST utrzymaniowy na sekundę. Budżet nie pochodzi z adresu klienta ani z nagłówka proxy.

## Uprawnienia

Zapisana rola jest ładowana dopiero po zgodności wiązania. Aktywne konto, które nie ma już uprawnienia trasy, nadal jest zabronione. Nieznane cele utrzymaniowe pozostają zabronione. `configuration.manage` i `hub.manage` pozostają niezaimplementowane i nie mają trasy. Przeglądarka nadal nie wysyła kick ani mute. Katalog uprawnień i dwanaście tras utrzymaniowych pozostają bez zmian.

## Bez zmian

Skopiowanie grantu, identyfikatora sesji i sekretu CSRF z pamięci strony nadal autoryzuje ten slot do wygaśnięcia albo do wymiany przez aktora. Odpowiedź dowodu nadal niesie te trzy sekrety. To nie zamyka zagrożeń szczątkowych T04 ani T05. Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, nowej strony WebAdmin, klucza INI ani ustawienia Apache. `Require local` zostaje. Nie ma sesji w ciasteczku ani publicznej uwierzytelnionej bramy. HTTP/2, OCSP i automatyczne odnawianie certyfikatu pozostają niezaimplementowane.

`accounts.webadmin.process` sprawdza, że grant bez wiązania sesji zwraca `csrf_required` i żadnych wierszy konta, że zły identyfikator sesji robi to samo, że źle ukształtowany nagłówek CSRF jest błędnym żądaniem i że związany odczyt nadal wymienia konta. Test adaptera sprawdza, że podany grant bez wiązania nie dochodzi do gniazda Unix.
