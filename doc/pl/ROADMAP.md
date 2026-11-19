# Plan rozwoju

**alpha-0.9.6 Bezpieczeństwo konta:** [Hasła, GPA/PAS i ochrona sekretów](ACCOUNT_SECURITY.md) odmawia hasła ADC identycznego z hasłem konta i odszyfrowuje to hasło dopiero po budżecie prób konta i sprawdzeniu bana. Zgadywanie online, przechwycony PAS przy wyłączonym require_tls oraz kubełki czyszczone restartem zostają. Schemat v9 pozostaje bez zmian.
**alpha-0.9.5 Bezpieczeństwo TLS:** [Polityka certyfikatu i szyfrów ADCS](TLS_SECURITY.md) sprawdza certyfikat liścia huba przed nasłuchem i ogranicza ADCS do TLS 1.2 i 1.3 AEAD. Zaufanie, nazwa hosta, HTTP/2, OCSP i automatyczne odnawianie pozostają osobną pracą. Schemat v9 pozostaje bez zmian.
**alpha-0.9.4 Limity ruchu:** [Spam i nadmierny ruch obecności](TRAFFIC_LIMITS.md) ogranicza aktualizacje INF gotowej sesji przed rozesłaniem. Budżety czatu, wyszukiwania i transferów zostają. Schemat v9 pozostaje bez zmian. Limit pasma i publiczna brama pozostają osobną pracą.
**alpha-0.9.3 Bezpieczeństwo sieci:** [Ochrona połączeń TCP, timeouty i limity buforów](NETWORK_SECURITY.md) ogranicza kolejkę nasłuchu, bufory jądra wysyłania i odbioru każdego przyjętego gniazda oraz TCP user timeout. Dotychczasowe timeouty aplikacji i kolejki wyjścia zostają. Schemat v9 pozostaje bez zmian. Limit pasma i publiczna brama pozostają osobną pracą.
**alpha-0.9.2 Bezpieczeństwo ADC:** [Walidacja parsera, fuzzing i kontrola protokołu](ADC_SECURITY.md) sprawdza każdą sparsowaną ramkę klienta względem profilu poleceń huba, zanim zmieni się stan sesji. Deterministyczna kampania fuzzingu w procesie spina parser, dekoder strumienia i ten profil. Schemat v9 pozostaje bez zmian. Fuzzing sterowany pokryciem i publiczna brama pozostają osobną pracą.
**alpha-0.9.1 Model zagrożeń:** [Analiza zagrożeń huba i WebAdmin](THREAT_MODEL.md) zapisuje aktywa, aktorów, granice zaufania i ryzyko szczątkowe obecnego huba oraz lokalnego WebAdmin. Nie dodaje mechanizmu, zmiany schematu, publicznej bramy ani produkcyjnego audytu. Zdalny dostęp administratora pozostaje osobną pracą.
**alpha-0.8.15 Sesje bezpieczeństwa:** [Kontrola dostępu, CSRF, sesje i limity żądań](SECURITY_SESSIONS.md) wiąże każdy grant w pamięci procesu z publicznym identyfikatorem sesji i osobnym sekretem CSRF. Zapisy utrzymaniowe wymagają obu wartości na tym samym slocie. Każda sesja przyjmuje 24 żądania w ciągu 10 sekund. Odczyty nadal wymagają tylko grantu. Wartości zostają w pamięci procesu, nie w SQL, ciasteczkach ani pamięci przeglądarki. Publiczna brama z uwierzytelnieniem pozostaje osobną pracą.
**alpha-0.8.14 Proxy HTTPS Apache2:** [Odwrotne proxy HTTPS i ochrona API](APACHE2_PROXY.md) przekazuje dozwoloną listę API WebAdmin do adaptera na loopbacku. Token bearer klienta, ciasteczka i tożsamość z nagłówków proxy są usuwane. Gniazdo Unix C++ i token usługi pozostają poza Apache. Domyślnie dostęp jest lokalny.
**alpha-0.8.13 Zarządzanie WebAdmin:** [Zarządzanie banami, kontami i rolami](WEBADMIN_MANAGEMENT.md) pokazuje, które obszary utrzymania dopuszcza bieżący grant, i prowadzi do istniejących stron kont, ról i banów. Strona nie wykonuje tych zapisów. Zapis konfiguracji, bieżąca administracja huba oraz kick i mute w przeglądarce pozostają poza tą stroną.
**alpha-0.8.12 Użytkownicy WebAdmin:** [Lista użytkowników i działania moderacyjne](WEBADMIN_USERS.md) publikuje katalog gotowych sesji oraz dwujęzyczną stronę decyzji kick, mute i unmute. Strona nie wysyła tych poleceń, nie tworzy banów i nie ujawnia CID, IP, konta ani pól profilu. Polecenia kick pozostają w uwierzytelnionym kliencie Direct Connect.
**alpha-0.8.11 Panel WebAdmin:** [Panel główny i statystyki](WEBADMIN_DASHBOARD.md) dodaje stałe podsumowanie istniejących liczników procesu do uwierzytelnionej migawki statusu oraz dwujęzyczną stronę WebAdmin. Listy kont, rekordy banów, wiersze audytu, zapisy konfiguracji i bieżąca administracja huba pozostają poza tym panelem.
**alpha-0.8.10 API administracyjne:** [Udowodniony dostęp administracyjny](ADMIN_API.md) wymaga hasła aktywnego konta przed odczytem i zapisem konserwacyjnym. Token procesu pozostaje poświadczeniem transportu. Zapisy konfiguracji i bieżąca administracja huba pozostają w planach.
**alpha-0.8.9 Dziennik audytu:** [Historia udanych działań konserwacyjnych](AUDIT_LOGS.md) jest zapisywana w schemacie v9. Aktywne konto z uprawnieniem audit.read wyświetla tę historię. Kick i wyciszenie na żywo, odmowy, hasła oraz dowolny tekst powodu pozostają poza zapisem. Zapis konfiguracji i administracja huba pozostają w planach.

