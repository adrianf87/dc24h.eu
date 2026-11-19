# Budowanie

**alpha-0.7.15 Configuration:** [startowe ustawienia bazy i logowania](DATABASE_AUTH_CONFIGURATION.md) dodają wspólne dziedziczenie timeoutów, zakresy także przy wyłączonym logowaniu, bezpieczny podgląd --show-configuration i stronę Konfiguracja EN/PL tylko do odczytu. Migawki nie zawierają poświadczeń ani prywatnych ścieżek; zmiany wymagają sprawdzenia i restartu. CI sprawdza cztery logiczne CPU na runner i używa nproc bez stałego limitu workerów. Nie czekamy na zdalne CI.

**alpha-0.7.14 WebAdmin Accounts:** [lokalne zarządzanie kontami](WEBADMIN_ACCOUNTS.md) dodaje stronicowanie metadanych, tworzenie wyłączonych kont, aktywację, wyłączenie/reset i zmianę hasła przez osobny program bez listenerów huba. Przed zapisem zatrzymaj hub; operacje chronią hasło administratora i rewizje. Schemat v6 i migawki uwierzytelniania pozostają. CI używa czterech workerów na polecenie; nie czekamy na jego zakończenie.

**alpha-0.7.13 Database Resilience:** [jawne odnowienie połączenia i niezależne limity czasu](DATABASE_RESILIENCE.md) dodają pojedynczą próbę reconnect w wątku właściciela, ponowną walidację pliku hasła oraz ochronę przed powtarzaniem SQL i zastąpieniem aktywnej transakcji. Schemat v6 i migawki uwierzytelniania pozostają. CI korzysta z runnerów GitHub-hosted Ubuntu 24.04; nie czekamy na jego zakończenie. Ten wpis zastępuje wcześniejsze opisy braku reconnect.

**Aktualny etap — alpha-0.7.5:** [Rejestracja](REGISTRATION.md) atomowo tworzy konta registered/disabled i osobno przechowywane weryfikatory haseł OpenSSL scrypt z losową solą. Migracja v4 dodaje poświadczenia; walidacja Unicode, stały koszt KDF i rollback zapobiegają częściowym kontom. Uwierzytelnianie sesji i publiczna rejestracja API/WebAdmin pozostają planowane. CI zachowuje przypięty workflow Proxmox Ubuntu 24.04/22.04 bez scalania github-runner-v0.1.

**Aktualny etap — alpha-0.7.2:** [Migracje bazy danych](DATABASE_MIGRATIONS.md) dodają uporządkowany kompilowany katalog SQL, dokładne sumy, odczyt `--database-status`, zachowanie v1 przy aktualizacji i trwały stan applying/applied przyszłych zmian po inicjalizacji. Migracje pozostają jawne; bez schematu aplikacji i automatycznego odtwarzania DDL.

**Etap dostarczony w alpha-0.7.1:** [Database Core](DATABASE_CORE.md) dodaje typowane parametry, ograniczone wyniki na własność, metadane zapisu i transakcje RAII właściciela wątku, z redakcją błędów, blokadą zatwierdzania po błędzie i zamykaniem połączenia bez odtwarzania zapisów. Zachowano lokalne poświadczenia/migracje i CI wyłącznie na Ubuntu.

**Etap dostarczony w alpha-0.6.13:** [Konfiguracja transferów](TRANSFER_CONFIGURATION.md) dodaje dokładną ograniczoną politykę startową `allowed_protocols`, wspólne egzekwowanie CTM/RCM w Core oraz efektywne limity/protokoły tylko do odczytu w zabezpieczonym API i dwujęzycznej stronie Połączenia. Domyślne wartości zachowują routing; zmiany wymagają restartu.

**Aktualny etap — alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) dodaje stałe zbiorcze liczniki wyników CTM/RCM, kolejek/błędów i raportów klientów, rozszerzenie zabezpieczonego API oraz dwujęzyczny monitoring Połączeń. [Debian 13 CI](CI_DEBIAN13.md) dodaje sekwencyjną macierz Debug/Release własnego runnera i środowisko akcji Node 24, zachowując Node 22 dla WebAdmina.

