# API administracyjne — alpha-0.8.10

Ten etap dodaje udowodniony grant administracyjny w procesie konserwacji C++. Punktem wyjścia jest main `d7c5908`. Nie ma automatycznego scalenia. Nazwa gałęzi to `alpha-0.8.10`.

Schemat v9 pozostaje bez zmian. Grant nie jest wierszem SQL, a udowodnienie dostępu nie dopisuje zdarzenia audytu. `configuration.manage` i `hub.manage` pozostają niewdrożone. Nie mają trasy. Zapisy konfiguracji i bieżąca administracja huba pozostają w planach. Kick i wyciszenie na żywo zostają w swoich licznikach.

Token procesu pozostaje poświadczeniem transportu. `GET /api/v1/status` i `GET /api/v1/health` wymagają tylko bearer. Nagłówek grantu na tych trasach jest ignorowany. Obecny nagłówek grantu, który nie ma 64 cyfr szesnastkowych, jest błędnym żądaniem na każdej trasie.

`POST /api/v1/admin/access` przyjmuje dokładnie `actor_id` i `proof`. Konto musi istnieć i być aktywne. Nieaktywne konto jest odmawiane przed sprawdzeniem hasła. Złe hasło jest zabronione. Odpowiedź zawiera identyfikator, rolę i status aktora, `expires_in` równe 600, grant z 64 małych cyfr szesnastkowych, siedemnaście uprawnień w kolejności katalogu oraz dwanaście szablonów tras. `granted` wynika z `allows()` i domyślnej tabeli ról. Skonfigurowana maska VIP nie jest stosowana. `implemented` jest fałszem tylko dla `configuration.manage` i `hub.manage`. Właściciel zobaczyłby `hub.manage` jako nadane i niewdrożone. Administrator widzi je jako nienadane. Hasło, dowód i verifier nie wchodzą do JSON. `GET /api/v1/admin/access` jest odrzucane. `POST /api/v1/admin` pozostaje odrzucane.

Proces trzyma co najwyżej cztery granty w jedynym wątku API. Nowy dowód tego samego aktora zastępuje jego grant, a poprzedni token przestaje działać. Pełna tabela zastępuje wpis o najbliższym wygaśnięciu. Każde porównanie używa surowych 32 bajtów. Restart czyści tabelę. Grant nie jest zapisywany w MariaDB.

Każda inna trasa konserwacyjna wymaga `X-DC24H-Admin-Grant`. Brakujący, wygasły albo nieznany grant to `401` z `admin_grant_required`. Obsługa potem wczytuje aktora ponownie, wymaga nadal aktywnego konta i liczy uprawnienie trasy od nowa. Brak tokenu bearer to nadal `401` z `unauthorized` i jest sprawdzany jako pierwszy.

`GET /api/v1/admin` przyjmuje dowolny żywy grant. Odczyt kont, w tym `staff_after`, oraz utworzenie, aktywacja, wyłączenie i reset wymagają `accounts.manage`. Operator nie ma tego uprawnienia, więc katalog personelu nie jest trasą operatora. Zmiana roli wymaga `roles.manage`; silnik nadal wymusza silniejszą rolę. Zmiana hasła wymaga `accounts.password.change`, więc konto zarejestrowane może nadal zmienić własne hasło. Silnik nadal odmawia hasła cudzego konta. Nieznane mutacje kont przyjmują dowolny żywy grant, a potem zwracają dotychczasowy brak zasobu. Odczyt, utworzenie i cofnięcie bana wymagają jednocześnie `users.ban` i `blocks.manage`. `POST /api/v1/audit` wymaga `audit.read` obok dotychczasowego dowodu hasła.

Prywatny adapter przyjmuje `POST /admin-api/v1/admin/access` i przekazuje poprawny nagłówek grantu małymi literami. Sam nie wymyśla grantu. `POST /admin-api/v1/admin` pozostaje w adapterze brakiem zasobu. WebAdmin udowadnia dostęp w Admin Core. Pole hasła jest czyszczone po wysłaniu. Grant zostaje w pamięci strony, nie jest pokazywany i nie trafia do pamięci przeglądarki. Polecenie Zapomnij dostęp albo przeładowanie usuwa go. Strony konserwacji wysyłają nagłówek, gdy ta pamięć trzyma grant.

alpha-0.8.13 dodaje dokładne `GET /api/v1/management` jako kolejny odczyt odkrywania. Nie jest jednym z dwunastu szablonów tras. [Zarządzanie WebAdmin](WEBADMIN_MANAGEMENT.md) opisuje katalog. Zapisy konfiguracji i bieżąca administracja huba pozostają w planach.
