# Lokalne API administracyjne — alpha-0.1.9

**Bieżący etap — alpha-0.5.7:** [Feature Routing](FEATURE_ROUTING.md) kompiluje selektory FSCH raz i korzysta z aktualnego publicznego INF SU: wszystkie wymagane, żadna wykluczająca. Negocjacja SUP jest niezależna. Zmiany możliwości, udział źródła, nieznane rewizje i limity sprawdzono przez ADC/ADCS i rzeczywisty ncdc. RES/routing wyników pozostaje kolejnym etapem.

**Etap dostarczony w alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) przekazuje sprawdzone SCH przez rzeczywiste ADC/ADCS: broadcast B/F i D/E związane z życiem celu współdzielą jeden niezmienny pakiet z oddzielnymi ograniczonymi kolejkami. Podane TO jest rejestrowane w Search Core; brak TO nadal jest poprawny. Obowiązują kontrole źródła/stanu/funkcji, jednokrotny dispatch i sprzątanie tokenów. RES/routing wyników pozostaje kolejnym etapem.

**Etap dostarczony w alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) dodaje ograniczony rejestr życia sesji/TO, lokalne generacje wyszukiwań, terminy ważności, anulowanie i jawne API aktywnych wyszukiwań Core. Nadal można przygotować plan bez rejestracji; odrzucone rzeczywiste SCH nie rezerwuje tokenów. Korelacja/routing wyników i administracja wyszukiwaniem pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) przekazuje pełny 192-bitowy hash pliku z SCH do niezmiennych zapytań/planów Core, opisów i filtrów. Zapytania z samym hashem lub łączone używają dokładnej równości i zachowują dodatkowe kryteria. Testowane są rzeczywiste zapytania TTH ncdc ADC/ADCS; routing zapytań/wyników pozostaje kolejnym etapem.

**Etap dostarczony w alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) dodaje skompilowane filtry nazw/ścieżek Unicode bez rozróżniania wielkości liter, alternatywy rozszerzeń, domknięte/dokładne limity bajtów i typ plik/katalog dla sprawdzonych opisów. Zapytania z samymi filtrami i nieprzezroczyste tokeny NFC przechodzą do Search Core. Dopasowanie nie alokuje pamięci; routing/wyniki, indeksy i administracja wyszukiwaniem pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.2:** [ADC SCH](ADC_SCH.md) dodaje własny typowany parser SCH i odbiór w ADC/ADCS po sprawdzeniu stanu/źródła. Sprawdza pola, selektory, limity i kanoniczne TR; poprawne zapytania otrzymują informację o braku routingu, błędne semantycznie odwracalną odmowę, a zapytania bez kryteriów są pomijane. Routing/wyniki pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.1:** [Search Core](SEARCH_CORE.md) dodaje niezależną bibliotekę C++20 dla ograniczonych, niezmiennych zapytań tekstowych, wyboru gotowych sesji/funkcji oraz dispatch związanego z czasem życia i domeną silnika. Parser, serializacja i podłączenie SCH/RES do transportu pozostają kolejnymi etapami; działający serwer nadal zwraca jawną odmowę nieobsługiwanej komendy.

**Etap dostarczony w alpha-0.4.12:** [Statystyki czatu WebAdmin](WEBADMIN_CHAT.md) dodają osobny widok Czat z dziewięcioma licznikami zbiorczymi, sekcjami wiadomości/kolejek, opisami US/PL, odświeżaniem, obsługą błędów/restartu i bezpiecznym eksportem.

**Etap dostarczony w alpha-0.4.11:** [Chat Logging](CHAT_LOGGING.md) dodaje opcjonalne logi wyników bez treści, z limitem tempa, oraz nasycane liczniki klienckiego B/D/E MSG w uwierzytelnionym API i WebAdmin US/PL. Przyjęcie do kolejki jest odróżnione od dostarczenia przez transport.

**Etap dostarczony w alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) grupuje startowe ustawienia czatu i dodaje limit odkodowanego tekstu, niezależne przełączniki publicznego/prywatnego/akcji i opcjonalne własne powitanie. Domyślne wartości zachowują działanie; odwracalne odmowy nie omijają walidacji ani Anti-Flood.

**Etap dostarczony w alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