**Aktualny etap — alpha-0.6.10:** [Transfer Limits](TRANSFER_LIMITS.md) dodaje wspólne budżety liczby/bajtów CTM/RCM nadawcy i sumy żądań odbiorcy, ograniczone prywatne odmowy oraz monotoniczne odnowienie. Konfiguracja jest walidowana przy starcie; zachowano sprzątanie sesji i istniejące kontrole bezpieczeństwa.

**Lokalna walidacja alpha-0.6.5 (2026-11-13):** Wszystkie **141/141 testów CTest** przeszły osobno w Debug, Release i ASan/UBSan z wymaganymi narzędziami i domyślnym wykrywaniem wycieków. Sprawdzono wybór/własność/błędy protokołu, jawny routing nazw znanych/rozszerzeń, fragmentowane błędy klientów IPv4/IPv6 mieszanych ADC/ADCS, cztery zestawy rzeczywistego ncdc z TLS klientów i weryfikacją certyfikatu, wcześniejsze zwykłe połączenia i regresję terminala ncdc, Search/UDP, MariaDB i Apache HTTPS. Każdy z 11 aktywnych scenariuszy ncdc przeszedł również trzy kolejne uruchomienia ASan/UBSan (33 wyniki poprawne) z rezerwacją portów. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, języki/linki, składnia zmienionych plików Python, przykładowy INI i białe znaki. Pełna macierz wspieranych Debian/Ubuntu i obciążenie produkcyjne pozostają niesprawdzone; zdalne CI raportuje oddzielnie GitHub Actions.

**Bieżący etap — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) dodaje do Transfer Core własne wartości protokołu i jawne rozróżnienie zwykłego ADC/1.0 oraz wymagającego TLS ADCS/1.0. Czysty wybór preferencji/możliwości nie obniża wymaganego TLS. CTM/RCM zachowują wybraną nazwę i zgodność rozszerzeń, a odpowiedzi błędów klientów zachowują tokeny. Rzeczywisty ncdc sprawdza TLS klientów z weryfikacją certyfikatu przez oba transporty huba; zachowano regresję pełnego terminala.

**Lokalna walidacja alpha-0.6.4 (2026-11-13):** Wszystkie **134/134 testy CTest** przeszły osobno w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracji i domyślnym wykrywaniem wycieków, w tym wspólna macierz SID/błędy/czasy życia, równoległe pary mieszanych ADC/ADCS IPv4/IPv6 z tym samym tokenem, rzeczywisty ncdc CTM/RCM i handshake klientów, wcześniejsze Search/UDP, MariaDB i Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, zgodność języków/linków, składnia zmienionych plików Python, przykładowy INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niesprawdzone.

**Etap dostarczony w alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) łączy przyjmowanie i dostarczanie D/E CTM oraz RCM we wspólnym routerze SID, jednym typie niezmiennego pakietu i jednej ścieżce ograniczonych kolejek. Odbiorcami są ustalone okresy życia gotowych sesji: D tylko cel, E cel i niezależne echo źródła. Token nie wybiera odbiorców; zachowano role, walidację, błędy i prywatność. Testy równoległych par używają tego samego tokenu bez mieszania odbiorców.

**Lokalna walidacja alpha-0.6.3 (2026-11-13):** Wszystkie **132/132 zestawy CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami i domyślnym wykrywaniem wycieków, w tym błędy/kolejki/czasy życia RCM, fragmentowane IPv4/IPv6 ADC/ADCS, rzeczywisty ncdc RCM→CTM i handshake obu ról, wcześniejsze CTM/Search/UDP, MariaDB i Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, języki/linki, składnia zmienionych plików Python, INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie pozostają niesprawdzone.

**Etap dostarczony w alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Lokalna walidacja alpha-0.6.2 (2026-11-13):** Wszystkie **128/128 zestawów CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami i domyślnym wykrywaniem wycieków, w tym błędy/kolejki/czasy życia CTM, fragmentowane IPv4/IPv6 ADC/ADCS, rzeczywisty handshake ncdc CTM–CSUP/CINF/CGET, poprzednie Search/UDP, MariaDB i Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, języki/linki dokumentacji, składnia zmienionych plików Python, INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie pozostają niesprawdzone.

