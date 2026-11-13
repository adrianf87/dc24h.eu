# ADC Parser — alpha-0.2.2

**Bieżący etap — alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

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

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Od alpha-0.2.7 [weryfikacja CID/PID](ADC_IDENTITY.md) jest osobnym wspólnym modułem z TigerCid/VerifiedIdentity, bezpośrednimi czyszczonymi buforami i stałoczasowym końcowym porównaniem skrótu. Snapshot INF zachowuje wyłącznie publiczny wynik weryfikacji. Podstawowa obsługa z alpha-0.2.6 pozostaje zgodna; transport i polityka kont/rejestru są osobne.

Od alpha-0.2.6 [ADC INF](ADC_INF.md) udostępnia ClientSession, łączący niższe warstwy z walidacją INF i weryfikacją Tiger PID/CID. Wymaga poprawnych metadanych przed zewnętrzną decyzją dopuszczenia i nie zachowuje/eksportuje PD. Unikalność w rejestrze i podłączenie Network pozostają planowane; wcześniejsze API/fixture zachowują swój zakres.

Od alpha-0.2.5 [ADC SID](ADC_SID.md) udostępnia wspólną pulę huba i SidSession z kontrolą własności ISID oraz automatycznym zwalnianiem rezerwacji. Parser/automat używają tego samego kodera SID; AAAA nadal jest poprawną składnią, ale pula rezerwuje go jako lokalną politykę. Pierwotne API niższego poziomu i historyczne fixture pozostają dostępne. Network nadal używa samego parsera.

alpha-0.2.4 dodaje niezależną [semantykę SUP i adapter stanów](ADC_SUP.md). Parser działa bez zmian. Dotychczasowy test ncdc huba nadal obejmuje początkowy HSUP; nowy izolowany fixture SUP wymienia ISUP wygenerowany przez moduł i stały testowy SID z ncdc, kończąc w niezatwierdzonym IDENTIFY. To nie reaktor Network.

Odniesienie: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), gramatyka linii i opisy komend BASE. Niezależna biblioteka C++20 `dc24h_adc` parsuje składnię; nie wykonuje komend ani nie realizuje sesji.

## Wynik i układ argumentów

`parse_line(string_view)` przyjmuje jedną treść ramki **bez LF** i zwraca `KeepAlive`, `Message` albo `ParseError`. Pusty LF jest keepalive. Message jest właścicielem typu, oryginalnej komendy, klasyfikacji, opcjonalnego source/target, uporządkowanych selektorów i odkodowanych parametrów. Bufor wejścia można od razu zwolnić. Nieznane komendy zachowują nazwę i wszystkie parametry.

`Message::positional_count` jest ustawiony dla znanych komend BASE. Parametry przed tym indeksem są pozycyjne, pozostałe nazwane. SUP traktuje wszystkie parametry jako pozycyjne aktualizacje cech. Układ nieznanej komendy pozostaje `nullopt`: prefiks NI nie pozwala zgadywać, że parametr rozszerzenia jest polem.

`parse_named_parameter(decoded_token)` sprawdza dwubajtową nazwę: wielka litera oraz wielka litera/cyfra. Zwraca opcjonalny `NamedParameterView {name, value}`. Oba string_view wskazują pamięć tokena; nie mogą przeżyć jej zwolnienia ani modyfikacji string/vector. Funkcja nie dekoduje ponownie i nie waliduje wartości. Sama nazwa daje pustą wartość, co pozwala anulować pole. Nieznane nazwy, powtórzenia/powtarzane terminy wyszukiwania i ich kolejność są zachowane; parser nie wybiera pierwszej/ostatniej wartości ani nie usuwa duplikatów.

`parse_integer(decoded_token)` zwraca opcjonalną dziesiętną liczbę całkowitą ze znakiem, 64-bit. Akceptuje minus, początkowe zera i ujemne zero. Odrzuca plus, białe znaki, ułamki, wykładniki, cyfry spoza ASCII, NUL, końcowe dane i przepełnienie. Zero jest poprawnym wynikiem. Funkcja nie narzuca polityki portu/zakresu/transferu.

## Składnia argumentów BASE

| Komendy | Początkowe argumenty pozycyjne | Dodatkowe sprawdzenia |
| --- | --- | --- |
| STA | 2 | Pierwszy argument ma trzy cyfry ASCII; nieznana wartość kodu pozostaje poprawna składniowo |
| SUP | Co najmniej jeden, zmienna liczba | Każdy token ma AD/RM oraz czteroznakową nazwę cechy; bez dowolnych końcowych pól |
| SID, QUI | 1 | Pozycyjny SID ma cztery znaki base32 |
| MSG, GPA, PAS | 1 | Tekst/dane zachowane; poprawność i długość hasha nie są sprawdzane |
| INF, SCH, RES | 0 | Każdy token ma poprawny prefiks nazwany; brak wymaganych pól aplikacji |
| CTM | 3 | Port ma ograniczoną składnię integer; protokół i token pozostają tekstem |
| RCM | 2 | Protokół i token pozostają tekstem |
| GET, SND | 4 | Przestrzeń transferu to litery/cyfry ASCII; offset/liczba bajtów mają składnię integer |
| GFI | 2 | Przestrzeń transferu to litery/cyfry ASCII |

