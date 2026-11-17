# Bezpieczeństwo — alpha-0.1.7

**alpha-0.7.15 Configuration:** [startowe ustawienia bazy i logowania](DATABASE_AUTH_CONFIGURATION.md) dodają wspólne dziedziczenie timeoutów, zakresy także przy wyłączonym logowaniu, bezpieczny podgląd --show-configuration i stronę Konfiguracja EN/PL tylko do odczytu. Migawki nie zawierają poświadczeń ani prywatnych ścieżek; zmiany wymagają sprawdzenia i restartu. CI sprawdza cztery logiczne CPU na runner i używa nproc bez stałego limitu workerów. Nie czekamy na zdalne CI.

**alpha-0.7.14 WebAdmin Accounts:** [lokalne zarządzanie kontami](WEBADMIN_ACCOUNTS.md) dodaje stronicowanie metadanych, tworzenie wyłączonych kont, aktywację, wyłączenie/reset i zmianę hasła przez osobny program bez listenerów huba. Przed zapisem zatrzymaj hub; operacje chronią hasło administratora i rewizje. Schemat v6 i migawki uwierzytelniania pozostają. CI używa czterech workerów na polecenie; nie czekamy na jego zakończenie.

**alpha-0.7.13 Database Resilience:** [jawne odnowienie połączenia i niezależne limity czasu](DATABASE_RESILIENCE.md) dodają pojedynczą próbę reconnect w wątku właściciela, ponowną walidację pliku hasła oraz ochronę przed powtarzaniem SQL i zastąpieniem aktywnej transakcji. Schemat v6 i migawki uwierzytelniania pozostają. CI korzysta z runnerów GitHub-hosted Ubuntu 24.04; nie czekamy na jego zakończenie. Ten wpis zastępuje wcześniejsze opisy braku reconnect.

**alpha-0.7.12 Anti-Bruteforce:** [wspólne limity prób logowania](ANTI_BRUTEFORCE.md) dodają ograniczone pamięciowo budżety globalne/IP/konta, automatyczne odnawianie i ogólną odmowę dla ADC/ADCS oraz reconnect. Sukces nie zwraca prób; każda rola podlega limitom. Stan pozostaje w pamięci i nie zmienia kont w bazie.

**alpha-0.7.11 Account Management:** [zaufana zmiana hasła, wyłączenie i reset](ACCOUNT_MANAGEMENT.md) łączą atomowy zapis konta/haseł ze świeżym potwierdzeniem, rewizją i usunięciem danych ADC. Wykonuj przy zatrzymanym hubie, następnie jawnie zapisz ADC/aktywuj konto według potrzeby i uruchom hub ponownie. Bez przeładowania na żywo, nowej migracji SQL i publicznego API zarządzania.

**alpha-0.7.10 Roles:** [sprawdzone role kont](ROLES.md) są teraz przypisane do gotowych sesji, nadają uprawnienia Search/Transfers i pozwalają personelowi unieważniać sesje niższych ról. Limity globalne i jawne blokady połączeń nadal obowiązują. Zmiany roli w bazie wymagają restartu; bez nowej migracji SQL i publicznego API administracji.

**alpha-0.7.9 Auth Sessions:** [sesje kont związane z połączeniem](AUTH_SESSIONS.md) dodają atomowe powiązanie przy gotowości, zaufane odczyty według konta i unieważnianie połączenia. Zamykanie natychmiast usuwa uwierzytelnienie. Bez nowej migracji SQL, automatycznych ról i sesji przeglądarkowych. CI korzysta z [runnerów GitHub](GITHUB_CI.md) bez czekania na wynik.

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) wdraża logowanie kont GPA/PAS, osobne hasło ADC szyfrowane AES-256-GCM i prywatny klucz poza bazą, zachowując scrypt. Wymagane są jawna migracja v6, aktywne konto i dokładna para nick/CID; domyślnie obowiązuje ADCS. Zmiany i blokady w katalogu startowym wymagają restartu. Publiczne przygotowanie kont, reset, rotacja i nadawanie ról pozostają kolejnymi etapami. Ten wpis zastępuje wcześniejsze opisy planowanego uwierzytelniania.

**alpha-0.7.7:** [Password Security](PASSWORD_SECURITY.md) opisuje ścisły format v1, stały koszt scrypt, porównanie kluczy w stałym czasie, wymazanie buforów i zaufany serwis SELECT. Nie deklarujemy stałego czasu całej operacji; autoryzacja, limity równoczesnych KDF i publiczne dopuszczanie sesji pozostają osobne.

