# Admin API w wersji Alpha 0.1

Hub C++20 udostępnia JSON pod `/api/v1` po uruchomieniu z `--admin-api`. Boost.Beast domyślnie nasłuchuje na `127.0.0.1:8081`. Jeden wątek I/O HTTP i dwa wątki robocze obsługują do 32 połączeń. Odczyt i zapis HTTP mają pięciosekundowe limity. Nagłówki oraz treść żądania są ograniczone do 8 KiB każde. Wątki robocze wykonują zapytania MariaDB i weryfikują hasła. Wątek sterujący huba odczytuje ograniczoną liczbę danych o połączonych użytkownikach na stronę.

## Baza danych i tworzenie kont

Uruchom `dc24h-hub --db migrate` na koncie migracyjnym E5 i sprawdź, czy `--db status` pokazuje `5 / 5`. Wersja 4 tworzy `admin_users` z unikalną nazwą użytkownika, rolą i hashem hasła. Wersja 5 tworzy `admin_sessions` z binarnym skrótem tokenu, odniesieniem do konta oraz czasem utworzenia i wygaśnięcia UTC. Dotychczasowe ustawienia, bany i audyt pozostają dostępne w tej samej bazie. [Konfiguracja MariaDB w E5](MARIADB.md)

Utwórz lokalne konta SQL `dc24h_api` i `dc24h_provision` z osobno wygenerowanymi hasłami. Nadaj kontu API dostęp do poniższych tabel. Konta `dc24h_provision` używaj do lokalnego tworzenia kont administratorów i unieważniania sesji.

```sql
GRANT SELECT ON dc24h.schema_migrations TO 'dc24h_api'@'localhost', 'dc24h_provision'@'localhost';
GRANT SELECT ON dc24h.admin_users TO 'dc24h_api'@'localhost';
GRANT SELECT, INSERT, DELETE ON dc24h.admin_sessions TO 'dc24h_api'@'localhost';
GRANT SELECT, INSERT, DELETE ON dc24h.bans TO 'dc24h_api'@'localhost';
GRANT SELECT, INSERT ON dc24h.audit_events TO 'dc24h_api'@'localhost';
GRANT SELECT (id, username), INSERT ON dc24h.admin_users TO 'dc24h_provision'@'localhost';
GRANT SELECT (admin_user_id), DELETE ON dc24h.admin_sessions TO 'dc24h_provision'@'localhost';
GRANT INSERT ON dc24h.audit_events TO 'dc24h_provision'@'localhost';
```

Aby utworzyć pierwsze konto, użyj osobnych prywatnych plików dla hasła SQL oraz hasła nowego administratora. Poniższe polecenie odczytuje nowe hasło bez wyświetlania go na terminalu.

```bash
export DC24H_DB_NAME=dc24h
export DC24H_DB_USER=dc24h_provision
export DC24H_DB_PASSWORD_FILE=/ścieżka/do/prywatnego/hasła-sql
umask 077
password_file=$(mktemp)
read -rsp 'Hasło nowego administratora: ' password
printf '\n'
printf '%s\n' "$password" > "$password_file"
unset password
dc24h-hub --db admin create operator admin --password-file "$password_file"
unlink "$password_file"
```

Użyj unikalnego hasła od 12 do 128 bajtów. Nazwy użytkowników zawierają od 3 do 48 małych liter ASCII, cyfr, kropek, podkreśleń lub łączników. Dla konta tylko do odczytu wybierz rolę `viewer`. Polecenie `dc24h-hub --db admin revoke operator` usuwa wszystkie bieżące sesje konta. Kolejne logowanie z poprawnym hasłem może utworzyć nową sesję. Konta Admin API są oddzielone od tożsamości klientów ADC GPA/PAS.

## Uruchamianie nasłuchu

Hub korzysta z konta tylko do odczytu określonego przez `DC24H_DB_USER` i `DC24H_DB_PASSWORD_FILE`. API używa `DC24H_API_DB_USER` i `DC24H_API_DB_PASSWORD_FILE` przy tych samych wartościach `DC24H_DB_NAME` i `DC24H_DB_SOCKET`. Pliki haseł mają tryb `0400` lub `0600`. Uruchom usługę na wydzielonym koncie systemowym huba z dostępem do tych plików, certyfikatu ADCS i klucza.

```sh
export DC24H_API_DB_USER=dc24h_api
export DC24H_API_DB_PASSWORD_FILE=/etc/dc24h/secrets/api-db
dc24h-hub --serve --mariadb --admin-api \
  --admin-origin https://hub.example.test \
  --tls-cert /etc/dc24h/tls/fullchain.pem --tls-key /etc/dc24h/tls/key.pem
```

Podaj kanoniczny adres źródła HTTPS przeglądarki bez końcowego ukośnika. Adres może zawierać port inny niż 443. Lokalny port TCP można ustawić przez `--admin-port`. W Apache umieść poniższe dyrektywy w hoście wirtualnym HTTPS, który obsługuje WebAdmin, z włączonymi modułami `mod_proxy` i `mod_proxy_http`.

```apache
ProxyRequests Off
ProxyPass        "/api/v1/" "http://127.0.0.1:8081/api/v1/"
ProxyPassReverse "/api/v1/" "http://127.0.0.1:8081/api/v1/"
```