Poza SUP wszystkie tokeny po stałym prefiksie pozycyjnym muszą być nazwane. Tekst pozycyjny przypominający nazwę pola nadal pozostaje pozycyjny. Parametry są w jednym własnym wektorze, bez dodatkowych kopii. Przykład: `HMSG NItext PMABCD` ma positional_count 1; `BINF ABCD NI` ma positional_count 0 i puste pole. Przykłady opisują składnię, nie uwierzytelnione osoby ani dozwolone przejścia stanu.

Błędne znane komendy, które alpha-0.1 przyjmowała jako ogólne tokeny, są teraz odrzucane: braki wymaganych argumentów, złe nazwy pól, aktualizacje SUP, pozycyjne SID/kody/liczby/nazwy przestrzeni. Nowe kategorie błędów: missing_parameters, invalid_named_parameter, invalid_support, invalid_status_code, invalid_integer, invalid_transfer_type i too_many_features. Dotychczasowe wartości enum zachowują kolejność. Ramkowanie/UTF-8/nagłówki/escape są sprawdzane przed argumentami. Przy kilku błędach nie należy polegać na kolejności diagnostyki poza tą zasadą.

## Ramkowanie, Unicode i limity lokalne

Obsługiwane są B/C/D/E/F/H/I/U. Sprawdzane są wymagane SID routingu, CID U o zmiennej długości base32 i selektory F ze znakiem. Długość CID nie jest związana z TIGR. Tokeny rozdziela pojedyncza spacja ASCII; puste/podwójne/końcowe separatory są błędne. Dekodowane są tylko `\s`, `\n` i `\\`. utf8proc sprawdza UTF-8/NFC; tekst wymagający normalizacji jest odrzucany bez przepisywania.

Lokalna polityka, nie maksima protokołu:

- 16384 bajty treści bez LF; 128 parametrów bez routingu.
- 128 selektorów nagłówka F, sprawdzane przed alokacją wpisów. Nowy osobny limit nie zmienia budżetu parametrów SUP.
- Liczbowe argumenty pozycyjne w zakresie signed 64-bit. Zakaz surowych ASCII control/DEL, w tym NUL, tab i CR; CRLF jest odrzucane.

`StreamDecoder::feed(bytes, sink)` dostarcza wyniki synchronicznie i w kolejności, bez zachowywania wiadomości. Sink nie może ponownie wejść do tego samego dekodera; wyjątki propagują się. Instancja należy do jednego wątku. Błędna ukończona ramka daje jeden wynik i wznowienie przy kolejnym LF. Nadmiar długości daje natychmiast jeden błąd i odrzucanie do LF, z buforem do 16 KiB. `finish()` zgłasza część ramki raz, resetuje i pozwala użyć dekodera ponownie; już policzony nadmiar nie jest ponawiany. `in_progress()` uwzględnia odrzucanie dla terminów bezwzględnych.

## Integracja i granice etapu

Każde połączenie TCP/TLS automatycznie używa pełniejszego parsera. Liczniki Network zliczają zaakceptowaną składnię; nieznane komendy nadal stanowią jej podzbiór. Błędne argumenty korzystają z dotychczasowych limitów błędów/wyników, ogólnego oczyszczonego ostrzeżenia i odzyskiwania strumienia. Parametry, PID i PAS nie trafiają do logów. Security może zamknąć połączenie po przekroczeniu limitów. Zobacz [SECURITY.md](SECURITY.md) i [ADCS_TLS.md](ADCS_TLS.md).

Niezależny [moduł ADC State](ADC_STATE.md) implementuje już kontrolę komend/typów/stanów, ale Network jeszcze go nie wywołuje. Brak serializera, kolejek wyjściowych, alokatora SID, walidacji tożsamości/hash, logowania użytkowników, routingu, wykonania chatu/wyszukiwania/transferu i nasłuchu UDP. Zakresy portów, sens offset/liczby bajtów, wartości pól i wymagane tożsamości INF/RES należą do przyszłych konsumentów. Parsowanie GET/SND nie włącza DATA ani nie odbiera danych binarnych. Prawdziwy ncdc nadal sprawdza początkowy HSUP; brak odpowiedzi ISUP/ISID i pełnego logowania.

## Weryfikacja

`adc.syntax` sprawdza 15 układów BASE, brakujące argumenty, dosłowny tekst pozycyjny, puste/powtórzone/nieznane pola, granice i błędy liczb, nieznane rozszerzenia, budżety selektorów/parametrów i własność wyniku. Korpus strumieniowy porównuje wszystkie nagłówki/wartości/układy/błędy przy każdym podziale na dwa fragmenty i podawaniu bajt po bajcie, także dla Unicode i escape. Dotychczasowe deterministyczne testy błędnych bajtów, podziałów, nadmiaru i EOF pozostają w `adc.parser`.

Regresje TCP i prawdziwego TLS mieszają błędne argumenty BASE z poprawnymi ramkami i nieznanymi rozszerzeniami, sprawdzając liczniki, odzyskiwanie i brak sekretów w logach. Dotychczasowy ncdc ADC/ADCS nadal sprawdza rzeczywisty ruch klienta. Uruchomienie pełnego zestawu opisuje [TESTING.md](TESTING.md).