**Etap dostarczony w alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

**Lokalna walidacja alpha-0.6.1 (2026-11-13):** Wszystkie **124/124 zestawy CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami i domyślnym wykrywaniem wycieków, w tym błędy/czasy życia Transfer Core, granice ADC/ADCS, rzeczywista odmowa listy plików ncdc i działający po niej czat, dotychczasowe Search/UDP, MariaDB i Apache HTTPS. Końcowy test zachowania próby i przenoszenia planu ponownie przeszedł w Debug i Release. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, języki/linki dokumentacji, składnia zmienionych plików Python, INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niesprawdzone.

**Etap dostarczony w alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) dodaje niezależny silnik C++20 koordynacji połączeń: ograniczone własne żądania, role aktywne/pasywne IPv4/IPv6 i niezmienne plany jednej próby związane z sesjami, anulowaniem i ponownymi kontrolami. Wspólna klasyfikacja adresów Users zachowuje Search. Routing CTM/RCM i dane plików pozostają kolejnymi etapami.

**Lokalna walidacja alpha-0.5.13 (2026-11-13):** Przeszły build serwera Debug i wszystkie **120/120 zestawów CTest** z wymaganymi narzędziami, w tym rzeczywistym ncdc ADC/ADCS/UDP, MariaDB i Apache HTTPS. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** oraz integracja API C++ z React potwierdzająca dodatnie tempo wyszukiwań. Sprawdzono widoki desktop i telefon PL. Poprawne są metadane, UTF-8, zgodność języków/linków, składnia zmienionych plików Python, przykładowy INI i białe znaki; brak ostrzeżeń kompilatora. Etap zmienia monitoring w panelu i metadane wydania, bez zmiany algorytmu wyszukiwania/API C++. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne nie zostały zweryfikowane.

**Etap dostarczony w alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

**Weryfikacja alpha-0.5.12 (historyczna):** Weryfikacja lokalna 2026-11-13: **120/120 zestawów CTest przeszło osobno w Debug, Release i ASan/UBSan**, z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym rzeczywistym ncdc ADC/ADCS/UDP, MariaDB i Apache HTTPS. Bez ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, 6 testów Node, 8 testów Chromium i integracja z API C++. Poprawne są metadane, UTF-8, zgodność plików językowych i linków dokumentacji, składnia zmienionych plików Python, przykładowy INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne nie zostały zweryfikowane.

**Etap dostarczony w alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

**Etap dostarczony w alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) dodaje limity liczby i bajtów zapytań oraz wysyłanych/odbieranych wyników na czas życia połączenia, ograniczone powiadomienia, politykę gości/globalną, zaufane uprawnienia sesji i konfigurowalne limity aktywnych wyszukiwań. Kontrola poprzedza routing/rejestrację tokenu. Opcjonalny wymagany token uzupełnia kontrolę właściciela/generacji; bezpośrednie UDP klientów jest poza kontrolą huba.

**Aktualizacja — alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) przenosi sprawdzone I4/I6/U4/U6 do publicznego modelu Users i dodaje metadane dróg odpowiedzi TCP/UDP dla każdej rodziny do Search Core oraz niezmiennych planów. Pasywne DRES nadal przechodzi przez hub, a aktywne URES bezpośrednio między klientami. Testowane są zmiany INF, własność adresów i rzeczywiste wysyłanie/odbieranie UDP przez ncdc.

**Aktualizacja — alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) przekazuje sprawdzone DRES wyłącznie do gotowego celu przez ADC/ADCS. Plan wiąże czas życia sesji oraz — gdy podano TO — generację aktywnego wyszukiwania właściciela celu. Nadal obsługiwane są wyniki bez tokenu. Współdzielone ograniczone kolejki zachowują oryginalne pola bez rozsyłania publicznego, echa do nadawcy i treści w administracji.