**alpha-0.8.8 Reguły moderacji:** [Wspólne sprawdzenie uprawnienia i celu](MODERATION_RULES.md) obejmuje kick, mute, unmute oraz bany konta, CID i adresu IP, razem z cofaniem. Schemat v8 pozostaje bez zmian.

**alpha-0.8.7 Mute:** [Moderacja czatu publicznego i prywatnego](MUTE.md) wdraża users.mute z uwierzytelnioną hierarchią, flagami sesji, +mute/+unmute i prywatnymi agregatami. Schemat v8 pozostaje bez zmian. Zastępuje wcześniejsze opisy planowanego mute.

**alpha-0.8.6 Temporary Bans:** [Czas trwania, automatyczne wygaśnięcie i czas serwera w UI](TEMPORARY_BANS.md) rozwijają Ban Core bez zmiany schematu v8 i migracji SQL 1–8. Nowy kontrakt czasu trwania i filtrów API opisano w podanym dokumencie.

**alpha-0.8.5 Ban Core:** [Trwałe blokady wejścia kont/CID/IP](BAN_CORE.md) dodają jawny schemat v8, dodawanie/cofanie z hasłem, egzekwowanie ze startu i wygaśnięcie. Admin Core ma dziesięć poleceń; users.ban/blocks.manage działają. Zastępuje wcześniejsze opisy v7/planowanych banów.

**alpha-0.8.4 Kick:** [Wyrzucanie aktywnych sesji](KICK.md) dodaje prywatne polecenie +kick, hierarchię, komunikaty końcowe, natychmiastowe odcięcie routingu i liczniki. Operacje serwisowe i schemat v7 pozostają.

**alpha-0.8.3 Admin Core:** [Centralny silnik poleceń serwisowych](ADMIN_CORE.md) obsługuje API kont i CLI Ownera, izolację punktów dostępu, wyniki, prywatny katalog i liczniki procesu. Dowody haseł, transakcje i schemat v7 pozostają.

**alpha-0.8.2 Operators:** [Spis zespołu i lokalne tworzenie/przekazanie własności](OPERATORS.md) zastępują wcześniejsze ograniczenia bootstrap/przekazania Ownera. Schemat v7 i zwykła ochrona Ownera pozostają.

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