**Etap dostarczony w alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizuje sprawdzanie tekstu, SID i gotowości/własności nadawcy w obu adapterach. Wybór życia odbiorcy i ponowna kontrola dispatch pozostają obowiązkowe; systemowe IMSG jest oddzielone.

**Etap dostarczony w alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) dodaje zaufane systemowe IMSG powiązane z życiem sesji i stałe powitanie po własnym BINF. Klient nie może nadawać komunikatów systemowych; zachowano limity kolejek i gotowość przy końcowym LF.

**Etap dostarczony w alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizuje dekodowanie PM/ME i kanoniczne wyjście kodeka oraz obu adapterów czatu. Brak ME, ME0 i ME1 pozostają rozróżniane; PM nie nadaje uprawnień do dostarczania.

**Etap dostarczony w alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizuje wybór zakresu MSG i dostarczanie do ograniczonych kolejek. Plany są powiązane z domeną macierzystego Chat Core i ID życia sesji; prywatne wiadomości nigdy nie trafiają zastępczo na czat publiczny.

**Etap dostarczony w alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) dodaje DMSG tylko do celu i EMSG do celu oraz nadawcy, pojedyncze wiadomości do siebie, powiązanie celu z ID życia i kontrolę PM. Czat publiczny pozostaje oddzielony; grupy/historia/moderacja to kolejne etapy.

**Etap dostarczony w alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) dodaje osobny adapter polityki i rozsyłania BMSG oraz jedną wspólną niemutowalną ramkę na publikację. Offsety i logiczne budżety odbiorców pozostają niezależne; zachowano bramki źródła/stanu/prywatności i FIFO.

**Etap dostarczony w alpha-0.4.2:** [Chat Core](CHAT_CORE.md) dodaje osobny silnik wiadomości niezależny od protokołu oraz publiczne BMSG, z kopią dla nadawcy i /me. Czat prywatny/grupowy/feature/hub, moderacja i historia pozostają osobnymi etapami.

**Etap dostarczony w alpha-0.4.1:** [ADC MSG](ADC_MSG.md) dodaje kodek MSG z typowanymi polami PM/ME, sprawdzanym budowaniem komunikatów, ograniczoną serializacją i walidacją w maszynie stanów. Dostarczanie czatu i polityka rozmów pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.3.13:** [Statistics](STATISTICS.md) dodaje liczniki cyklu życia w czasie stałym, użytkowników online/szczyt i sumę logowań, atomowe statystyki transportu, pola chronionego API oraz metryki WebAdmin US/PL. Starsze etapy poniżej zachowują swój zakres historyczny.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

**Etap dostarczony w alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) posiada indeksy SID/CID/nick wraz z żywymi modelami. Odczyt const i zarządzane zmiany cyklu życia/User zachowują unikalność przy przyjęciu, zmianie nicku i sprzątaniu. LoginHub zachowuje protokół i kolejki. Starsze opisy dotyczą zakresu historycznych etapów.

**Etap dostarczony w alpha-0.3.3:** [User Core](USER_CORE.md) dodaje do Session publiczny model gościa i profilu, projekcję całego zwalidowanego INF oraz aktualizacje przez bramkę cyklu życia. Powstał bezpośrednio z main z Session Core; main łączy go teraz z wcześniej scalonym alpha-0.3.2 Session Manager. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.


**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

Zaimplementowano opcjonalne API HTTP/1.1/JSON tylko do odczytu, dostępne przez lokalne gniazdo Unix w Linux. Jest niezależne od ADC/ADCS i nie otwiera portu TCP. To podstawa API, nie kompletny backend WebAdmin. Brak zarządzania użytkownikami, ról, operacji zmieniających stan, edycji konfiguracji, zapytań SQL, zdalnego HTTP/TLS i produkcyjnego logowania WWW.

## Dostęp i konfiguracja

Każde żądanie wymaga połączenia z efektywnego UID procesu (`SO_PEERCRED`) oraz `Authorization: Bearer TOKEN`. Token to dokładnie 64 znaki szesnastkowe (32 losowe bajty), odczytane raz przy starcie z prywatnego pliku. Jest to sekret usługi, nie hasło użytkownika huba ani rola. Porównanie tokenów tej samej długości używa `CRYPTO_memcmp`. Zmiana pliku wymaga restartu; brak przeładowania/unieważniania tokenów. Nie umieszczaj tokena w kodzie, wartościach INI, URL i logach. Serwer nie przyjmuje tokena przez CLI ani środowisko.