**Aktualizacja — alpha-0.5.8:** [ADC RES](ADC_RES.md) dodaje typowany parser DRES z własnymi danymi i rzeczywistą walidację ADC/ADCS nazw, rozmiarów, slotów, tokenów i kanonicznych korzeni TTH po kontroli sesji/źródła. Błędy semantyczne są odwracalne i nie ujawniają treści; poprawne wyniki otrzymują jawną informację o oczekującym routingu. Korelacja tokenów, przekazywanie wyników i UDP pozostają kolejnymi etapami.

**Aktualizacja — alpha-0.5.7:** [Feature Routing](FEATURE_ROUTING.md) kompiluje selektory FSCH raz i korzysta z aktualnego publicznego INF SU: wszystkie wymagane, żadna wykluczająca. Negocjacja SUP jest niezależna. Zmiany możliwości, udział źródła, nieznane rewizje i limity sprawdzono przez ADC/ADCS i rzeczywisty ncdc. RES/routing wyników pozostaje kolejnym etapem.

**Etap dostarczony w alpha-0.5.6:** [Search Routing](SEARCH_ROUTING.md) przekazuje sprawdzone SCH przez rzeczywiste ADC/ADCS: broadcast B/F i D/E związane z życiem celu współdzielą jeden niezmienny pakiet z oddzielnymi ograniczonymi kolejkami. Podane TO jest rejestrowane w Search Core; brak TO nadal jest poprawny. Obowiązują kontrole źródła/stanu/funkcji, jednokrotny dispatch i sprzątanie tokenów. RES/routing wyników pozostaje kolejnym etapem.

**Etap dostarczony w alpha-0.5.5:** [Search Tokens](SEARCH_TOKENS.md) dodaje ograniczony rejestr życia sesji/TO, lokalne generacje wyszukiwań, terminy ważności, anulowanie i jawne API aktywnych wyszukiwań Core. Nadal można przygotować plan bez rejestracji; odrzucone rzeczywiste SCH nie rezerwuje tokenów. Korelacja/routing wyników i administracja wyszukiwaniem pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.4:** [TTH Search](TTH_SEARCH.md) przekazuje pełny 192-bitowy hash pliku z SCH do niezmiennych zapytań/planów Core, opisów i filtrów. Zapytania z samym hashem lub łączone używają dokładnej równości i zachowują dodatkowe kryteria. Testowane są rzeczywiste zapytania TTH ncdc ADC/ADCS; routing zapytań/wyników pozostaje kolejnym etapem.

**Etap dostarczony w alpha-0.5.3:** [Search Filters](SEARCH_FILTERS.md) dodaje skompilowane filtry nazw/ścieżek Unicode bez rozróżniania wielkości liter, alternatywy rozszerzeń, domknięte/dokładne limity bajtów i typ plik/katalog dla sprawdzonych opisów. Zapytania z samymi filtrami i nieprzezroczyste tokeny NFC przechodzą do Search Core. Dopasowanie nie alokuje pamięci; routing/wyniki, indeksy i administracja wyszukiwaniem pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.2:** [ADC SCH](ADC_SCH.md) dodaje własny typowany parser SCH i odbiór w ADC/ADCS po sprawdzeniu stanu/źródła. Sprawdza pola, selektory, limity i kanoniczne TR; poprawne zapytania otrzymują informację o braku routingu, błędne semantycznie odwracalną odmowę, a zapytania bez kryteriów są pomijane. Routing/wyniki pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.1:** [Search Core](SEARCH_CORE.md) dodaje niezależną bibliotekę C++20 dla ograniczonych, niezmiennych zapytań tekstowych, wyboru gotowych sesji/funkcji oraz dispatch związanego z czasem życia i domeną silnika. Parser, serializacja i podłączenie SCH/RES do transportu pozostają kolejnymi etapami; działający serwer nadal zwraca jawną odmowę nieobsługiwanej komendy.