**alpha-0.7.15 Configuration:** [startowe ustawienia bazy i logowania](DATABASE_AUTH_CONFIGURATION.md) dodają wspólne dziedziczenie timeoutów, zakresy także przy wyłączonym logowaniu, bezpieczny podgląd --show-configuration i stronę Konfiguracja EN/PL tylko do odczytu. Migawki nie zawierają poświadczeń ani prywatnych ścieżek; zmiany wymagają sprawdzenia i restartu. CI sprawdza cztery logiczne CPU na runner i używa nproc bez stałego limitu workerów. Nie czekamy na zdalne CI.

**alpha-0.7.14 WebAdmin Accounts:** [lokalne zarządzanie kontami](WEBADMIN_ACCOUNTS.md) dodaje stronicowanie metadanych, tworzenie wyłączonych kont, aktywację, wyłączenie/reset i zmianę hasła przez osobny program bez listenerów huba. Przed zapisem zatrzymaj hub; operacje chronią hasło administratora i rewizje. Schemat v6 i migawki uwierzytelniania pozostają. CI używa czterech workerów na polecenie; nie czekamy na jego zakończenie.

**alpha-0.7.13 Database Resilience:** [jawne odnowienie połączenia i niezależne limity czasu](DATABASE_RESILIENCE.md) dodają pojedynczą próbę reconnect w wątku właściciela, ponowną walidację pliku hasła oraz ochronę przed powtarzaniem SQL i zastąpieniem aktywnej transakcji. Schemat v6 i migawki uwierzytelniania pozostają. CI korzysta z runnerów GitHub-hosted Ubuntu 24.04; nie czekamy na jego zakończenie. Ten wpis zastępuje wcześniejsze opisy braku reconnect.

**alpha-0.7.12 Anti-Bruteforce:** [wspólne limity prób logowania](ANTI_BRUTEFORCE.md) dodają ograniczone pamięciowo budżety globalne/IP/konta, automatyczne odnawianie i ogólną odmowę dla ADC/ADCS oraz reconnect. Sukces nie zwraca prób; każda rola podlega limitom. Stan pozostaje w pamięci i nie zmienia kont w bazie.

**alpha-0.7.11 Account Management:** [zaufana zmiana hasła, wyłączenie i reset](ACCOUNT_MANAGEMENT.md) łączą atomowy zapis konta/haseł ze świeżym potwierdzeniem, rewizją i usunięciem danych ADC. Wykonuj przy zatrzymanym hubie, następnie jawnie zapisz ADC/aktywuj konto według potrzeby i uruchom hub ponownie. Bez przeładowania na żywo, nowej migracji SQL i publicznego API zarządzania.

**alpha-0.7.10 Roles:** [sprawdzone role kont](ROLES.md) są teraz przypisane do gotowych sesji, nadają uprawnienia Search/Transfers i pozwalają personelowi unieważniać sesje niższych ról. Limity globalne i jawne blokady połączeń nadal obowiązują. Zmiany roli w bazie wymagają restartu; bez nowej migracji SQL i publicznego API administracji.

**alpha-0.7.9 Auth Sessions:** [sesje kont związane z połączeniem](AUTH_SESSIONS.md) dodają atomowe powiązanie przy gotowości, zaufane odczyty według konta i unieważnianie połączenia. Zamykanie natychmiast usuwa uwierzytelnienie. Bez nowej migracji SQL, automatycznych ról i sesji przeglądarkowych. CI korzysta z [runnerów GitHub](GITHUB_CI.md) bez czekania na wynik.

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) wdraża logowanie kont GPA/PAS, osobne hasło ADC szyfrowane AES-256-GCM i prywatny klucz poza bazą, zachowując scrypt. Wymagane są jawna migracja v6, aktywne konto i dokładna para nick/CID; domyślnie obowiązuje ADCS. Zmiany i blokady w katalogu startowym wymagają restartu. Publiczne przygotowanie kont, reset, rotacja i nadawanie ról pozostają kolejnymi etapami. Ten wpis zastępuje wcześniejsze opisy planowanego uwierzytelniania.

**Dostarczono w alpha-0.7.7:** [Password Security](PASSWORD_SECURITY.md) dodaje zaufany serwis C++ tylko do odczytu, ścisły format i stały koszt scrypt, porównanie kluczy w stałym czasie oraz wymazanie własnych buforów. Dopasowanie hasła nie aktywuje konta ani sesji; publiczne logowanie, reset i przeliczanie KDF są kolejnymi etapami.