| Klucz INI `[api]` | Wartość domyślna / zasada |
| --- | --- |
| `enabled` | `off`; włączenie wymaga `socket` i `token_file` |
| `socket` | brak; ścieżka do 107 bajtów, ostatni element do 64 bajtów |
| `token_file` | brak; ścieżka względem katalogu INI |
| `max_connections` | 16; zakres 1..128 klientów API, niezależnie od ADC |
| `request_timeout` | 2; zakres 1..30 sekund, bezwzględny termin od przyjęcia do zakończenia |

Ścieżka gniazda po rozwiązaniu względem INI musi być absolutna. Bezpośredni katalog nadrzędny musi istnieć, należeć do efektywnego UID i nie mieć praw grupy/innych (zwykle 0700). Jego ostatni element nie może być symlinkiem. Katalogi nadrzędne muszą pozostać zaufane. Tworzenie jest zakotwiczone przez `/proc/self/fd` w sprawdzonym katalogu, bez zmiany umask/katalogu roboczego procesu. Gniazdo otrzymuje 0600 przed nasłuchem. Brak dostępu grupowego i bramy między UID, w tym dostępu workerów Apache działających jako inny użytkownik. Nie osłabiaj uprawnień dla Apache; osobno zaprojektowana uwierzytelniona brama jest przyszłym etapem.

Plik tokena korzysta ze wspólnego `security::read_private_secret`: właściciel to efektywny UID, zwykły plik, bez praw grupy/innych, jedno dowiązanie twarde, brak końcowego symlinku, 1..4096 bajtów przed walidacją tokena. Usuwane jest jedno końcowe LF/CRLF. Katalogi/FIFO, NUL/znaki sterujące, puste/zbyt duże pliki i błędny token zatrzymują start. Tymczasowy bufor i token aplikacji są zerowane przy zwalnianiu; nie deklarujemy całkowitego usuwania kopii żądań, sterowników i systemu. Hasło MariaDB zachowuje dotychczasowe zasady przez ten sam czytnik.

Tryb wyłączony nie otwiera zasobów API ani nie czyta tokena. `--check-config` sprawdza ustawienia/kolizje ścieżek, ale nie czyta tokena, nie bada praw katalogu wykonawczego i nie tworzy gniazd. Sprawdzenie/migracja bazy również nie uruchamiają API. Błąd startu API daje kod 1 przed otwarciem portów ADC; błędna składnia/kolizje dają 2. Log nie może wskazywać tokena ani gniazda, także przez istniejące aliasy. Gniazdo nie może wskazywać konfiguracji, plików TLS, hasła/gniazda bazy, tokena i logu.

## Przykład lokalny

Wykonaj jako to samo dedykowane konto, które uruchamia dc24h, w zaufanym zapisywalnym katalogu. Polecenia tworzą/zastępują sekret; używaj ich przy pierwszym przygotowaniu lub świadomej rotacji.

```sh
install -d -m 0700 .runtime
install -m 0600 /dev/null .runtime/admin.token
openssl rand -hex 32 > .runtime/admin.token
```

W `config/dc24h.ini` zastąp istniejącą sekcję `[api]`, nie powtarzaj kluczy:

```ini
[api]
enabled = on
socket = ../.runtime/admin.sock
token_file = ../.runtime/admin.token
max_connections = 16
request_timeout = 2
```

Uruchom `./build/dc24h --config config/dc24h.ini`. Przekaż nagłówek curl przez stdin, aby token nie trafił do argumentów procesu:

```sh
{ printf 'Authorization: Bearer '; cat .runtime/admin.token; } |
  curl --silent --show-error --fail --unix-socket .runtime/admin.sock \
    --header @- http://localhost/api/v1/status
```

To HTTP przez gniazdo Unix; `localhost` jest wartością Host, nie połączeniem TCP. Token nie trafia do usługi zewnętrznej. Curl służy do ręcznej obsługi i nie jest zależnością kompilacji.

