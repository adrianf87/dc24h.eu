# WebAdmin — alpha-0.4.12

**Bieżący etap — alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

**Etap dostarczony w alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

**Bieżący etap — alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) przekazuje sprawdzone SCH przez rzeczywiste ADC/ADCS: broadcast B/F i D/E związane z życiem celu współdzielą jeden niezmienny pakiet z oddzielnymi ograniczonymi kolejkami. Podane TO jest rejestrowane w Search Core; brak TO nadal jest poprawny. Obowiązują kontrole źródła/stanu/funkcji, jednokrotny dispatch i sprzątanie tokenów. RES/routing wyników pozostaje kolejnym etapem.

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

Zaimplementowano projekt React 19 / TypeScript / Vite w `webadmin/`, responsywny panel tylko do odczytu, katalogi EN/US i PL, pięć widoków opartych na hash (przegląd, połączenia, czat, bezpieczeństwo, baza), odświeżanie ręczne/co 5 sekund, ograniczoną historię obserwacji sesji oraz eksport JSON. Konfiguracja i logi są jawnie wyłączone jako przyszłe funkcje. Brak kont/logowania/ról, zmian administracyjnych, trwałych metryk i zmyślonych danych.

Domyślny panel pokazuje oczekiwanie/brak połączenia/błędy, a niedostępne liczniki kreskami. Dane pojawiają się tylko po poprawnej odpowiedzi API; błąd odświeżenia czyści wyświetlany stan i wykres. Znacznik ostatniego odczytu dotyczy ostatniej udanej próbki. Flaga bazy określa konfigurację przy starcie, nie bieżącą dostępność SQL. Liczniki sieci są niezależnymi odczytami; pamięć przeglądarki przechowuje do 30 ostatnich udanych obserwacji i zeruje historię po wykryciu spadku uptime. Logowanie gości ADC jest zaimplementowane; uwierzytelnianie kont i administracyjne sesje przeglądarkowe pozostają osobnymi przyszłymi etapami.

## Wymagania i budowanie

Node.js `^20.19.0 || >=22.12.0` i npm obsługujące package-lock v3. Zależności są przypięte w pliku lock; odtwarzalna instalacja to `npm ci`. Domyślne budowanie C++ nie wymaga Node/npm/przeglądarki. Pliki są UTF-8, tłumaczenia w `webadmin/src/languages/us` i `pl`. Domyślny język to EN/US; wyłącznie preferencja języka trafia do localStorage (z bezpiecznym fallbackiem po blokadzie pamięci).

```sh
cd webadmin
npm ci
npm run build
npm run dev
```

Vite nasłuchuje na `127.0.0.1:5173`, ze stałym portem, wyłączonym CORS i ograniczonym serwowaniem plików. Bez adaptera wartości pozostają niedostępne. Panel nie prosi o wklejenie tokena do przeglądarki. `npm run preview` serwuje build na `127.0.0.1:4173`, z celowo wyłączonym adapterem. Stop przez Ctrl+C; to lokalne narzędzia deweloperskie, nie usługa produkcyjna.

## Opcjonalny lokalny adapter deweloperski

Przeglądarka nie łączy się z gniazdem Unix. Plugin Vite może pośredniczyć w ograniczonych żądaniach tylko do odczytu przez klienta HTTP Node. Uruchom C++ z prywatnym API według [API.md](API.md), a Vite jako **ten sam efektywny UID**. Katalog gniazda musi pozostać prywatny i należeć do tego UID; użyj istniejącego prywatnego pliku tokena 64-hex. Obie ścieżki są absolutne; token musi być poza katalogiem WebAdmin i nie może być końcowym symlinkiem.

```sh
cd webadmin
DC24H_API_SOCKET=/absolutny/prywatny/katalog/admin.sock \
DC24H_API_TOKEN_FILE=/absolutny/prywatny/katalog/admin.token \
  npm run dev
```

Te zmienne serwera zawierają ścieżki, nigdy wartość tokena. Nie używaj `VITE_*` dla sekretów. Adapter odczytuje token przy starcie, sprawdza właściciela/uprawnienia/liczbę dowiązań/format i odrzuca plik w serwowanym katalogu. Rotacja wymaga restartu Vite. Przeglądarka odpytuje względne `./admin-api/v1/status`, bez tokena w żądaniach, JavaScript, URL i pamięci przeglądarki. Reguły Vite blokują również pliki tokenów/haseł/kluczy/certyfikatów, `.env` i metadane Git.

Pośrednictwo obejmuje tylko GET `/admin-api/v1/status` i `/admin-api/v1/health`, mapowane na C++ `/api/v1/`. Inne metody/ścieżki, body, klienci spoza loopback, obcy Host/Origin i cross-site Fetch Metadata są odrzucane. Authorization klienta jest ignorowane; adapter dodaje własny Bearer. Limity: 8 aktywnych żądań, 2 sekundy bezwzględnego czasu i 16 KiB odpowiedzi. Błędy są stałe i nie ujawniają szczegółów. JSON jest sprawdzany/normalizowany, nieznane pola usuwane. Przeglądarka niezależnie sprawdza schemat i bezpieczne nieujemne liczby całkowite JavaScript, z terminem 4 sekund i limitem 16 KiB. Liczniki powyżej `Number.MAX_SAFE_INTEGER` są odrzucane zamiast po cichu zaokrąglane.