**Dostarczono w alpha-0.7.6:** [Identity Mapping](IDENTITY_MAPPING.md) dodaje niezmienne, unikalne przypisania konto/nick/CID i odczyt dokładnej pary w jawnym schemacie v5. To zaufana operacja zarządzania; uwierzytelnianie hasłem, uprawnienia sesji i cofanie przypisań są dalszymi etapami. [Contabo CI](CONTABO_CI.md) używa Ubuntu 24.04 i równoległości 6.

**Aktualny etap — alpha-0.7.5:** [Rejestracja](REGISTRATION.md) atomowo tworzy konta registered/disabled i osobno przechowywane weryfikatory haseł OpenSSL scrypt z losową solą. Migracja v4 dodaje poświadczenia; walidacja Unicode, stały koszt KDF i rollback zapobiegają częściowym kontom. Uwierzytelnianie sesji i publiczna rejestracja API/WebAdmin pozostają planowane. CI zachowuje przypięty workflow Proxmox Ubuntu 24.04/22.04 bez scalania github-runner-v0.1.

**Aktualny etap — alpha-0.7.2:** [Migracje bazy danych](DATABASE_MIGRATIONS.md) dodają uporządkowany kompilowany katalog SQL, dokładne sumy, odczyt `--database-status`, zachowanie v1 przy aktualizacji i trwały stan applying/applied przyszłych zmian po inicjalizacji. Migracje pozostają jawne; bez schematu aplikacji i automatycznego odtwarzania DDL.

**Etap dostarczony w alpha-0.7.1:** [Database Core](DATABASE_CORE.md) dodaje typowane parametry, ograniczone wyniki na własność, metadane zapisu i transakcje RAII właściciela wątku, z redakcją błędów, blokadą zatwierdzania po błędzie i zamykaniem połączenia bez odtwarzania zapisów. Zachowano lokalne poświadczenia/migracje i CI wyłącznie na Ubuntu.

**Etap dostarczony w alpha-0.6.13:** [Konfiguracja transferów](TRANSFER_CONFIGURATION.md) dodaje dokładną ograniczoną politykę startową `allowed_protocols`, wspólne egzekwowanie CTM/RCM w Core oraz efektywne limity/protokoły tylko do odczytu w zabezpieczonym API i dwujęzycznej stronie Połączenia. Domyślne wartości zachowują routing; zmiany wymagają restartu.

**Etap dostarczony w alpha-0.6.12:** [Monitoring transferów WebAdmin](WEBADMIN_TRANSFERS.md) dodaje obserwowane tempo CTM/RCM, odmów i kopii kolejek, ograniczony wykres oraz dostępną tabelę na stronie Połączenia. Lokalne obserwacje zerują się po błędzie, wykrytym restarcie i dłuższej przerwie; wspólne odpytywanie, pauza/ręczne odświeżanie, lokalne czyszczenie i EN/PL zachowują liczniki łączne oraz izolację poświadczeń.

**Etap dostarczony w alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) dodaje stałe zbiorcze liczniki wyników CTM/RCM, kolejek/błędów i raportów klientów, rozszerzenie zabezpieczonego API oraz dwujęzyczny monitoring Połączeń. [Debian 13 CI](CI_DEBIAN13.md) dodaje sekwencyjną macierz Debug/Release własnego runnera i środowisko akcji Node 24, zachowując Node 22 dla WebAdmina.

**Aktualny etap — alpha-0.6.10:** [Transfer Limits](TRANSFER_LIMITS.md) dodaje wspólne budżety liczby/bajtów CTM/RCM nadawcy i sumy żądań odbiorcy, ograniczone prywatne odmowy oraz monotoniczne odnowienie. Konfiguracja jest walidowana przy starcie; zachowano sprzątanie sesji i istniejące kontrole bezpieczeństwa.

**Bieżący etap — alpha-0.6.9:** [Error Handling](ERROR_HANDLING.md) ujednolica diagnostykę połączeń i wspólny niezmienny routing STA do stałej listy gotowych sesji. Przygotowanie, kolejki odbiorców, diagnostyka zamykania i wyjściowa walidacja STA izolują błędy pamięci; kierowany fatalny status klienta zachowuje sesje huba. Sprawdzają to testy natywne, mieszane ADC/ADCS i ncdc z resetem/błędami/dalszą komunikacją.