## Wersjonowane endpointy

Obie trasy wymagają uwierzytelnienia. Klucze JSON i błędy protokołu pozostają EN/US niezależnie od języka operatora.

| Metoda / ścieżka | Wynik |
| --- | --- |
| `GET /api/v1/health` | 200 `{"status":"ok"}`: wątek API obsługuje żądanie; nie jest to test bazy ani pełnej gotowości ADC |
| `GET /api/v1/status` | 200: ograniczony zestaw danych poniżej |

```json
{
  "api_version": "v1",
  "server": {"name": "dc24h.eu", "version": "alpha-0.4.4", "core_running": true, "uptime_seconds": 3},
  "network": {"active_connections": 0, "accepted_connections": 1, "adc_frames": 1, "adcs_handshakes": 0, "security_rejected": 0, "security_closed": 0, "rejected_connections": 0, "closed_connections": 1, "received_bytes": 20, "sent_bytes": 32, "peak_connections": 1},
  "users": {"online": 0, "peak_online": 0, "successful_logins": 0},
  "database": {"enabled": false}
}
```

Liczniki są niezależnymi odczytami atomowymi, nie jedną transakcją. `core_running` podaje tylko stan Core, nie pełne logowanie/gotowość ADC. Uptime pochodzi z zegara monotonicznego od przygotowania transportu/API po opcjonalnym połączeniu bazy; nie jest wiekiem procesu według zegara ściennego. `database.enabled` oznacza obecność skonfigurowanego połączenia startowego, nie bieżącą dostępność SQL. Żądania nie wykonują SQL. API nie zwraca adresów klientów, nicków, ramek, ścieżek, sekretów ani ustawień administracyjnych. Nieznane ścieżki (również query string) dają 404 po autoryzacji; inne metody dają 405 i `Allow: GET`. HEAD jest nieobsługiwane (405), bez treści odpowiedzi.

Brak/błędne uwierzytelnienie daje 401 i `WWW-Authenticate: Bearer realm="dc24h-admin"` przed sprawdzeniem trasy/metody. Błędy to stały JSON: `{"error":"unauthorized"}`, `not_found`, `method_not_allowed`, `bad_request`, `headers_too_large`; bez danych wejściowych i treści wyjątków. Długość/format tokena nie są tajne; nie deklarujemy ochrony przed brute force ze strony procesów już działających jako UID usługi.

## Podzbiór HTTP i ograniczenia zasobów

Parser wymaga HTTP/1.1, ścieżek origin-form, CRLF, jednego niepustego Host i nagłówków ASCII. Wielkość liter nazw nagłówków jest ignorowana; powtórzenia, spacje przed dwukropkiem, składane wiersze, znaki sterujące i bajty spoza ASCII są odrzucane. Limity: 8192 bajty żądania, 32 nagłówki, 1024 bajty pierwszego wiersza. Przekroczenie nagłówków daje 431; błędne żądanie 400. Kompletne nagłówki dokładnie na granicy są dozwolone. Content-Length może nie istnieć lub mieć dokładnie `0`; odrzucane są Transfer-Encoding, Expect, Upgrade, niezerowa/niejednoznaczna długość oraz już odebrane bajty treści/kolejnych żądań. Jeżeli dodatkowe bajty dotrą po obsłużeniu pierwszego żądania, połączenie jest zamykane bez uruchomienia kolejnego. Brak keep-alive, parsera body, kompresji, CORS i upgrade. Jedno połączenie obsługuje najwyżej jedno żądanie.

Odpowiedzi mają Content-Length, JSON UTF-8, Date, `Connection: close`, `Cache-Control: no-store` i `X-Content-Type-Options: nosniff`. Bufory są ograniczone, dane użytkownika nie są odbijane. Wątek używa nieblokujących gniazd i ograniczonej pętli poll. Na klienta przypada jedna próba odczytu/zapisu na obrót, a przyjmowanie ma limit 16 prób na obrót. Nadmiarowi klienci i inne UID są zamykani bez odpowiedzi HTTP. Bezwzględny termin obejmuje żądanie/odpowiedź i nie przesuwa się po nadejściu danych. Wygaśnięcie zamyka klienta. Brak limitu tempa przyjęć API i deklaracji ochrony DDoS. Zaufane callbacki i synchroniczny logger nie powinny blokować bez końca.