Plugin to **lokalny adapter deweloperski**, nie produkcyjne uwierzytelnianie WWW. Lokalny komputer pozostaje zaufany; brak sesji/tożsamości/ról, CSRF dla przyszłych zmian, TLS, audytu i bramy Apache między UID. Nie udostępniaj Vite w sieci ani nie wyłączaj ochrony hosta/plików. Nie trzeba osłabiać uprawnień gniazda C++.

## Układ i zachowanie

Grafitowa nawigacja, jasny obszar pracy i niebieskie kontrolki tworzą podstawowy spójny układ. Ikony Lucide są bundlowane; brak zdalnych skryptów/fontów/obrazów. Panel ma responsywną nawigację, widoczny fokus, link pomijania, semantyczną tabelę/status, nazwane kontrolki i ograniczanie animacji. EN/PL zmienia etykiety i język dokumentu; identyfikatory protokołu pozostają bez zmian. Eksport jest aktywny tylko dla bieżącego zweryfikowanego odczytu; zawiera dozwolone pola i `observed_at`, bez sekretów/konfiguracji. Linki dokumentacji respektują język.

## Apache2 i opcjonalna instalacja

`npm run build` tworzy statyczne pliki w `webadmin/dist`, ze względnymi ścieżkami zasobów i nawigacją hash. Do builda nie trafia serwer Node, token ani plugin deweloperski. Opcjonalna instalacja przez CMake:

```sh
npm --prefix webadmin ci
npm --prefix webadmin run build
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DDC24H_INSTALL_WEBADMIN=ON
cmake --build build-release --parallel 2
sudo cmake --install build-release --prefix /usr/local
```

Instalowane są zasoby `share/dc24h/webadmin` i przykład `etc/dc24h/apache2-webadmin.conf.example`. Opcja domyślnie OFF; ON wymaga istniejącego `webadmin/dist/index.html`. Zawsze buduj frontend z tego samego checkoutu; CMake nie uruchamia npm i nie weryfikuje pochodzenia gotowych zasobów. Instalacja nie aktywuje konfiguracji i nie startuje usługi.

[Przykład Apache2](../../config/apache2-webadmin.conf.example) to **statyczny lokalny podgląd** `127.0.0.1:8080`: dostęp lokalny, bez indeksowania katalogów, nagłówki CSP/no-sniff/frame/referrer i jawna blokada `/admin-api/`. Wymaga mod_headers oraz standardowych modułów katalogów/autoryzacji, z czytelnymi plikami zainstalowanego builda. Dostosuj ścieżki/porty i sprawdź `apache2ctl configtest` przed świadomym włączeniem. Przykład celowo nie przekazuje żądań do prywatnego API; panel statyczny nie otrzymuje wtedy danych. Produkcyjna brama, sesje/role WWW, wdrożenie pozostają planowane. Nie przekazuj tokena usługi do JavaScript ani nie udostępniaj prywatnego gniazda C++ innemu UID.

## Weryfikacja

```sh
cd webadmin
npm run build
npm test
npx playwright install --with-deps chromium
npm run test:e2e
npm run test:integration -- ../build/dc24h
```

Przy zainstalowanym Chromium można ustawić `PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium`. Testy używają tymczasowych sekretów/gniazd. Jednostkowe sprawdzają schemat/precyzję, normalizację, pliki prywatne, Origin/Host/metody/ścieżki, wstrzykiwanie tokena, przeciążenie, rozmiary/czas i błędy bez szczegółów. Przeglądarkowe obejmują brak/dostępność/błędne/utracone dane, nawigację, trwałość języka, eksport i szerokość mobilną. Harness integracyjny uruchamia rzeczywisty tymczasowy C++, osobne Vite i Chromium; sprawdza liczniki ADC, izolację tokena, desktop/mobile PL i sprząta procesy. CTest/ncdc/MariaDB pozostają osobnymi zestawami.

Źródła: [Vite — uruchomienie](https://vite.dev/guide/), [ograniczenia serwera Vite](https://vite.dev/config/server-options), [React i TypeScript](https://react.dev/learn/typescript).

## Przykład HTTPS — alpha-0.1.11

Osobny szablon statycznej witryny HTTP→HTTPS/TLS 1.2+ jest dostępny i zawsze instalowany z przykładami konfiguracji C++. Zachowuje domyślny dostęp lokalny/blokadę API; nie jest produkcyjną bramą uwierzytelnioną. Ścieżki certyfikatów, dostosowanie, włączenie i testy: [APACHE2_HTTPS.md](APACHE2_HTTPS.md).