**Aktualny etap — alpha-0.7.5:** [Rejestracja](REGISTRATION.md) atomowo tworzy konta registered/disabled i osobno przechowywane weryfikatory haseł OpenSSL scrypt z losową solą. Migracja v4 dodaje poświadczenia; walidacja Unicode, stały koszt KDF i rollback zapobiegają częściowym kontom. Uwierzytelnianie sesji i publiczna rejestracja API/WebAdmin pozostają planowane. CI zachowuje przypięty workflow Proxmox Ubuntu 24.04/22.04 bez scalania github-runner-v0.1.

**Aktualny etap — alpha-0.7.2:** [Migracje bazy danych](DATABASE_MIGRATIONS.md) dodają uporządkowany kompilowany katalog SQL, dokładne sumy, odczyt `--database-status`, zachowanie v1 przy aktualizacji i trwały stan applying/applied przyszłych zmian po inicjalizacji. Migracje pozostają jawne; bez schematu aplikacji i automatycznego odtwarzania DDL.

**Etap dostarczony w alpha-0.7.1:** [Database Core](DATABASE_CORE.md) dodaje typowane parametry, ograniczone wyniki na własność, metadane zapisu i transakcje RAII właściciela wątku, z redakcją błędów, blokadą zatwierdzania po błędzie i zamykaniem połączenia bez odtwarzania zapisów. Zachowano lokalne poświadczenia/migracje i CI wyłącznie na Ubuntu.

**Etap dostarczony w alpha-0.6.13:** [Konfiguracja transferów](TRANSFER_CONFIGURATION.md) dodaje dokładną ograniczoną politykę startową `allowed_protocols`, wspólne egzekwowanie CTM/RCM w Core oraz efektywne limity/protokoły tylko do odczytu w zabezpieczonym API i dwujęzycznej stronie Połączenia. Domyślne wartości zachowują routing; zmiany wymagają restartu.

**Aktualny etap — alpha-0.6.11:** [Transfer Statistics](TRANSFER_STATISTICS.md) dodaje stałe zbiorcze liczniki wyników CTM/RCM, kolejek/błędów i raportów klientów, rozszerzenie zabezpieczonego API oraz dwujęzyczny monitoring Połączeń. [Debian 13 CI](CI_DEBIAN13.md) dodaje sekwencyjną macierz Debug/Release własnego runnera i środowisko akcji Node 24, zachowując Node 22 dla WebAdmina.

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

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Etap dodaje lokalne limity zasobów do TCP/ADCS i parsera składni ADC. Nadal nie ma uwierzytelniania, tożsamości sesji, ról ani pełnego logowania ADC. Uruchamiaj jako zwykły użytkownik; domyślny nasłuch pozostaje lokalny, adres publiczny/wildcard wymaga jawnej konfiguracji.

## Zaimplementowane limity

| Ustawienie | Domyślnie | Zakres |
| --- | --- | --- |
| Aktywne połączenia na kanoniczny źródłowy IP | 128 | 1..65535 |
| Przyjęcia IP w stałym oknie minuty | 256 | 1..100000 |
| Wyniki dekodera na połączenie w stałym oknie sekundy | 1024 | 1..100000 |
| Błędne ramki w czasie życia połączenia | 512 | 1..65535 |
| Bezwzględny termin negocjacji TLS | 10 sekund | 1..3600 |
| Bezwzględny termin niedokończonej ramki ADC | 10 sekund | 1..3600 |

