# Model zagrożeń — alpha-0.9.1

**Aktualizacja beta-0.2.7–beta-0.2.12 High Starter:** Czterosekundowy limit
całkowitego czasu zmniejsza ryzyko blokowania bramy przez powolną odpowiedź
prywatnego API. Ścisły typ JSON i zerowanie buforów ograniczają ryzyko
niepoprawnych odpowiedzi i pozostałości danych w pamięci. Lokalny Apache
odrzuca Fetch Metadata cross-site przed Node. Testy obejmują równoległe
cofnięcie uprawnień i usunięcie sesji. Nie chroni to przed przejęciem
lokalnego hosta, wykradzeniem hasła administratora ani tokenu usługi.
MFA i publiczna brama pozostają nieobsługiwane.

**beta-0.2.6 — prywatność statusu:** Osobny parser bramy lokalnej sprawdza
tylko publiczne agregaty i tworzy nową odpowiedź z zamkniętej listy.
Tożsamości, adresy klientów, konta, tokeny, audyt i prywatne ścieżki,
nawet zagnieżdżone w odpowiedzi C++, nie są serializowane do przeglądarki.
Błędny schemat, wymagane liczniki, nadmierne liczby lub awaria API dają
ogólny błąd 503 bez wcześniej zapisanych danych. Nie chroni to przed
przejęciem hosta, serwerowego tokenu czy prywatnego API i nie pozwala
na dostęp publiczny.

**Aktualizacja beta-0.2.5 — granica zaufania:** Osobny rozwojowy vhost
Apache z TLS na loopback weryfikuje Host, metodę, query string i Origin
przed przekazaniem pięciu stałych tras `/remote/v1/*`. Wspólny katalog
nie udostępnia zwykłego `index.html` ani plików `local-*` panelu
administracyjnego; rozmiar żądania ma limit. Negatywne testy na
jednorazowym Apache sprawdzają brak przekazania do backendu.
Zagrożenia pozostałe: publiczny tunel dodany przez operatora, lokalny
przejęty proces lub skradzione poświadczenia. Bez MFA brak dostępu
publicznego. Istniejący publiczny vhost nie jest modyfikowany ani wdrażany.

Ta analiza zapisuje, jak można atakować hub i WebAdmin, które mechanizmy już istnieją i które ryzyka zostają. Gałąź powstaje z main `7872471`. Nazwa gałęzi to `alpha-0.9.1`.

**Aktualizacja beta-0.2.2 (tylko brama zdalna):** Niezaufane metadane proxy
i przepisywania ścieżek (`X-Forwarded-*`, `Proxy-Authorization`, `X-Real-IP`,
`X-Original-URL`, `X-Rewrite-URL`) nie mogą wpływać na odrębną lokalną bramę
sesyjną. Node odrzuca te nagłówki przed wydaniem wyzwania lub kontaktem
z prywatnym API, a Apache usuwa popularne warianty. Dokładna kontrola
tras/metod i negatywne testy body ograniczają dwuznaczne żądania.
Nie zastępuje to MFA, ochrony przed przejęciem lokalnego proxy ani
zgody na wystawienie usługi do Internetu.