## Własność, zatrzymywanie i błędy

`api::Server` posiada wątek i gniazda; `start()` jest jednorazowe. Konstruktor odczytuje token, ale nie binduje. Destruktor zatrzymuje/dołącza wątek, zamyka klientów/listener i usuwa tylko zapamiętany inode gniazda przez zachowany fd katalogu. Nie usuwa pliku/symlinku/gniazda podmienionego pod tą samą nazwą. Istniejące ścieżki, także stare gniazda, nie są nadpisywane/usuwane przy starcie: zatrzymaj wcześniejszy proces i ręcznie usuń sprawdzone stare gniazdo. SIGKILL nie uruchamia sprzątania.

Main tworzy API po przygotowaniu Core/Network, z bezpiecznymi współbieżnie callbackami stanu/błędu. Wątek nie posiada Core ani nie wykonuje operacji zmieniających jego stan na żądanie HTTP. SIGINT/SIGTERM i błąd startu Network zatrzymują/dołączają API przed zniszczeniem zależności. Krytyczny wyjątek infrastruktury/callbacku daje wyłącznie log `Local API worker failed`, żąda stop przez callback właściciela i kończy proces błędem. Błędne dane, timeouty i błędy I/O klienta zamykają tylko jego połączenie. Logi startu/błędów nie zawierają żądań, nagłówków, tokena i ścieżek. Brak rejestru audytowego żądań.