**Etap dostarczony w alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) dodaje startową politykę CTM/RCM/gości, zaufane nadania gotowych sesji i ponowną autoryzację przygotowania/wysyłki/echa. Wspólne kontrole IP i portu zachowują działanie zarządzanych profili. Testy natywne, mieszane ADC/ADCS i ncdc obejmują odmowy, podszywanie i dalszą komunikację.

**Etap dostarczony w alpha-0.6.7:** [Transfer Tokens](TRANSFER_TOKENS.md) ujednolica ograniczoną walidację tokenów i własne wartości w Core, CTM/RCM i błędach klientów. Unicode, skrajne spacje i ukośniki pozostają dokładne przy routingu i granicznych handshake ncdc. Niezależne żądania mogą powtarzać token; nie dodano rejestru ani domniemanej korelacji.

**Etap dostarczony w alpha-0.6.6:** [Client Capabilities](CLIENT_CAPABILITIES.md) ujednolica własne migawki trybów TCP IPv4/IPv6 i deklaracji protokołów. Wspólny wybór jawnego CTM/RCM zachowuje polecenie klienta; osobny pomocnik inicjatora wybiera CTM lub RCM i odmawia dla dwóch pasywnych klientów. Sprawdzono zmiany INF, ponowną kontrolę starych planów i zmianę trybu rzeczywistego ncdc.

**Etap dostarczony w alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) dodaje do Transfer Core własne wartości protokołu i jawne rozróżnienie zwykłego ADC/1.0 oraz wymagającego TLS ADCS/1.0. Czysty wybór preferencji/możliwości nie obniża wymaganego TLS. CTM/RCM zachowują wybraną nazwę i zgodność rozszerzeń, a odpowiedzi błędów klientów zachowują tokeny. Rzeczywisty ncdc sprawdza TLS klientów z weryfikacją certyfikatu przez oba transporty huba; zachowano regresję pełnego terminala.

**Etap dostarczony w alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) łączy przyjmowanie i dostarczanie D/E CTM oraz RCM we wspólnym routerze SID, jednym typie niezmiennego pakietu i jednej ścieżce ograniczonych kolejek. Odbiorcami są ustalone okresy życia gotowych sesji: D tylko cel, E cel i niezależne echo źródła. Token nie wybiera odbiorców; zachowano role, walidację, błędy i prywatność. Testy równoległych par używają tego samego tokenu bez mieszania odbiorców.

**Etap dostarczony w alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Etap dostarczony w alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) dodaje niezależny silnik C++20 koordynacji połączeń: ograniczone własne żądania, role aktywne/pasywne IPv4/IPv6 i niezmienne plany jednej próby związane z sesjami, anulowaniem i ponownymi kontrolami. Wspólna klasyfikacja adresów Users zachowuje Search. Routing CTM/RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

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

**Aktualna integracja — alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

