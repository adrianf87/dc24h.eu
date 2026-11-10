# SUP — alpha-0.2.4

**Aktualny etap — alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Od alpha-0.2.7 [weryfikacja CID/PID](ADC_IDENTITY.md) jest osobnym wspólnym modułem z TigerCid/VerifiedIdentity, bezpośrednimi czyszczonymi buforami i stałoczasowym końcowym porównaniem skrótu. Snapshot INF zachowuje wyłącznie publiczny wynik weryfikacji. Podstawowa obsługa z alpha-0.2.6 pozostaje zgodna; transport i polityka kont/rejestru są osobne.

Od alpha-0.2.6 [ADC INF](ADC_INF.md) udostępnia ClientSession, łączący niższe warstwy z walidacją INF i weryfikacją Tiger PID/CID. Wymaga poprawnych metadanych przed zewnętrzną decyzją dopuszczenia i nie zachowuje/eksportuje PD. Unikalność w rejestrze i podłączenie Network pozostają planowane; wcześniejsze API/fixture zachowują swój zakres.

Od alpha-0.2.5 [ADC SID](ADC_SID.md) udostępnia wspólną pulę huba i SidSession z kontrolą własności ISID oraz automatycznym zwalnianiem rezerwacji. Parser/automat używają tego samego kodera SID; AAAA nadal jest poprawną składnią, ale pula rezerwuje go jako lokalną politykę. Pierwotne API niższego poziomu i historyczne fixture pozostają dostępne. Network nadal używa samego parsera.

## Zakres etapu

`dc24h/adc/sup.hpp` dodaje `SupportNegotiation` i `ProtocolConnection` do `dc24h_adc`. Pierwszy utrzymuje reklamowane funkcje i wybiera hash sesji. Drugi łączy negocjację z [ADC State](ADC_STATE.md), zastępując ręczne zatwierdzenie protokołu sprawdzoną wymianą SUP. Brak nowej zależności od gniazd, logowania, bazy, pliku konfiguracji i kryptografii.