Źródła: [RFC 9112 — ramkowanie HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112.html), [RFC 9110 — semantyka HTTP](https://www.rfc-editor.org/rfc/rfc9110.html), [gniazda Unix i poświadczenia procesów w Linux](https://man7.org/linux/man-pages/man7/unix.7.html).

## Wewnętrzne C++ w alpha-0.1.5

Nagłówek `dc24h/core/server.hpp`:

- `Server(logging::Logger&, network::TcpServer* = nullptr)` — Core z opcjonalnym
  transportem. Logger i wskazany Network muszą żyć dłużej niż Core.
- `run()` — blokujący, jednorazowy start. Bez transportu czeka na zmiennej
  warunkowej; z transportem wykonuje jego pętlę. Wyjątek Network ustawia
  `stopped` i jest przekazywany do wywołującego.
- `request_stop()` — bezpieczne współbieżnie, idempotentne żądanie stop,
  przekazywane również do Network. Żądanie przed startem jest zapamiętywane.
- `state()` — spójny odczyt `created`, `running`, `stopping` lub `stopped`.

Nagłówek `dc24h/network/tcp_server.hpp`:

- `Endpoint { address, port }`, `parse_endpoint(string_view)` — numeryczne
  IPv4:port / [IPv6]:port, walidacja przez `inet_pton` i `from_chars`.
- `Options { listeners, tls_listeners, tls_certificate, tls_private_key, max_connections, idle_timeout }` — konfiguracja TCP;
  konstruktor sprawdza zakresy, bez otwierania portów.
- `TcpServer(Logger&, Options = {})` — tworzy deskryptor budzenia `eventfd`.
- `run()` — jednorazowo otwiera listenery, obsługuje klientów i zamyka gniazda
  przy stopie/błędzie. Drugie wywołanie rzuca `logic_error`.
- `request_stop() noexcept` — atomowa flaga i nieblokujące budzenie; bezpieczne
  współbieżnie, również przed `run()`.
- `statistics()` — niezależnie odczytane atomowe liczniki `active`, `accepted`,
  `rejected`, `received_bytes`, `adc_frames`, `adc_unknown`, `adc_invalid`,
  `adc_keepalives`, `adcs_handshakes`, `adcs_incomplete`. Nie stanowią wspólnego transakcyjnego snapshotu.

Właściciel dołącza wątek `run()` i wszystkich wywołujących `request_stop()` przed
zniszczeniem obiektów. Destruktor nie zastępuje stop/join. Gniazda TCP należą
wyłącznie do wątku pętli; `request_stop()` nie zamyka ich z innego wątku.
Transport nie udostępnia jeszcze callbacków wiadomości ani zapisu do klienta.

`TerminationSignals` jest adapterem procesu dla Linux; ustawienie maski należy
wykonać w głównym wątku przed utworzeniem innych wątków.

## Parser ADC

Nagłówek `dc24h/adc/parser.hpp` udostępnia `Message`, `MessageType`, `Command`,
`FeatureSelector`, `ParseError`, wariant `ParseResult`, `parse_line`,
`recognize_command` oraz `StreamDecoder` z `feed`, `finish` i `buffered_bytes`.
Szczegółowy kontrakt, zasady własności, limity i semantyka błędów:
[ADC_PROTOCOL.md](ADC_PROTOCOL.md). Dekoder jest jednowątkowy; niezależne
instancje oraz `parse_line` można używać równolegle.

## Plan WebAdmin

Dalsze operacje administracyjne i integracja Apache wymagają kontroli ról oraz rejestru audytowego.

## ADCS i języki

`adcs::Context` posiada kontekst TLS. `adcs::Connection` wymaga, by fd i kontekst żyły dłużej niż sesja. `handshake()` i `receive()` zwracają complete/want_read/want_write/closed/error wraz z liczbą bajtów. `pending()` zgłasza odszyfrowane dane w buforze. Pętla odpowiada za gotowość gniazda i obsługę SIGPIPE. Brak API zapisu. `Logger(Language = us)` i `i18n::parse_language/translate/help` dostarczają komunikaty US/PL.

## Konfiguracja i logger

`configuration::Settings` zawiera opcje sieci/loggera/bazy/API i pochodzenie (`source_file`, `implicit_plain_listener`). `load_file` parsuje składnię i pojedyncze pola; zależności sprawdza osobne `validate(Settings)` po nadpisaniach. `parse_arguments(argc, argv)` wczytuje plik, stosuje CLI i waliduje; `CommandLine` dodaje flagi check/help/version. `logging::Options` ma level, file_path (puste = stderr), language i connections. `Logger(Options = {})` otwiera plik w konstruktorze; pozostaje też `Logger(Language)`. Obiekt jest niekopiowalny i musi żyć dłużej niż wywołujący. Brak API zmiany ustawień/reload.

## Logger — alpha-0.1.6

`Level` ma info/warning/error/off. `info`, `warning`, `error` przyjmują niesekretny tekst wewnętrzny i nie rzucają. `connection(ConnectionEvent, uint64_t id, bool secure)` dopuszcza tylko enum zdarzeń, bez dowolnej treści. `connection_enabled()` sprawdza niezmienną flagę i próg INFO. Mutex chroni wspólny limit 64 wpisów/okno sekundy i raport pominięć w destruktorze/przy odnowieniu. Kolejność Options: level, file_path, language, connections; wcześniejsze inicjalizacje pozostają zgodne. Szczegóły: [LOGGING.md](LOGGING.md).

## Security — alpha-0.1.7

`network::Options::security` zawiera sześć pól `security::Limits`. `security::validate` odrzuca wartości poza zakresem. `PeerLimiter(Limits)` należy do reaktora: `admit(kanoniczny_klucz, czas_monotoniczny)` przyznaje aktywny slot/przyjęcie; po błędzie/zamknięciu wykonaj dokładnie jedno `release(key)` dla każdego sukcesu. Długość klucza i mapa są ograniczone; `tracked_peers()` zwraca liczbę wpisów. `FrameLimiter(maximum)::admit(czas)` daje okno sekundy na klienta. Czasy muszą być monotoniczne. `Statistics` dodaje niezależne atomowe security_rejected/security_closed. Brak współdzielenia stanu między instancjami/procesami. Szczegóły: [SECURITY.md](SECURITY.md).

## Podstawa WebAdmin — alpha-0.1.10

Panel React tylko do odczytu i opcjonalny lokalny adapter Vite tego samego UID są dostępne: [WEBADMIN.md](WEBADMIN.md). Kontrakt i uprawnienia prywatnego API C++ nie zmieniają się. Statyczny Apache nie zapewnia uwierzytelnionej bramy API.

Od alpha-0.2.2 `Message::positional_count` określa układ argumentów BASE (nieznane komendy pozostają nieinterpretowane). `parse_named_parameter` zwraca pożyczone widoki nazwy/wartości; `parse_integer` opcjonalne signed 64-bit. Dotychczasowe parse_line/StreamDecoder odrzucają teraz błędną składnię argumentów BASE; szczegóły opisuje [kontrakt parsera](ADC_PROTOCOL.md).

## API stanów ADC — alpha-0.2.3

`dc24h/adc/state.hpp` udostępnia `ConnectionStateMachine`, `ConnectionState`, `Direction`, `IdentityMode`, `StateError` i `StateResult`. Metody `observe`, `keepalive`, `approve_protocol`, `approve_identity`, `approve_password` i terminalne `disconnect` opisuje [ADC_STATE.md](ADC_STATE.md). Wyniki zawierają stałe kategorie błędów, bez danych klienta. To API modułu C++; brak nowych tras/liczników HTTP i egzekwowania sesji w transporcie.

## API negocjacji SUP — alpha-0.2.4

`dc24h/adc/sup.hpp` udostępnia SupportPolicy, SupportNegotiation, SessionHash, SupportError i ProtocolConnection. Adapter zwraca opcjonalny `variant<StateError, SupportError>`; odrzucenie zachowuje stan/funkcje. Udostępnia wyłącznie stałe metadane negocjacji i nie daje ręcznego zatwierdzenia protokołu. Odpowiedzi Message/string posiadają własne dane. Brak zmian tras HTTP i schematu/liczników. [ADC_SUP.md](ADC_SUP.md) opisuje czasy życia, limity, błędy i przyszłą integrację transportu.

## API SID — alpha-0.2.5

`adc/sid.hpp` udostępnia parsowanie/kodowanie wartości Sid, SidPool i przenoszalny SidLease. `adc/sid_session.hpp` łączy własność puli z funkcjami/stanem w SidSession; zwraca opcjonalny `variant<StateError, SupportError, SidError>`. Zatwierdzić można wyłącznie zarezerwowany SID. Przygotowanie/kodowanie nie zmienia fazy; obserwacja ukończonego wysłania tak. Liczniki puli należą do reaktora, nie telemetrii HTTP. Brak zmian tras/schematu HTTP. [ADC_SID.md](ADC_SID.md) opisuje czas życia, pojemność i kolejność zwalniania.

## API INF — alpha-0.2.6

`adc/identity_hash.hpp` udostępnia Tiger/192 i kanoniczne base32 39-znakowe. `adc/inf.hpp` dodaje InfValidator, ClientInfo tylko do odczytu i ClientSession. Snapshot udostępnia opcjonalne pożyczone widoki pól, sprawdzone liczby i własny publiczny Message BINF bez PD. `observe` zwraca opcjonalny `variant<StateError, SupportError, SidError, InfError>`; błąd zachowuje stan/metadane/rezerwacje. Zatwierdzenie tożsamości wymaga poprawnego INF, ale nadal deleguje rejestr/dopuszczenie i decyzje kont. Trasy/schemat/liczniki HTTP są bez zmian. Zobacz [ADC_INF.md](ADC_INF.md).

## API CID/PID — alpha-0.2.7

`adc/tiger_identity.hpp` udostępnia TigerCid::parse/text/bytes i verify_tiger_identity(cid, pid, hash), zwracający variant<VerifiedIdentity, IdentityError>. Zachowany jest tylko publiczny CID. `ClientInfo::identity()` daje opcjonalną własną kopię wyniku. `decode_tiger_base32` i `tiger192_into` zapisują 24-bajtowy bufor właściciela i czyszczą go po błędzie; stare funkcje optional pozostają zgodne. Końcowe porównanie używa CRYPTO_memcmp. Brak zmian tras/schematu HTTP i API uwierzytelniania. Zobacz [ADC_IDENTITY.md](ADC_IDENTITY.md).


## alpha-0.2.8

Schemat API v1 bez zmian. active_connections liczy gniazda, adc_frames składnię; żaden nie liczy zalogowanych użytkowników. Gotowość gościa to stan reaktora i opcjonalne logi. Zobacz [ADC_LOGIN.md](ADC_LOGIN.md).

Bieżące dodatkowe pola statusu, próbkowanie i zgodność: [Statistics](STATISTICS.md).