Nadal obowiązują globalny limit klientów (1024), bezczynność (120 sekund), 16384 bajty treści ramki i 128 parametrów. Są to zasady dc24h, nie limity [ADC 1.0.4](https://dc-protocols.github.io/ADC.html). Pojedyncze błędne ramki są ignorowane z możliwością resynchronizacji; kolejne mogą wyczerpać limit błędów i zamknąć klienta.

Sześć opcji jest dostępnych w `[security]` i CLI; zobacz [CONFIGURATION.md](CONFIGURATION.md). Zero nie wyłącza ochrony. `--check-config` sprawdza zakresy bez portów/logu. Konstruktor Network sprawdza też ustawienia przekazane przez kod C++.

## Przyjmowanie i pamięć

Po accept reaktor sprawdza limit globalny, a potem wspólny limiter IP **przed utworzeniem sesji TLS i dodaniem watch klienta**. Obowiązuje na wszystkich portach TCP/TLS. Nieukończone TLS zajmuje slot. Zamknięcie/błąd/timeout/stop go zwalnia; nieudane tworzenie sesji lub watch także zwalnia licznik aktywnych. Przyznane przyjęcie nadal liczy się w minucie, nawet jeśli późniejsza inicjalizacja zawiedzie. Odrzucenia nie odnawiają okna ani nie liczą się jako przyjęcia. Kernel może zakończyć handshake TCP przed odrzuceniem przez aplikację.

IP pochodzi z sockaddr systemu, nie z pól ADC klienta. IPv4-mapped IPv6 normalizuje się do IPv4; nasłuch IPv6 nadal ma IPV6_V6ONLY. Różne tekstowe zapisy tego samego adresu nie tworzą nowych kluczy. Pamięć przechowuje najwyżej 65536 wpisów, klucz do 48 bajtów; nowy adres przy pełnej mapie jest odrzucany. Przy przyjmowaniu, najwyżej raz na pięć sekund, usuwane są nieaktywne wpisy z wygasłym oknem minuty. Aktywne pozostają. Nie ma trwałych banów ani tożsamości kont.

Okna zaczynają się od pierwszego przyjęcia/wyniku i odnawiają na żądanie. Są stałe, nie kroczące ani token bucket; na granicy możliwy jest większy chwilowy ruch. Klienci za NAT/proxy współdzielą budżet IP; limity trzeba dostosować. Nagłówki proxy i zaufana tożsamość reverse proxy nie są obsługiwane.

## Ramki i terminy

Każdy wynik dekodera zużywa budżet: poprawne komendy, nieznane rozszerzenia, keepalive i błędy. Zbyt długa ramka generuje jeden błąd, a dane są odrzucane do LF bez wzrostu bufora 16 KiB. Przy max_invalid_frames=N N-ty błąd zamyka połączenie; poprawne ramki nie zerują licznika. Wynik ponad tempo nie trafia do zwykłych liczników ADC. Reaktor podaje dekoderowi najwyżej jeden segment ramki na wywołanie i przestaje przetwarzać resztę odczytu po przekroczeniu limitu. Wynik przekraczający limit mógł już być sparsowany, ale dalsze ramki nie są parsowane. Brak kolejki ramek.

TLS ma termin od przyjęcia; dosyłane bajty go nie odnawiają. Efektywny czas ograniczają termin TLS i bezczynność. Niedokończona ramka ma termin od pierwszych zaobserwowanych bajtów, także w stanie odrzucania za długość. Dalsze bajty nie przedłużają go. Ukończenie zeruje timer; następna częściowa ramka z tego samego odczytu dostaje własny start. Kontrola następuje przed/po I/O i przed segmentami, więc spóźnione dane nie odnawiają wygasłego terminu. Osobny timer bezczynności mierzy aktywność aplikacji. Przegląd wygasania działa też przy pętli oczekującej do 250 ms; CPU/dysk/planista mogą dodać opóźnienie.

Fragmentacja między odczytami i rekordami TLS jest dozwolona. Pozostają walidacja UTF-8/NFC, nagłówków/escape, odrzucanie NUL/control i limit parametrów. Przy końcowym EOF można doliczyć ucięcie raz, gdy klient już jest usuwany, bez kolejnego żywego slotu tempa ramek.

## Obserwowalność i prywatność

Atomowy `security_rejected` zlicza odrzucenia przez IP/pojemność mapy, zawarte też w Network rejected. `security_closed` obejmuje limit ramek/błędów i terminy TLS/częściowej ramki, także bezczynność podczas nieukończonego TLS. Zwykła bezczynność, I/O klienta, EOF i stop nie zwiększają go. Końcowy INFO ma postać `Security stopped; rejected=N closed=N`. Pola atomowe są odczytywane niezależnie.

Ostrzeżenia limitów przyjęć, ramek i częściowej ramki są ograniczone częstotliwością, bez źródłowych IP i danych. Opcjonalne zdarzenia INFO dodają `security_quota`, `partial_frame_timeout` i używają istniejącego `handshake_timeout`. Liczniki i WARN mogą być widoczne przy wyłączonych szczegółowych zdarzeniach. Nie loguje się tekstu ADC, nicków, CID/PID, haseł/hashów ani IP klientów. Gotowość portu nadal pokazuje lokalny adres bind. Zobacz [LOGGING.md](LOGGING.md).

## Pozostałe kontrole i ograniczenia

TLS >=1.2 chroni tylko ADCS; wczytywalna zgodna para PEM jest sprawdzana przed gotowością. Zaufanie/nazwę/ważność sprawdza klient. Brak certyfikatów klientów i uwierzytelniania kont. Klucze pozostają poza Git z ograniczonymi prawami. INI/logi zachowują kontrole typu/rozmiaru deskryptora i kolizji ścieżek oraz ograniczone UTF-8; wymagane są zaufane katalogi. Stop jest synchroniczny, main ignoruje SIGPIPE, deskryptory mają close-on-exec. Szczegóły: [TLS](ADCS_TLS.md), [konfiguracja](CONFIGURATION.md).

Nie jest to ochrona DDoS, limit bajtów/przepustowości, rejestr audytowy ani audyt bezpieczeństwa. Synchroniczny dysk/stderr może blokować reaktor. Krytyczny błąd pętli/alokacji może zakończyć proces. Brak limitu podsieci/globalnych prób, współdzielenia ochrony między procesami, wyjścia/backpressure, semantyki sesji, kont/ról/banów, haseł użytkowników, zapisu danych aplikacji do SQL i autoryzacji ról dla zmian administracyjnych HTTP. Kolejne moduły muszą wdrożyć je jawnie. Zakres testów i platform podaje [TESTING.md](TESTING.md).

## Hasło bazy — alpha-0.1.8

Używaj dedykowanego lokalnego konta SQL z minimalnymi uprawnieniami. Hasło pochodzi tylko z prywatnego zwykłego pliku właściciela, bez wartości w CLI/INI. Parametry SQL są wiązane; LOCAL INFILE i reconnect są wyłączone. Log nie może wskazywać pliku hasła. Błędy nie zawierają tekstu serwera ani sekretów. Migracje są jawne, chronione współpracującą blokadą i zapisem SHA-256; nie zastępują kopii bazy ani audytu struktury tabel. Zobacz [DATABASE.md](DATABASE.md).

## Lokalne API administracyjne — alpha-0.1.9

Każde żądanie tylko do odczytu wymaga tego samego efektywnego UID, prywatnego katalogu 0700/gniazda 0600 i prywatnego losowego tokena Bearer 64-hex. Porównanie tej samej długości jest stałoczasowe. Brak TCP, zmian stanu, CORS i anonimowego health. Ścisły podzbiór HTTP bez body odrzuca duplikaty/niejednoznaczne ramkowanie; klienci, nagłówki, bufory i bezwzględny czas żądania/odpowiedzi są ograniczone. Dane żądań/sekrety nie trafiają do logów ani odpowiedzi. Sprzątanie sprawdza inode i nie usuwa istniejącego endpointu przy starcie. Katalogi i procesy UID usługi są zaufane; przejęcie root/tego samego UID, limity tempa API, sesje WWW, role, brama i audyt żądań pozostają poza etapem. Zobacz [API.md](API.md).

## Podstawa WebAdmin — alpha-0.1.10

Nie umieszczaj tokena w pamięci przeglądarki, JavaScript, URL, zmiennych VITE ani HTML. Adapter deweloperski sprawdza prywatny plik właściciela poza serwowanym katalogiem, blokuje obcy origin/host/site/metody/ścieżki/body, dodaje własny sekret i normalizuje ograniczone odpowiedzi. To lokalne narzędzie, nie produkcyjne logowanie WWW. Przykład Apache2 serwuje statyczne zasoby lokalnie i blokuje API. Etap nie dodaje operacji zmieniających stan, zdalnego API, sesji WWW ani ról. Zobacz [WEBADMIN.md](WEBADMIN.md).

## Przykład HTTPS Apache2 — alpha-0.1.11

TLS i logowanie WWW są osobnymi mechanizmami. Szablon wybiera TLS 1.2/1.3 z AEAD, ustala host przekierowania, zachowuje Require local, blokuje prywatne/ukryte/sekretne ścieżki i wyłącza indeksy/symlinki. HSTS jest krótki, tylko dla hosta i HTTPS; CSP/no-sniff/referrer/frame obejmują błędy. Przed użyciem zweryfikuj zaufanie/nazwę/zgodność klucza; sama składnia nie dowodzi zgodności. Szablon nie ujawnia tokena i nie aktywuje bramy produkcyjnej. Zobacz [APACHE2_HTTPS.md](APACHE2_HTTPS.md).

## Granica ADC State — alpha-0.2.3

Niezależny moduł stanów odrzuca przedwczesne komendy, powtórzone kroki logowania, przychodzące typy zarezerwowane dla huba i podszyty źródłowy SID. Jawne zatwierdzenie wymaga oczekującego żądania; samo parsowanie INF/PAS nie przyznaje NORMAL. Moduł nie zachowuje PID, PAS, nicku ani parametrów i nie loguje. To kontrola kolejności, nie uwierzytelnianie ani wdrożona granica bezpieczeństwa: odbiorczy Network go nie wywołuje. Przyszły dispatcher musi sprawdzić zgodność funkcji/hash, PID/CID, wymagane pola, IP, unikalność nicku/CID i hasło przed zatwierdzeniem, usunąć PD przed przekazaniem, zastosować limity/terminy i zamknąć po odrzuceniu. Zobacz [ADC_STATE.md](ADC_STATE.md).

## Polityka SUP — alpha-0.2.4

Połączony moduł SUP/stanów sprawdza początkowy BASE/wspólny TIGR, rozdziela funkcje lokalne i klienta, ogranicza tokeny/pamięć i wycofuje błędne aktualizacje. Wybranego hash nie można usunąć ani zmienić. Nieznane funkcje klienta są biernymi metadanymi i nie są odbijane do odpowiedzi huba. Dodatkowe funkcje lokalne wymagają implementacji wywołującego; domyślnie reklamowane są tylko BASE/TIGR. Wybór nie oznacza obliczania Tiger, walidacji PID/CID, hasła ani konfiguracji TLS. Transport produkcyjny jeszcze nie wywołuje modułu. Błędy/logi nie zawierają tekstu klienta ani sekretów. Zobacz [ADC_SUP.md](ADC_SUP.md).

## Własność SID — alpha-0.2.5

Wspólna pula gwarantuje unikalność aktywnych rezerwacji jednego huba, także podczas oczekiwania na wysłanie ISID. Prywatny przenoszalny uchwyt zapobiega zwalnianiu przez skopiowany SID, a reset przeniesionego/starego uchwytu nie zwalnia nowego właściciela. Koder obejmuje pełne 20 bitów; lokalna polityka wyklucza zero/AAAA z przydziału klientom. SID jest deterministycznym publicznym adresem routingu, nie sekretem, uprawnieniem ani cookie. Nadal wymagane są kontrole wykonawcy/źródła. Wyczerpanie daje stały błąd bez częściowej rezerwacji. Zmiany puli/sesji i zniszczenie ostatniego współwłaściciela należą do jednego wątku. Anulować stare wyjście i referencje rejestru przed zwolnieniem/ponownym użyciem. Moduł nie jest jeszcze granicą bezpieczeństwa działającego transportu. Zobacz [ADC_SID.md](ADC_SID.md).

## Walidacja INF — alpha-0.2.6

Początkowe NI/ID/PD, kanoniczne kodowanie hash, Tiger(PID)==CID, limity liczb/tekstu/funkcji i zgodność IP są sprawdzane przed zatwierdzeniem. PD jest pożyczony z wejścia, nigdy nie trafia do ClientInfo i jest usunięty z publicznych snapshotów; odkodowane tymczasowe bajty PID są czyszczone. Oryginalnych ramek właściciela API nie usuwa — nie wolno ich logować/przekazywać. Roszczenia uprawnień poza CT 0/1 są odrzucane; CT1 jest informacyjne, nie autoryzuje. Nieznane pola są ignorowane; duplikaty odrzucane także dla nieznanych nazw. Brak algorytmu Libgcrypt odrzuca walidację bez zastępczego hash i zmian globalnej polityki kryptografii. Globalna unikalność nicku/CID, konta, role i podłączenie transportu pozostają poza modułem. Zobacz [ADC_INF.md](ADC_INF.md).

## Dedykowany weryfikator CID/PID — alpha-0.2.7

Wspólny weryfikator pożycza tekst, dekoduje PID bezpośrednio do niekopiowalnego bufora, oblicza hash w drugim czyszczonym buforze i porównuje dokładnie 24 bajty przez CRYPTO_memcmp. Każdy zwykły powrót czyści oba własne bufory; nie obiecujemy wymazania wejścia, rejestrów i dowolnych kopii kompilatora/bibliotek. Stałoczasowość dotyczy końcowego porównania, nie składni i całego handshake. Sukces zawiera tylko CID, nie poświadczenie konta, PID, rolę ani dowód globalnej unikalności. Awarie algorytmu/kontekstu/odczytu są testowane lokalnymi wrapperami linkera; produkcja nie ma wstrzykiwanego dostawcy ani zmian FIPS/globalnych trybów. Zobacz [ADC_IDENTITY.md](ADC_IDENTITY.md).