**Bieżący etap — alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) grupuje startowe ustawienia czatu i dodaje limit odkodowanego tekstu, niezależne przełączniki publicznego/prywatnego/akcji i opcjonalne własne powitanie. Domyślne wartości zachowują działanie; odwracalne odmowy nie omijają walidacji ani Anti-Flood.

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

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Pełny zestaw integracji wymaga `-DDC24H_REQUIRE_INTEGRATION_TESTS=ON` przy włączonym BUILD_TESTING. Potrzebne są openssl, ncdc, narzędzia serwera/klienta MariaDB i Apache2/mod_ssl; CI używa tego trybu. Brak narzędzi przerywa konfigurację. Domyślne OFF zachowuje opcjonalność testów dla deweloperów. `project.metadata` potrzebuje tylko Pythona i sprawdza spójność wersji/dokumentacji. Zobacz [audyt](AUDIT_ALPHA_0.1.md).

Wymagane: Linux, CMake >=3.22, kompilator C++20 (GCC >=11), pthreads, utf8proc 2.x i pliki deweloperskie OpenSSL >=1.1.1. Testy używają Pythona >=3.8 i programu openssl. ncdc dodaje dwa testy rzeczywistego klienta; bez niego CMake jawnie informuje o pominięciu. Zależności nie są pobierane podczas konfiguracji.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
ctest --test-dir build-release --output-on-failure
```

`-DBUILD_TESTING=OFF` wyłącza testy i wymaganie Pythona. `cmake --install build-release --prefix /usr/local` instaluje sam program (może wymagać sudo). Katalogi językowe są wkompilowane. Wersja pochodzi z CMake; `--version` zwraca alpha-0.4.10. Dokładną kompilację identyfikuje także `git rev-parse HEAD`. Nie edytuj plików generowanych. Wymagane są biblioteki Connector/C MariaDB; działający serwer SQL jest opcjonalny. Node.js i Apache2 nie są wymagane do budowania C++; frontend ma osobne wymagania.

Jeden sanitizer na build: [Bezpieczeństwo pamięci](MEMORY_SAFETY.md). `DC24H_SANITIZER` przyjmuje `address`, `undefined` albo `thread`. Zwykły Debug i Release zostawiają przełącznik pusty. Nie łącz sanitizerów w jednym programie. Osobne budowanie frontendu: [WEBADMIN.md](WEBADMIN.md).

Instalacja obejmuje program i `etc/dc24h/dc24h.ini.example` względem wybranego prefiksu; nie instaluje aktywnej konfiguracji ani nie uruchamia usługi.

Wymagany jest Connector/C (`libmariadb-dev`). Zainstaluj `mariadb-server`, aby uruchomić opcjonalny test `database.process`; CMake informuje o brakujących narzędziach. Instalowana jest też niezmienna migracja SQL w `share/dc24h/migrations`.

## Budowanie WebAdmin

Użyj wspieranego Node/npm, potem `npm --prefix webadmin ci` i `npm --prefix webadmin run build`. Zasoby są niezależne od domyślnego CMake. Instalacja gotowego frontendu wymaga `-DDC24H_INSTALL_WEBADMIN=ON`; operator odpowiada za build z tego samego checkoutu. Testy przeglądarkowe i Apache2: [WEBADMIN.md](WEBADMIN.md).

## Zależność INF — alpha-0.2.6

Zainstalować `libgcrypt20-dev` obok dotychczasowych zależności C++. CMake wymaga gcrypt.h/libgcrypt dla dc24h_adc. Tożsamość INF używa GCRY_MD_TIGER1 (standardowy Tiger/192); brak algorytmu/kontekstu odrzuca walidację. Moduł nie zmienia FIPS/globalnych trybów. To nie szyfry TLS ani obliczanie drzew Tiger plików.

## Testy awarii tożsamości — alpha-0.2.7

Brak nowej zależności poza Libgcrypt/OpenSSL. Izolowany adc_identity_failure_tests wymaga linkera Linux zgodnego z GNU i --wrap; CMake stosuje flagi wyłącznie do tego testu. Program serwera nie zawiera wrapperów ani interfejsu wstrzykiwania kryptografii.