Źródła: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), rozdziały 3.2, 3.4, 5.1, 5.3.2 i 6.1; [ADC Extensions, TIGR](https://adc.sourceforge.io/ADC-EXT.html). Lokalny rejestr obsługiwanych hash zawiera obecnie wyłącznie TIGR. Wybrany algorytm to Tiger z wynikiem 24 bajtów; moduł go wybiera/opisuje, ale nie oblicza skrótów, drzew, CID i odpowiedzi hasłowych. Wybór TIGR nie określa szyfrów TLS, skrótów certyfikatów i keyprint OpenSSL.

Domyślny profil lokalny to BASE/TIGR. Dodatkowe funkcje w `SupportPolicy::features` są deklaracją wywołującego i wymagają odpowiednich implementacji przed rzeczywistym wdrożeniem. Test profilu nie oznacza zaimplementowania tych rozszerzeń. Produkcyjny Network nadal wyłącznie parsuje wejście; nie podłącza ProtocolConnection i nie wysyła tych odpowiedzi. Pełne logowanie pozostaje planowane.

## Semantyka funkcji i limity

Pierwszy HSUP zaczyna od pustego zbioru funkcji klienta. AD dodaje funkcję, RM usuwa. Tokeny wykonują się w kolejności sieciowej; dodanie istniejącej/usunięcie nieobecnej funkcji jest idempotentne. Ostatnia operacja nazwy określa obecność. Powtórny pierwszy SUP po sukcesie jest odrzucany; aktualizacje przez ProtocolConnection należą wyłącznie do NORMAL.

Po zastosowaniu pierwszej ramki musi istnieć BASE i przynajmniej jeden wspólny lokalnie obsługiwany hash. Obecnie oznacza to TIGR. Nieznana funkcja klienta pozostaje zapisana, ale nigdy samoczynnie nie staje się kandydatem hash ani funkcją lokalną. Reklamowane funkcje huba i klienta są osobne; odpowiedź nie jest przecięciem zbiorów i nie odbija listy klienta. Funkcje SUP nie zastępują pola INF SU używanego przez routing funkcji.

Lokalne limity aplikacji, niezależne od maksimów protokołu:

- Do 128 zadeklarowanych tokenów polityki lokalnej przed usunięciem duplikatów; BASE i TIGR są obowiązkowe. Błędna nazwa/brak wymagań/nadmiar powodują `invalid_argument` ze stałym tekstem przy konstrukcji.
- Do 128 tokenów wejściowego SUP, tak jak w parserze; każdy token jest dodatkowo sprawdzany pod kątem AD/RM i czteroznakowej nazwy z wielką literą na początku i wielkimi literami/cyframi dalej, także w ręcznym Message.
- Do 128 różnych funkcji klienta w całej sesji, nie na ramkę. Własny posortowany wektor nie tworzy nieograniczonej mapy rozszerzeń. Wyszukiwanie nie alokuje.
- Limit zbioru sprawdzany jest przed każdym dodaniem. Przy pełnym zbiorze najpierw usunąć opcjonalną funkcję. Add-then-remove przekraczający limit jest odrzucany, nawet jeśli wynik końcowy by się zmieścił. Duplikaty nie zużywają miejsca.

Aktualizacje używają ograniczonej kopii kandydackiej. Błąd składni, pojemności, brak BASE lub usunięcie wybranego hash nie zmienia funkcji ani stanu połączenia. Ramka usuwająca i odtwarzająca BASE/TIGR jest akceptowana, jeśli końcowy zbiór zawiera oba i każde pośrednie dodanie mieści się w limicie. Nieznane opcjonalne funkcje mogą być dodawane/usuwane. Żadna aktualizacja nie renegocjuje hash sesji. Polityka lokalna jest stała; wychodzący SUP w NORMAL zwraca local_update_unsupported zamiast niejawnie zmieniać reklamowane funkcje.

## Odpowiedź i błędy

Przed sukcesem `selected_hash()`, `initial_reply()` i `initial_reply_line()` zwracają puste optional. Po sukcesie odpowiedź posiada własny Message (I/SUP, bez routingu), z positional_count. Kolejność: ADBASE, wybrany hash ADTIGR, następnie inne jawnie lokalne funkcje leksykograficznie. Wybrany hash poprzedza inne zarejestrowane hash; nowy algorytm wymaga rozszerzenia jawnego rejestru i testów wyboru/kolejności. Nieznanych funkcji nie rozpoznaje się jako hash na podstawie nazwy/kolejności.

Minimalna odpowiedź to `ISUP ADBASE ADTIGR` zakończona LF. `initial_reply_line()` jest ograniczonym koderem wyłącznie SUP z kontrolowanych tokenów, nie ogólnym serializerem ani zapisem sieciowym. Przy 128 funkcjach treść ma najwyżej 900 bajtów. Odpowiedź posiada własną pamięć; jej modyfikacja/zniszczenie nie zmienia negocjacji.

Stałe kategorie `SupportError`: invalid_message, feature_limit, missing_base, no_common_hash, session_hash_removed, invalid_sequence, reply_mismatch i local_update_unsupported. Nie zawierają tokenów klienta ani sekretów. `support_status_code()` proponuje przyszłemu dispatcherowi kody fatalne: 245 dla braku BASE/usunięcia hash sesji, 247 dla braku początkowego wspólnego hash, 240 dla pozostałych błędów. Nie wysyła STA, nie wybiera opisu i nie zamyka gniazda. Właściciel musi ograniczać błędy/zamykać połączenie; odrzucenie bez zmian nie oznacza nieskończonych ponowień.

`SupportNegotiation::accept_initial` i `update_peer` zwracają opcjonalny SupportError. Klasa metadanych nie zna fazy połączenia; przy użyciu bezpośrednim kolejność zapewnia wywołujący. Błąd konstruktora jest wyjątkiem, nie SupportError. Błędy alokacji propagują się bez częściowego zatwierdzenia funkcji. Referencje local_features/peer_features są pożyczone i nie mogą przetrwać modyfikacji/zniszczenia. Instancje nie współdzielą stanu.

## Połączenie ze stanami

Jedna niekopiowalna/nieprzenoszalna instancja ProtocolConnection powinna należeć do przyszłego połączenia na jego wątku reaktora. `observe(Direction, Message)` zwraca opcjonalny `variant<StateError, SupportError>`; brak błędu oznacza sukces. Udostępnia stan/SID i stałe metadane funkcji oraz keepalive, jawne zatwierdzanie tożsamości/hasła i terminalne rozłączenie. Nie daje ręcznego zatwierdzenia protokołu ani modyfikowalnego dostępu do stanu/funkcji.

1. Pierwszy przychodzący HSUP przygotowuje negocjację kandydacką. Dopiero sukces pozwala automatowi przyjąć HSUP i zatwierdzić protokół. Zatwierdzenie kandydata nie rzuca. Faza pozostaje PROTOCOL.
2. Pierwszy wychodzący ISUP musi dokładnie odpowiadać wygenerowanej reklamie/typowi/routingowi. Odpowiedź ucięta, przestawiona, odbijająca klienta lub nieobsługiwana jest odrzucana. Obserwować dopiero po ukończonym uporządkowanym wysłaniu.
3. Ukończony ISID przechodzi do IDENTIFY. Samo parsowanie BINF nie zatwierdza tożsamości. Bramki VERIFY/hasła pozostają dotychczasowe.
4. W NORMAL przychodzący HSUP jest przygotowywany/sprawdzany przed zatwierdzeniem stanu/funkcji. IDENTIFY/VERIFY go odrzucają. Kontrole źródła/typu/komendy pozostałych komunikatów pozostają aktywne. DISCONNECTED odrzuca wszystko i nie pozwala ponownie użyć instancji.

Adapter gwarantuje brak zmian stanu/funkcji po zwróconym błędzie. Potencjalna alokacja kandydata/odpowiedzi następuje przed zmianą posiadanego automatu. Brak ogólnego interfejsu kontroli wyjścia przed wysłaniem, kolejki, callbacka ukończenia, terminów, alokatora SID, obliczania Tiger, walidacji tożsamości, haseł i dispatchera. To nadal wymagania przed podłączeniem Network. Keepalive nigdy nie zatwierdza logowania. Brak zmian schematu/liczników HTTP; dotychczasowe liczniki nadal mierzą składnię.

## Testy i granica ncdc

`adc.sup` sprawdza BASE/wspólny hash, nieznane/powtórzone/uporządkowane funkcje, limity polityki/klienta, rollback, stały hash, odpowiedzi, własność danych, błędne typy/ręczne wejście, zatwierdzenie stanu, dokładny ISUP, dalszą ścieżkę gościa/hasła, źródłowy SID i stan terminalny. Testy strumienia obejmują każdy podział pipeline SUP/powtórzenia na dwa fragmenty.

`adc.sup.ncdc` uruchamia prawdziwy ncdc z tymczasowym PTY/konfiguracją i bez udziałów. Python posiada gniazda TCP/TLS loopback i przekazuje HSUP do sterownika modułu C++. Po wysłaniu ISUP modułu i stałego testowego SID zgłasza ukończenie wysyłki i przekazuje rzeczywisty BINF ncdc do adaptera. Fixture C++ sprawdza długości pól ID/PD dla Tiger i pozostaje w niezatwierdzonym IDENTIFY; nie waliduje ani nie loguje tożsamości. Fixture nie jest reaktorem serwera ani produkcyjnym alokatorem SID. Jego TLS należy do Pythona, nie do C++ ADCS.

Dotychczasowe `adc.ncdc`/`adcs.ncdc` osobno testują rzeczywisty odbiorczy reaktor C++ i TLS OpenSSL. Łącznie testy potwierdzają granice modułów/transportu, nie pełne logowanie huba. Zobacz [TESTING.md](TESTING.md).