Zachowaj treść żądań i nagłówki z poświadczeniami poza logami proxy. Przy zapisie przeglądarka przekazuje standardowy nagłówek `Origin`. [Dokumentacja reverse proxy Apache](https://httpd.apache.org/docs/2.4/en/mod/mod_proxy.html)

## Endpointy

| Metoda i ścieżka | Dostęp i wynik |
| --- | --- |
| `POST /api/v1/auth/login` | Adres źródła HTTPS i dane logowania JSON. Zwraca `200` i ciasteczko sesji. |
| `POST /api/v1/auth/logout` | Admin lub viewer z tokenem CSRF. Zwraca `204` i usuwa ciasteczko. |
| `GET /api/v1/auth/me` | Admin lub viewer. Zwraca nazwę użytkownika, rolę i token CSRF. |
| `GET /api/v1/status` | Admin lub viewer. Zwraca wersję huba, liczbę aktywnych połączeń i użytkowników online. |
| `GET /api/v1/users` | Admin lub viewer. Zwraca listę bieżących SID, nazw i CID użytkowników. |
| `GET /api/v1/bans` | Admin lub viewer. Zwraca zapisane bany z powodami i datami. |
| `POST /api/v1/bans` | Admin z tokenem CSRF. Dodaje ban i zwraca `201` oraz ID. |
| `DELETE /api/v1/bans/{id}` | Admin z tokenem CSRF. Zwraca `204` lub `404` dla nieznanego ID. |
| `GET /api/v1/audit` | Admin lub viewer. Zwraca wpisy audytu. |

Przy logowaniu wyślij dane JSON z `Content-Type: application/json` i skonfigurowanym nagłówkiem `Origin`. Poniższe hasło jest tekstem zastępczym.

```json
{"username":"operator","password":"<hasło administratora>"}
```

Po zalogowaniu serwer ustawia `__Host-dc24h_session` i zwraca poniższą strukturę. Przeglądarka wysyła ciasteczko w kolejnych żądaniach do tego samego źródła. Po odświeżeniu strony można odczytać token CSRF przez `GET /api/v1/auth/me`.

```json
{"username":"operator","role":"admin","csrf_token":"<64 małe znaki szesnastkowe>"}
```

Przy wylogowaniu, dodawaniu i usuwaniu bana wyślij `X-CSRF-Token` i skonfigurowany `Origin`. Wylogowanie i usuwanie bana nie wymagają treści. Przy dodawaniu użyj `cid` dla kanonicznego CID ADC lub `ip` dla pojedynczego adresu liczbowego. Ban bezterminowy można utworzyć przez pominięcie `duration_seconds` lub podanie `null`.

```json
{"kind":"ip","identifier":"192.0.2.25","reason":"Powtarzający się spam","duration_seconds":3600}
```

Po dodaniu API zwraca odpowiedź typu `{"id":"42"}`. ID mają postać ciągów dziesiętnych, aby zachować precyzję liczb 64-bitowych. Audyt zmian banów używa nazwy zalogowanego operatora. Hub stosuje zmiany przy następnym sprawdzeniu logowania.

Podaj na przykład `?limit=25` i użyj zwróconego `next_cursor` jako `after` dla kolejnej strony. Domyślny limit to 50, a maksymalny 100. Lista użytkowników używa SID jako kursora. Listy banów i audytu używają dziesiętnego ID. Lista użytkowników może zmieniać się między stronami.

```json
{"items":[{"sid":"AAAB","nickname":"example","cid":"ZXO4VT7KPNYLJBLFLOR5YP3A33SPNOHYMEDJ4MY"}],"next_cursor":null}
```

Wszystkie endpointy zwracają błędy w tej samej strukturze.

```json
{"error":{"code":"forbidden","message":"Administrator role is required"}}
```

## Uwierzytelnianie i rozwiązywanie problemów

- `401` oznacza brak sesji, sesję nieprawidłową, unieważnioną lub wygasłą albo odrzucone poświadczenia. Sesja wygasa po ośmiu godzinach. Hasła korzystają z Argon2id z 64 MiB pamięci, trzema przebiegami, jednym pasmem i losową solą. Identyfikator sesji ma 32 losowe bajty. MariaDB przechowuje jego skrót SHA-256. [RFC 9106](https://www.rfc-editor.org/rfc/rfc9106.html)
- `403` oznacza brak wymaganej roli lub odrzucenie sprawdzenia CSRF albo źródła. Ciasteczka mają flagi `Secure`, `HttpOnly` i `SameSite=Strict`. Logowanie sprawdza źródło. Zmiany po zalogowaniu wymagają tokenu CSRF powiązanego z daną sesją. [Wytyczne OWASP dotyczące CSRF](https://cheatsheetseries.owasp.org/cheatsheets/Cross-Site_Request_Forgery_Prevention_Cheat_Sheet.html)
- `400` oznacza nieprawidłowe dane, a `413` i `431` oznaczają przekroczenie limitu treści lub nagłówków. Wysyłaj płaskie obiekty JSON z unikalnymi kluczami i obsługiwanymi polami.
- `429` oznacza limit pięciu prób na nazwę użytkownika lub 30 prób łącznie w jednominutowym oknie dla procesu. Ponów próbę po czasie podanym w `Retry-After`.
- `503` oznacza brak dostępu do MariaDB lub przekroczenie czasu odczytu listy klientów. Sprawdź lokalne gniazdo, wersję schematu i uprawnienia SQL konta API. I/O ADC/ADCS działa podczas awarii bazy używanej przez API.

Audyt udanych logowań, wylogowań i zmian banów nie zawiera poświadczeń. Pełna kopia schematu obejmuje obie tabele administracyjne. Chroń ją tak samo jak dotychczasowe kopie MariaDB.

## Testy

```sh
cmake --preset debug -DDC24H_TEST_MARIADB=ON
cmake --build --preset debug
ctest --preset debug -L admin-api
```

Test uruchamia tymczasową instancję MariaDB z własnym gniazdem uniksowym i sprawdza CLI i ścieżki HTTP przez lokalne połączenia. Pełny CTest obejmuje wcześniejsze testy ADC i ADCS.
