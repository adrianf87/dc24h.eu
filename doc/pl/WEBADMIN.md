# WebAdmin — alpha-0.1.10

**Bieżący etap — alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizuje wybór zakresu MSG i dostarczanie do ograniczonych kolejek. Plany są powiązane z domeną macierzystego Chat Core i ID życia sesji; prywatne wiadomości nigdy nie trafiają zastępczo na czat publiczny.

**Etap dostarczony w alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) dodaje DMSG tylko do celu i EMSG do celu oraz nadawcy, pojedyncze wiadomości do siebie, powiązanie celu z ID życia i kontrolę PM. Czat publiczny pozostaje oddzielony; grupy/historia/moderacja to kolejne etapy.

**Etap dostarczony w alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) dodaje osobny adapter polityki i rozsyłania BMSG oraz jedną wspólną niemutowalną ramkę na publikację. Offsety i logiczne budżety odbiorców pozostają niezależne; zachowano bramki źródła/stanu/prywatności i FIFO.

**Etap dostarczony w alpha-0.4.2:** [Chat Core](CHAT_CORE.md) dodaje osobny silnik wiadomości niezależny od protokołu oraz publiczne BMSG, z kopią dla nadawcy i /me. Czat prywatny/grupowy/feature/hub, moderacja i historia pozostają osobnymi etapami.

**Etap dostarczony w alpha-0.4.1:** [ADC MSG](ADC_MSG.md) dodaje kodek MSG z typowanymi polami PM/ME, sprawdzanym budowaniem komunikatów, ograniczoną serializacją i walidacją w maszynie stanów. Dostarczanie czatu i polityka rozmów pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.3.13:** [Statistics](STATISTICS.md) dodaje liczniki cyklu życia w czasie stałym, użytkowników online/szczyt i sumę logowań, atomowe statystyki transportu, pola chronionego API oraz metryki WebAdmin US/PL. Starsze etapy poniżej zachowują swój zakres historyczny.

Zaimplementowano projekt React 19 / TypeScript / Vite w `webadmin/`, responsywny panel tylko do odczytu, katalogi EN/US i PL, cztery widoki oparte na hash (przegląd, połączenia, bezpieczeństwo, baza), odświeżanie ręczne/co 5 sekund, ograniczoną historię obserwacji sesji oraz eksport JSON. Konfiguracja i logi są jawnie wyłączone jako przyszłe funkcje. Brak kont/logowania/ról, zmian administracyjnych, trwałych metryk i zmyślonych danych.

Domyślny panel pokazuje oczekiwanie/brak połączenia/błędy, a niedostępne liczniki kreskami. Dane pojawiają się tylko po poprawnej odpowiedzi API; błąd odświeżenia czyści wyświetlany stan i wykres. Znacznik ostatniego odczytu dotyczy ostatniej udanej próbki. Flaga bazy określa konfigurację przy starcie, nie bieżącą dostępność SQL. Liczniki sieci są niezależnymi odczytami; pamięć przeglądarki przechowuje do 30 ostatnich udanych obserwacji i zeruje historię po wykryciu spadku uptime. Pełnego logowania ADC nadal nie zaimplementowano.

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