Analiza nie dodaje sprawdzenia protokołu, migracji schematu, ustawienia Apache ani mechanizmu WebAdmin. Schemat v9 pozostaje bez zmian. `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Kick i mute w przeglądarce nadal nie są wysyłane. To nie jest publiczna uwierzytelniona brama i to nie jest test penetracyjny.

beta-0.1 zmienia zgłaszany napis wydania na `beta-0.1`. Nie dodaje mechanizmu i nie zamyka żadnego zagrożenia. Schemat v9 pozostaje bez zmian. Zobacz [Linia beta](BETA.md).

alpha-0.9.17 kontrola zależności, dodana po tej analizie, sprawdza bezpośrednie biblioteki dzielone plików huba, dokładny lockfile WebAdmin i zarejestrowane testy bezpieczeństwa. Nie dodaje mechanizmu i nie zamyka żadnego zagrożenia. Schemat v9 pozostaje bez zmian. Zobacz [Zależności](DEPENDENCIES.md).

alpha-0.9.16 testy przeciążenia, dodane po tej analizie, powtarzają w procesie istniejące limity ramek, klientów, logowania, czatu i sesji. Nie dodają mechanizmu i nie zamykają żadnego zagrożenia. Schemat v9 pozostaje bez zmian. Zobacz [Testy przeciążenia](STRESS.md).

alpha-0.9.15 bezpieczeństwo pamięci, dodane po tej analizie, buduje ten sam hub z jednym z AddressSanitizer, UndefinedBehaviorSanitizer albo ThreadSanitizer. Nie dodaje mechanizmu i nie zamyka żadnego zagrożenia. Schemat v9 pozostaje bez zmian. Zobacz [Bezpieczeństwo pamięci](MEMORY_SAFETY.md).

alpha-0.9.14 optymalizacja, dodana po tej analizie, alokuje każdą ramkę routingu raz i używa ponownie zmierzonego kodowania listy powitalnej. Nie dodaje mechanizmu i nie zamyka żadnego zagrożenia. Schemat v9 pozostaje bez zmian. Zobacz [Optymalizacja](OPTIMIZATION.md).

alpha-0.9.13 profilowanie, dodane po tej analizie, szereguje stały zestaw istniejących decyzji uprawnień, moderacji i ról w procesie i nazywa najwolniejszy etap. Nie dodaje mechanizmu i nie zamyka żadnego zagrożenia. Schemat v9 pozostaje bez zmian. Zobacz [Profilowanie](PROFILING.md).

alpha-0.9.12 pomiary, dodane po tej analizie, mierzą stały zestaw istniejących decyzji uprawnień i moderacji w procesie. Nie dodają mechanizmu i nie zamykają żadnego zagrożenia. Schemat v9 pozostaje bez zmian. Zobacz [Pomiary](BENCHMARK.md).

alpha-0.9.11 monitorowanie, dodane po tej analizie, publikuje jedną próbkę procesu i liczniki sesji na istniejącym statusie z bearer. To nie jest monitor hosta i nie zapisuje historii. Nie zamyka T13. Schemat v9 pozostaje bez zmian. Zobacz [Monitorowanie](MONITORING.md).

alpha-0.9.10 bezpieczeństwo logów, dodane po tej analizie, utrzymuje plik logu jako prywatny dla właściciela i zapisuje stałe skutki bezpieczeństwa bez danych klienta. Te zapisy nie są produkcyjnym audytem. Nie zamyka T11 ani T13. Schemat v9 pozostaje bez zmian. Zobacz [Bezpieczeństwo logów](LOGGING_SECURITY.md).

alpha-0.9.9 bezpieczeństwo systemu, dodane po tej analizie, dodaje jeden przykład systemd, który uruchamia hub jako zablokowanego użytkownika systemowego z pustym zbiorem uprawnień i ścisłą hierarchią tylko do odczytu. Budowanie nie instaluje ani nie uruchamia tej usługi. Nie zamyka T07. Schemat v9 pozostaje bez zmian. Zobacz [Bezpieczeństwo systemu](SYSTEM_SECURITY.md).

alpha-0.9.8 bezpieczeństwo bazy danych, dodane po tej analizie, zapisuje jedną spójną kopię tylko dla właściciela, gdy operator o to poprosi, i odtwarza ją wyłącznie do pustych tabel tego samego schematu. Utracone potwierdzenie zatwierdzenia nie jest powtarzane. Nie zamyka T14. Schemat v9 pozostaje bez zmian. Zobacz [Bezpieczeństwo bazy danych](DATABASE_SECURITY.md).

alpha-0.9.7 bezpieczeństwo API, dodane po tej analizie, wymaga identyfikatora sesji i sekretu CSRF przy każdym wywołaniu utrzymaniowym poza dowodem hasła. Sam grant nie czyta już danych. Nie zamyka T04 ani T05. Schemat v9 pozostaje bez zmian. Zobacz [Bezpieczeństwo API](API_SECURITY.md).

alpha-0.9.6 bezpieczeństwo konta, dodane po tej analizie, odmawia hasła ADC identycznego z hasłem konta i odszyfrowuje to hasło w reaktorze dopiero po budżecie prób konta i sprawdzeniu bana konta. Nie zamyka T02 ani T03. Schemat v9 pozostaje bez zmian. Zobacz [Bezpieczeństwo konta](ACCOUNT_SECURITY.md).

Starsze części [Bezpieczeństwa](SECURITY.md) opisują wydanie, które je napisało. Zdanie, które nadal mówi, że konta, role albo bany są nieobecne, opisuje tamto wcześniejsze wydanie. Bieżące zachowanie jest zbiorem dokumentów podlinkowanych poniżej.

## Aktywa

- Stan nasłuchu huba: połączenia, sesje, nicki, CID, adresy źródłowe i tablice routingu.
- Sekrety konta: weryfikator scrypt, osobne hasło ADC w AES-256-GCM i zewnętrzny plik klucza ADC. Zobacz [Uwierzytelnianie ADC](ADC_AUTHENTICATION.md).
- Token bearer procesu i prywatne gniazdo Unix procesu, który je trzyma. Zobacz [API](API.md).
- Grant utrzymaniowy, publiczny identyfikator sesji i sekret CSRF w procesie utrzymaniowym. Zobacz [Sesje bezpieczeństwa](SECURITY_SESSIONS.md).
- Wiersze MariaDB dla kont, banów i udanego utrzymania. Zobacz [Bazę danych](DATABASE.md) i [Dziennik audytu](AUDIT_LOGS.md).
- Klucze prywatne TLS dla ADCS i dla przykładów Apache.
- Tekst czatu, kryteria wyszukiwania i tokeny transferu, gdy hub je routuje. Hub nie trzyma archiwum wiadomości.
- Pliki konfiguracji i plik hasła MariaDB.

## Aktorzy

- Zdalny klient ADC albo ADCS, w tym gość i uwierzytelnione konto.
- Konto personelu, które już przeszło GPA/PAS. Ranga pochodzi z zapisanej roli, nie z INF klienta. Zobacz [Uprawnienia](PERMISSIONS.md).
- Osoba przy przeglądarce WebAdmin na loopbacku. Przykłady Apache zostawiają `Require local`.
- Operator hosta, który może odczytać token usługi, klucz ADC, plik hasła bazy i gniazdo Unix tego samego użytkownika.
- Atakujący w sieci, który może wysłać TCP, TLS albo HTTP do odsłoniętego nasłuchu.

Operator hosta i każdy proces z UID huba są wewnątrz granicy zaufania gniazda i plików sekretów. Ta analiza nie traktuje takiego przejęcia jako rozwiązanego.

## Granice

1. Ścieżka accept w jądrze. Tożsamością jest zaakceptowany sockaddr. Nagłówki proxy nie są źródłem tożsamości. Zobacz [Bezpieczeństwo](SECURITY.md).
2. Parser ADC, maszyna stanów, sprawdzenie INF/PID/CID i polityka SUP, a potem GPA/PAS, gdy nick albo CID jest zarejestrowany.
3. Polecenia gotowej sesji: czat, wyszukiwanie, koordynacja transferu, kick, mute i przyjęcie bana.
4. Dwa prywatne gniazda Unix. Działający hub udostępnia status i health i wymaga bearer. `dc24h-account-admin` jest osobnym procesem dla utrzymania kont, ról, banów i audytu. Wspierana procedura zatrzymuje huby współdzielące bazę przed takimi zapisami. Nie ma bieżącego przeładowania konfiguracji.
5. Adapter loopback. Jest skonfigurowany jednym gniazdem i jednym plikiem tokena. Wstawia ten bearer i ignoruje nagłówek `Authorization` przeglądarki. Skierowanie go na gniazdo huba nie zamienia huba w proces utrzymaniowy.
6. Apache. Przykład statyczny blokuje `/admin-api` i `/api`. Przykład proxy przekazuje listę dozwoloną i usuwa token bearer klienta, ciasteczka oraz tożsamość z nagłówków. Zobacz [HTTPS Apache2](APACHE2_HTTPS.md) i [proxy Apache2](APACHE2_PROXY.md).
7. Strona przeglądarki. Preferencja języka może być zapisana. Grant, identyfikator sesji i sekret CSRF zostają w pamięci strony.

## Zagrożenia

### T01 Podszywanie się pod klienta

Klient może podać cudzy nick albo CID. Sprawdzenie INF wymaga, aby Tiger(PID) równał się CID, i usuwa PD z publicznego obrazu. Zarejestrowany nick albo CID niezgodny z kontem jest odrzucany i nie wraca do gościa. Nieznana para nadal może wejść jako gość. Skradziony PID pozwala jego posiadaczowi wyliczyć pasujący CID. Hub nie wiąże CID z certyfikatem klienta TLS.

Obecny mechanizm: [Uwierzytelnianie ADC](ADC_AUTHENTICATION.md) i reguły INF w [Bezpieczeństwie](SECURITY.md). Ryzyko szczątkowe: wejście gościa dla nieznanej tożsamości i brak powiązania z certyfikatem klienta.

### T02 Zgadywanie hasła online

Powtarzane próby GPA/PAS mogą celować w konto albo adres. Wspólne kubełki tokenów ograniczają próby na sekundę, na źródłowy adres IP i na konto. Sukces nie zwraca tokenu. Odmowa jest ogólna. Kubełki żyją w pamięci i zerują się po restarcie procesu. Klienci za jednym NAT dzielą jeden budżet adresu. Nie ma współdzielenia między procesami ani hostami.

Obecny mechanizm: [Ochrona przed brutalną siłą](ANTI_BRUTEFORCE.md). Ryzyko szczątkowe: restart czyści kubełki, a limity są lokalne dla jednego procesu huba.

### T03 Przechwycone hasło ADC

PAS w ADC to Tiger(hasło sklejone z wyzwaniem). Hub musi odczytać hasło ADC, więc trzyma je w AES-256-GCM, osobno od weryfikatora scrypt. `require_tls` domyślnie jest włączone. `require_tls=off` jest jawną zgodą operatora i pozwala atakować przechwycony PAS offline. Zewnętrzny plik klucza jest tak samo wrażliwy jak hasła.

Obecny mechanizm: [Uwierzytelnianie ADC](ADC_AUTHENTICATION.md). Ryzyko szczątkowe: operator, który wyłącza wymóg TLS albo ujawnia plik klucza, oddaje ten mechanizm.

### T04 Skradziony grant utrzymaniowy

Grant to 64 znaki szesnastkowe w procesie utrzymaniowym, z czasem życia 600 sekund i czterema slotami. Nowy dowód tego samego aktora usuwa stary slot. Dowód hasła jest jedyną odpowiedzią utrzymaniową, która zwraca grant, identyfikator sesji i sekret CSRF. WebAdmin nie wkłada ich do pamięci przeglądarki. Każde inne wywołanie utrzymaniowe, także odczyt, wymaga wszystkich trzech z tego samego slotu. Sam grant jest odrzucany z 403 `csrf_required`.

Obecny mechanizm: [Bezpieczeństwo API](API_SECURITY.md), [Sesje bezpieczeństwa](SECURITY_SESSIONS.md) i [API administracyjne](ADMIN_API.md). Ryzyko szczątkowe: skopiowanie grantu, identyfikatora sesji i sekretu CSRF z pamięci strony albo z lokalnego procesu nadal czyta i zapisuje dane utrzymaniowe do końca slotu.

### T05 Zapis utrzymaniowy z innej strony

Przeglądarka na tej samej maszynie może spróbować wysłać wywołanie utrzymaniowe z grantem zaobserwowanym gdzie indziej. Odczyty i zapisy, poza dowodem hasła, wymagają identyfikatora sesji i sekretu CSRF z tego samego slotu. Niezgodność zwraca 403 `csrf_required`. Adapter odrzuca podany grant bez obu wartości, zanim użyje gniazda Unix. Dowód dostępu nie wymaga CSRF, bo ta odpowiedź wydaje sekrety.

Obecny mechanizm: [Bezpieczeństwo API](API_SECURITY.md) i [Sesje bezpieczeństwa](SECURITY_SESSIONS.md). Ryzyko szczątkowe: odpowiedź dowodu nadal niesie te trzy sekrety, a wywołujący, który skopiuje wszystkie trzy, może ich używać do końca slotu. To nie zamyka T05.

### T06 Token usługi w przeglądarce albo w Apache

Adapter wstawia bearer. JavaScript przeglądarki, witryna statyczna i przykład proxy go nie zawierają. Proxy usuwa nagłówek Authorization klienta. Po usunięciu `Require local` zdalny klient może użyć adaptera tak, jakby był na loopbacku, i dojść do skonfigurowanego procesu tokenem adaptera.

Obecny mechanizm: [API](API.md), [WebAdmin](WEBADMIN.md) i [proxy Apache2](APACHE2_PROXY.md). Ryzyko szczątkowe: usunięcie `Require local` łamie tę granicę.

### T07 Dostęp do gniazda Unix z tego samego UID

Katalog gniazda jest prywatny dla właściciela, a poświadczenie drugiej strony musi mieć ten sam efektywny UID. Root i każdy proces tego użytkownika może się połączyć i podać token, jeśli odczyta plik tokena. API ma mały limit połączeń i krótki czas żądania. Ten limit nie jest granicą uprawnień między programami tego samego użytkownika.

Obecny mechanizm: [API](API.md) i [Bezpieczeństwo systemu](SYSTEM_SECURITY.md). Ryzyko szczątkowe: przejęcie tego samego UID i roota konta usługi zostaje wewnątrz granicy zaufania. Przykładowa jednostka nie jest włączana przez budowanie. To nie zamyka T07.

### T08 Zdalny WebAdmin

Oba przykłady Apache zostawiają `Require local`. Nie ma sesji w ciasteczku i nie ma zdalnego logowania administratora. Zgodność nazwy certyfikatu z kluczem należy do operatora. Przechodzący test konfiguracji tej zgodności nie dowodzi.

Obecny mechanizm: [HTTPS Apache2](APACHE2_HTTPS.md). Ryzyko szczątkowe: publiczna uwierzytelniona brama pozostaje osobną pracą. To nie jest publiczna uwierzytelniona brama.

### T09 Podrobiona tożsamość proxy

Hub nie ufa `X-Forwarded-For` ani żadnemu innemu nagłówkowi przekazanej tożsamości. Przyjęcie, kubełki uwierzytelniania i budżet utrzymaniowy używają zaakceptowanego sockaddr albo sesji w pamięci, nie nagłówka. Przykład proxy usuwa te nagłówki i ustawia `ProxyAddHeaders Off`. Zaufany skok przez proxy nie jest zaimplementowany. Klienci za jednym adresem dzielą jeden budżet adresu.

Obecny mechanizm: [Bezpieczeństwo](SECURITY.md) i [proxy Apache2](APACHE2_PROXY.md). Ryzyko szczątkowe: jeden NAT albo jedno proxy składa wielu klientów w jeden budżet.

### T10 Uprawnienia personelu

Kick, mute, unmute oraz bany konta, CID i adresu IP używają zapisanej roli i reguł moderacji. Klient nie podnosi własnej rangi przez INF. Przekazanie właściciela wymaga dwóch dowodów przez lokalne CLI. WebAdmin pokazuje decyzje kick i mute i ich nie wysyła. `configuration.manage` i `hub.manage` nie mają silnika. Konto personelu, które już ma uprawnienie, może go użyć. Skradzione hasło personelu niesie tę samą władzę, aż konto zostanie wyłączone.

Obecny mechanizm: [Uprawnienia](PERMISSIONS.md), [Reguły moderacji](MODERATION_RULES.md), [Kick](KICK.md), [Mute](MUTE.md) i [Rdzeń banów](BAN_CORE.md). Ryzyko szczątkowe: zwykły kick, mute albo ban nie wymaga drugiej osoby.

### T11 Brakujący audyt

Udane utrzymanie kont, właściciela i banów może odczytać `audit.read`. Odmowy, hasła, dowody, powody w wolnym tekście, bieżący kick i bieżący mute nie są wierszami. Rejestracja poza Admin Core nie dodaje wiersza. Zdarzenia dziennika bezpieczeństwa są zbiorcze. Prywatny log może przechowywać stały zapis bezpieczeństwa bez aktora i bez celu. Nie ma produkcyjnego audytu nieudanego CSRF ani użycia grantu.

Obecny mechanizm: [Dziennik audytu](AUDIT_LOGS.md) i [Bezpieczeństwo logów](LOGGING_SECURITY.md). Ryzyko szczątkowe: tabela audytu nie odtworzy odmowy ani bieżącego kicka.

### T12 Hub widzi routowane dane klienta

Dla ADCS TLS kończy się na hubie. Hub potem routuje tekst czatu, kryteria wyszukiwania i tokeny połączeń w pamięci huba. Wiadomość prywatna trafia do zaadresowanej sesji. Hub nie trzyma archiwum wiadomości. Udokumentowane zdarzenia bezpieczeństwa i uwierzytelniania pomijają adresy źródłowe, nicki, CID, PID, hasła i tekst ADC. Zobacz [Dziennik](LOGGING.md) i [Ochronę przed brutalną siłą](ANTI_BRUTEFORCE.md). Zrzut procesu, debugger albo przejęty host huba nadal widzi tekst jawny po TLS. Bajty pliku między peerami nie przechodzą przez hub. Zobacz [Limity transferu](TRANSFER_LIMITS.md).

Obecny mechanizm: te reguły routingu i dziennika. Ryzyko szczątkowe: hub jest na ścieżce wiadomości, które routuje.

### T13 Wyczerpanie zasobów

Przyjęcie ogranicza połączenia na adres IP, przyjęcia na adres IP, tempo ramek, błędne ramki, czas uzgadniania i niedokończone ramki. Wyszukiwanie i koordynacja transferu mają własne budżety. Zobacz [Bezpieczeństwo wyszukiwania](SEARCH_SECURITY.md) i [Limity transferu](TRANSFER_LIMITS.md). Utrzymanie przyjmuje jeden POST na sekundę i 24 żądania w ciągu 10 sekund na żywą sesję. To lokalne polityki zasobów. To nie jest limit pasma i to nie jest ochrona przed rozproszoną odmową usługi. Klient, który przeszedł przyjęcie, nadal może zużywać CPU na TLS i na scrypt. Wolny dysk dziennika może zablokować proces.

Obecny mechanizm: [Bezpieczeństwo](SECURITY.md) i [Sesje bezpieczeństwa](SECURITY_SESSIONS.md). Ryzyko szczątkowe: nie ma limitu pasma w bajtach i nie ma współdzielenia między hostami.

### T14 Pliki sekretów

Hasło bazy, token API i klucz ADC pochodzą z plików właściciela, nie z wiersza poleceń. SQL używa wiązanych parametrów. Błędy pomijają tekst serwera. Kopia zapasowa, kopia czytelna dla wszystkich albo kopia w dzienniku niweczy ten rozdział. Migracje biegną tylko wtedy, gdy operator o nie poprosi. Zwykły start nie migruje.

Obecny mechanizm: [Bezpieczeństwo bazy danych](DATABASE_SECURITY.md) i [Baza danych](DATABASE.md). Ryzyko szczątkowe: kopie poza hostem i uprawnienia katalogu danych MariaDB zostają przy operatorze. Hub zapisuje jedną kopię tylko dla właściciela wyłącznie na żądanie. To nie zamyka T14.

### T15 Luki polityki TLS

ADCS na nasłuchu huba używa TLS 1.2 albo nowszego. Przykłady Apache wybierają TLS 1.2 i 1.3 z szyframi AEAD i krótką polityką HSTS dla hosta. HTTP/2, OCSP i automatyczne odnawianie certyfikatu nie są zaimplementowane. Klienci muszą sami sprawdzić certyfikat huba. Hub nie żąda certyfikatu klienta.

Obecny mechanizm: [HTTPS Apache2](APACHE2_HTTPS.md). Ryzyko szczątkowe: te trzy operacje TLS pozostają niezaimplementowane.

### T16 Niedokończona powierzchnia administracyjna

Katalog utrzymaniowy może nazwać konfigurację i administrację huba, a potem zgłosić je jako niezaimplementowane. WebAdmin nie wysyła kicka ani mute. Zapisy kont, ról i banów należą do `dc24h-account-admin` po zatrzymaniu hubów współdzielących bazę. Unix API działającego huba pozostaje statusem i health.

Obecny mechanizm: brakujące silniki są nieobecne, a strony tak mówią. Ryzyko szczątkowe: ta powierzchnia jest niedokończoną pracą, nie ukrytym mechanizmem.

## Co pozostaje osobną pracą

Publiczna uwierzytelniona brama z MFA, zewnętrzny dostęp administratora przez Internet, HTTP/2, OCSP, automatyczne odnawianie certyfikatu, produkcyjny dziennik audytu, polityka AppArmor albo SELinux, zapora, nienadzorowane aktualizacje pakietów, `configuration.manage`, `hub.manage` oraz kick albo mute w przeglądarce. beta-0.2 nie oznacza żadnego z nich jako ukończonego.

### Granica lokalnej bramy beta-0.2

Osobna lokalna brama HTTPS używa ciasteczek HttpOnly/Secure/SameSite,
krótkich sesji w pamięci i dowodu hasła administratora albo Ownera.
Każdy odczyt ponownie weryfikuje aktualną rolę i wysyła tylko określone
zbiorcze liczniki. Brak proxy do ogólnego API, zapisu i poleceń systemd.
Pozostają ryzyka przejęcia lokalnego tokenu/gniazda, serwera,
przeglądarki lub hasła oraz błędnej konfiguracji lokalnego TLS i limitów
obowiązujących tylko w procesie. **Brak MFA i publicznego dostępu.**
Szczegóły: [Brama WebAdmin](REMOTE_GATEWAY.md).

W beta-0.2.4 dodano testy odmowy ponownego użycia starego cookie po
zalogowaniu, izolacji CSRF między przeglądarkami, jednorazowych wyzwań,
wygaśnięcia przy opóźnionej autoryzacji, wylogowania w trakcie odczytu
oraz utraty wszystkich sesji po restarcie procesu. Brama pozostaje
lokalna i przechowuje sesje tylko w pamięci; przejęcie hosta lub
przeglądarki, kradzież hasła i brak MFA pozostają ryzykami.

Dodatkowy audyt usuwa wyścig równoległych ponownych logowań oraz logowania
trwającego podczas wylogowania, które wcześniej mogły pozostawić nową
osieroconą sesję. Przeglądarka ignoruje spóźnione statystyki po zmianie
sesji, więc stary odczyt nie przekracza granicy nowej autoryzacji.