| Etap | Zaimplementowany zakres |
| --- | --- |
| alpha-0.7.3 Account Model | Niezależne zweryfikowane ID/nazwy kont, role/stan, daty UTC i kandydaci zmian |
| alpha-0.7.4 Account Repository | Zaimplementowano: generowane ID, odczyt/zapis metadanych, dokładne unikalne nazwy, rewizje i jawny schemat v3 |
| alpha-0.7.5 Registration | Zaimplementowano: zaufana rejestracja C++, minimalne uprawnienia, scrypt z solą i atomowy zapis schematu v4 |
| alpha-0.1.1 Core | Cykl życia, start/stop, sygnały i logger |
| alpha-0.1.2 Network | TCP IPv4/IPv6, wiele portów i klientów, limity zasobów |
| alpha-0.1.3 ADC | Ograniczony parser strumienia, komendy i walidacja danych |
| alpha-0.1.4 ADCS | Certyfikaty, nieblokujący TLS, testy, dokumentacja i komunikaty US/PL |
| alpha-0.1.5 Configuration | INI, nadpisania CLI, walidacja ścieżek/limitów i logowanie append |
| alpha-0.1.6 Logging | Podsumowania startu, WARN, zdarzenia połączeń, ograniczone UTF-8 i budżety |
| alpha-0.1.7 Security | Ograniczone przyjęcia IP, ramki i terminy bezwzględne |
| alpha-0.1.8 Database | Opcjonalne połączenie lokalne MariaDB, prepared statements i jawny rejestr migracji |
| alpha-0.1.9 API | Lokalne uwierzytelnione HTTP/JSON: stan i health, tylko odczyt |
| alpha-0.1.10 WebAdmin | Układ React/TypeScript/Vite tylko do odczytu, EN/PL, lokalny adapter i statyczny przykład Apache |
| alpha-0.1.11 Apache2 | Przykład statycznego HTTPS, przekierowanie, TLS/nagłówki i testy tymczasowego Apache |
| alpha-0.2.1 Project | Audyt alpha-0.1, poprawki spójności, wymagane integracje w CI i podstawa alpha-0.2 |
| alpha-0.2.2 ADC Parser | Składnia argumentów BASE, pola nazwane, liczby i odzyskiwanie TCP/TLS |
| alpha-0.2.3 ADC State | Niezależny automat huba, jawne zatwierdzanie i kontrola komend/kierunków/SID |
| alpha-0.2.4 SUP | Funkcje AD/RM, BASE/wspólny TIGR, limity i połączenie z automatem |
| alpha-0.2.5 SID | Koder 20-bitowy, ograniczona wspólna pula, rezerwacje RAII i zarządzane sesje SID |
| alpha-0.2.6 INF | Ograniczone metadane, Tiger tożsamości, kontrola IP i publiczne snapshoty bez PD |
| alpha-0.2.7 CID/PID | Dedykowany weryfikator Tiger, wynik publiczny, czyszczenie buforów i testy awarii |
| alpha-0.2.8 Login | NORMAL gości, ograniczone uporządkowane wyjście, rejestr nick/CID, publiczna obecność i reconnect ncdc |
| alpha-0.2.9 STA/QUI | Semantyka błędów, routing STA, końcowe własne QUI i ograniczone opróżnianie |
| alpha-0.2.10 Security | Przyjęcia oczekujące, łączne budżety negocjacji i limity przed weryfikacją |
| alpha-0.3.1 Session Core | Model Users, identyfikator czasu życia, kopie i rzeczywisty cykl połączenia |
| alpha-0.3.2 Session Manager | Centralna własność, ograniczony rejestr, stabilny odczyt, liczenie faz i strony kopii |
| alpha-0.3.3 User Core | Publiczna tożsamość/profil gościa, własność w sesji, projekcja i aktualizacje INF (integracja po Session Manager) |
| alpha-0.3.4 Session Manager | Centralne indeksy tożsamości publicznej i zarządzane zmiany cyklu życia/profilu |
| alpha-0.3.5 User Core | Sprawdzone nazwy/tekst profilu, wartości zmiany nicku i zwalidowana projekcja ADC |
| alpha-0.3.6 Nick Manager | Ograniczony rejestr nicków, walidacja, transakcyjna zmiana i zwalnianie przy usunięciu sesji |
| alpha-0.3.7 User Registry | Ograniczone indeksy SID/CID, User tylko do odczytu, transakcyjne przyjęcie i usuwanie |
| alpha-0.3.8 User Login | Rejestracja zweryfikowanego gościa, typowane błędy i gotowość po końcowym powitaniu |
| alpha-0.3.9 User List | Deterministyczna synchronizacja początkowa, ograniczone powitanie i aktualizacje obecności FIFO |
| alpha-0.3.10 INF Updates | Transakcyjne metadane/profil/nick i zweryfikowane rozsyłanie przyrostowe |
| alpha-0.3.11 User Disconnect | Finalizacja, zwalnianie zasobów i ograniczone publiczne QUI po sprzątaniu |
| alpha-0.3.12 Limits | Konfigurowalne limity połączeń/sesji/użytkowników i bezpieczne przyjęcie/zwalnianie |
| alpha-0.3.13 Statistics | Liczniki cyklu życia, online/szczyt, połączenia, API i WebAdmin |
| alpha-0.4.1 ADC MSG | Kodek MSG i walidacja stanów; rozsyłanie czatu pozostaje planem |
| alpha-0.4.2 Chat Core | Publiczne BMSG i /me; polityka prywatna/grupowa i moderacja pozostają planem |
| alpha-0.4.3 Public Chat | Osobna polityka publicznego BMSG i wspólna ramka; czat prywatny/grupowy pozostaje planem |
| alpha-0.4.4 Private Chat | Prywatne DMSG/EMSG; grupy, historia i moderacja pozostają planem |
| alpha-0.4.5 Message Routing | Wspólny router MSG, domeny silnika i bezpieczne kierowanie do sesji/kolejek |
| alpha-0.4.6 Message Flags | Wspólna typowana walidacja PM/ME i kanoniczna projekcja publiczna/prywatna |
| alpha-0.4.7 Hub Messages | Przygotowanie/rozsyłanie IMSG, ograniczone współdzielone wyjście i powitanie |
| alpha-0.4.8 Chat Validation | Wspólne kontrole tekstu/SID/nadawcy, stan odbiorcy i ponowna kontrola dispatch |
| alpha-0.4.9 Anti-Flood | Włączone limity liczby/bajtów B/D/E, odwracalne odmowy i polityka INI |
| alpha-0.4.10 Chat Configuration | Zgrupowane INI, polityka klienta i własne powitanie bez zmiany gotowości |
| alpha-0.4.11 Chat Logging | Ograniczone logi wyników bez treści i liczniki zbiorcze API/WebAdmin |
| alpha-0.4.12 WebAdmin | Osobny widok wszystkich zbiorczych statystyk czatu tylko do odczytu |
| alpha-0.5.1 Search Core | Sprawdzany model zapytania tekstowego i biblioteka dispatch gotowych sesji; sieciowe SCH/RES są planowane |
| alpha-0.5.2 ADC SCH | Typowane parsowanie pól i odbiór ADC/ADCS; routing/wyniki są planowane |
| alpha-0.5.3 Search Filters | Filtry nazw/ścieżek Unicode, rozszerzeń, rozmiarów i typów; routing/wyniki pozostają planowane |
| alpha-0.5.4 TTH Search | Dokładne filtry hasha pliku, własne dane SCH/Core/opisu i zapytania z samym hashem; routing/wyniki pozostają planowane |
| alpha-0.5.5 Search Tokens | Ograniczony rejestr życia sesji/TO, terminy i aktywne plany Core związane z generacją; korelacja/routing rzeczywistych wyników pozostają planowane |
| alpha-0.5.6 Search Routing | Rzeczywista dostawa B/F/D/E SCH przez ADC/ADCS, współdzielone ograniczone kolejki i aktywne TO; RES pozostaje planowane |

## Historyczny plan odbioru przed alpha-0.2.8

alpha-0.2.2 realizuje zamówiony pełniejszy parser. Wcześniejsza propozycja serializera pozostaje planowana; numery dalszych etapów zostaną przypisane po określeniu zakresu.

To plan kryteriów odbioru, nie zaimplementowane funkcje. Ostateczny zakres należy ustalić przed rozpoczęciem każdego numerowanego etapu.

| Proponowany etap | Zakres | Kryterium odbioru |
| --- | --- | --- |
| Plan: ADC output | Serializer, escape, walidacja nagłówków i limity zakodowanych bajtów | Dokładne bajty zgodne z gramatyką; parse/serialize, Unicode, granice i błędy; jeszcze bez zapisu do sieci |
| Plan: Network output | Ograniczone kolejki klienta i łączne, częściowy zapis TCP/TLS, WANT_READ/WANT_WRITE i polityka wolnego odbiorcy | Fragmentowany zapis TCP/ADCS, limity kolejek, sprawiedliwa obsługa przy zatrzymanym odbiorcy, błędy klienta i ograniczony stop; bez nieograniczonych alokacji |
| Plan: Sessions | Integracja stanów/SUP/SID/INF z transportem, rejestr użytkowników, unikalność nicku/CID i dopuszczenie | Dozwolone/niedozwolone przejścia, unikalność/zwalnianie SID, odrzucanie podszytego źródła i duplikatów; bez wycieku PID |
| Plan: Client integration | Pełne logowanie gościa przez ADC i ADCS, rozłączenia i znaczenie gotowości | Prawdziwy ncdc osiąga normalny stan zalogowania na obu transportach; reconnect, wielu użytkowników, błędna negocjacja i dotychczasowe regresje przechodzą |

Pełny chat, wyszukiwanie i koordynacja transferów pozostają osobnymi etapami po poprawności sesji. Konta wymagają przeglądu uwierzytelniania/uprawnień/haseł oraz migracji aplikacji. Produkcyjna administracja WWW wymaga uwierzytelnionej bramy Apache, sesji/ról przeglądarki, CSRF i audytu przed udostępnieniem. Izolacja/rotacja logów, close_notify/reload TLS, pomiary obciążenia/fuzzing/fault injection i pozostałe systemy to jawne dalsze zadania.

## Gałęzie i ukończenie

alpha-0.2.2 powstaje z main w `ca464e02e714c1e81ed4c37279496168a6c47e23`, po scaleniu obu gałęzi audytu. Nie zmienia samoczynnie main ani znacznika alpha-0.2.

alpha-0.2.1 powstaje z main po scaleniu alpha-0.1.11. alpha-0.2 wskazuje ten sam commit audytu jako znacznik linii; nie zastępuje wymaganych numerowanych gałęzi z main. Scalanie sprawdzonego etapu do main wymaga osobnego polecenia. Zachować historię gałęzi i niezmienne migracje SQL.

Każdy etap wymaga CTest z obowiązkowymi integracjami, testów frontendu gdy zmieniany, kontroli metadanych/dokumentacji i changeloga. Zielony ncdc z początkowym HSUP potwierdza transport, dopóki pełne logowanie nie zostanie zaimplementowane. Zobacz [audyt i priorytety](AUDIT_ALPHA_0.1.md).

alpha-0.2.3 powstaje z main w `4c98e10` po scaleniu alpha-0.2.2. [Moduł stanów](ADC_STATE.md) realizuje samodzielną kontrolę kolejności; wyjście transportu, walidacja funkcji/tożsamości/haseł, unikalność SID i pełne logowanie ncdc pozostają planowane. Etap nie scala się samoczynnie do main.

alpha-0.2.4 powstaje z main w `1dd0b9f1a94626789d1ac9addad991a6eae78910`, po jawnym scaleniu alpha-0.2.3. [Moduł SUP](ADC_SUP.md) implementuje wybór funkcji/hash i łączy go z zatwierdzeniem stanu. Ogólne wyjście, podłączenie transportu, obliczanie hash i pełne logowanie pozostają planowane. Gałąź nie scala się samoczynnie do main.

alpha-0.2.5 powstaje z main w `350a43f69b5134bd049923739cd606f798f846ec` po scaleniu SUP. [SID](ADC_SID.md) realizuje własność alokatora/rezerwacji i połączenie modułów; wyjście/podłączenie sesji i tożsamość pozostają planowane. Gałąź nie scala się samoczynnie do main.

alpha-0.2.6 powstaje z main w `85524067756ac5fcbd177bb47f5d2cfe33fad063`. [INF](ADC_INF.md) realizuje walidację pól/PID-CID i połączenie stosu. Globalny rejestr/dopuszczenie, hasła kont, wyjście i pełne logowanie pozostają planowane. Gałąź nie scala się automatycznie.

alpha-0.2.7 powstaje z main w `9d3443a4d889e745b6490a139435a8f44297cfba`. [CID/PID](ADC_IDENTITY.md) wydziela i wzmacnia sprawdzanie rozpoczęte w INF; nie dodaje logowania, globalnej unikalności CID, uwierzytelniania kont ani drzew plików. Gałąź nie scala się automatycznie.


## alpha-0.2.8 Login

Dostarczono: uporządkowane wyjście/serializację, sesje gości i rejestr, rzeczywiste TCP/TLS NORMAL oraz reconnect ncdc. Pozostają: konta/hasła/uprawnienia, obsługa czatu/wyszukiwania/transferów, TLS close_notify/reload, produkcyjne testy obciążenia/fuzz/fault injection i pozostałych OS. Podstawa: main b8978491fa8e146527eba818f9bc1ab0a31aa90d. Gałąź numerowana nie scala się automatycznie.
