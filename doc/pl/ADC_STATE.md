# ADC State — alpha-0.2.3

**Bieżący etap — alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizuje wybór zakresu MSG i dostarczanie do ograniczonych kolejek. Plany są powiązane z domeną macierzystego Chat Core i ID życia sesji; prywatne wiadomości nigdy nie trafiają zastępczo na czat publiczny.

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

Od alpha-0.2.4 [ADC SUP](ADC_SUP.md) udostępnia `ProtocolConnection`, łączący ten automat z negocjacją funkcji/hash i automatycznym zatwierdzeniem protokołu wyłącznie po poprawnym SUP. Tożsamość/hasło nadal wymagają zewnętrznych decyzji. Ten dokument opisuje pierwotne API automatu; podłączenie Network nadal wymaga uporządkowanego wyjścia.

## Zakres i własność

`dc24h/adc/state.hpp` dodaje niezależny `ConnectionStateMachine` po stronie huba do `dc24h_adc`. Odbiera sparsowane wiadomości i jawne decyzje aplikacji. Nie wysyła bajtów, nie przydziela globalnych SID, nie negocjuje hash, nie sprawdza tożsamości ani haseł. Network nadal wywołuje wyłącznie parser: ten etap nie dodaje działającego logowania ani nowego zachowania sieciowego. Dotychczasowe testy prawdziwego ncdc ADC/ADCS nadal sprawdzają początkowy HSUP i transport.

Model bazuje na kolejności logowania huba w [ADC 1.0.4, rozdziały 5.1 i 6.1](https://adc.sourceforge.io/ADC.html). Połączenie z hubem w BASE nie ma stanu DATA; binarne transfery klient–klient i rozszerzenia transferów z hubem są poza profilem. `disconnected` to dodatkowy lokalny stan terminalny dla błędów transportu/aplikacji.

Przyszłe połączenie powinno posiadać jedną niekopiowalną i nieprzenoszalną instancję na swoim wątku reaktora. Brak gniazd, blokad, callbacków, loggerów, zegarów i zależności od bazy. Pamięć zawiera początkowo pusty/docelowo czteroznakowy SID i sześć stałych flag. Nie zachowuje parametrów, PID, odpowiedzi hasłowej, nicku ani zestawu funkcji. `sid()` zwraca pożyczony widok; nie zachowywać go po końcu życia instancji ani przypisaniu SID. To lokalne powiązanie, nie gwarancja globalnej unikalności SID.

## Uporządkowane przejścia

| Stan bieżący | Zdarzenie / decyzja | Wynik |
| --- | --- | --- |
| PROTOCOL | HSUP klienta | Oczekiwanie na zatwierdzenie negocjacji; nadal PROTOCOL |
| PROTOCOL | `approve_protocol()` po zewnętrznej walidacji | Zezwolenie na odpowiedź huba; nadal PROTOCOL |
| PROTOCOL | Ukończone wysłanie ISUP, następnie ISID | Powiązanie SID i przejście do IDENTIFY |
| IDENTIFY | Opcjonalny IINF huba | Nadal IDENTIFY |
| IDENTIFY | BINF klienta z przypisanym SID | Oczekiwanie na zatwierdzenie tożsamości; nadal IDENTIFY |
| IDENTIFY | `approve_identity(guest)` | Przejście do NORMAL |
| IDENTIFY | `approve_identity(password_required)` | Przejście do VERIFY |
| VERIFY | Ukończone wysłanie IGPA, następnie HPAS klienta | Oczekiwanie na wynik sprawdzenia hasła; nadal VERIFY |
| VERIFY | `approve_password()` po poprawnej zewnętrznej weryfikacji | Przejście do NORMAL |
| Stan aktywny | Keepalive albo niefatalny STA huba/klienta | Bez zmiany stanu |
| IDENTIFY / VERIFY / NORMAL | Ukończony IQUI wskazujący SID tego połączenia | Przejście do DISCONNECTED |
| Stan aktywny | Fatalny HSTA/ISTA albo `disconnect()` | Przejście do DISCONNECTED |

Zatwierdzenia to zaufane decyzje aplikacji, nie walidatory. Wywołujący musi sprawdzić zgodność funkcji/hash przed zatwierdzeniem protokołu, PID/CID, wymagane pola/IP i unikalność nicku/CID przed tożsamością oraz odpowiedź na wyzwanie względem wybranego konta przed zatwierdzeniem hasła. Zatwierdzenie w teście nie jest usługą uwierzytelniania. Błędne sprawdzenie wymaga oczyszczonej odpowiedzi, gdy wyjście będzie dostępne, i rozłączenia. Brak ścieżki od odrzucenia do NORMAL, odnawiania terminów, automatycznego ponawiania i resetu. Nowe połączenie wymaga nowej instancji.

Powtórzenia SUP, zatwierdzenia, odpowiedzi, pierwszego INF, wyzwania i hasła są odrzucane w trakcie/po danym kroku. Sam INF/PAS nie awansuje stanu. Sukces STA nie zastępuje zatwierdzenia. Aktualizacje SUP i INF w NORMAL przechodzą kontrolę kolejności, ale wymagają osobnego konsumenta semantycznego. IQUI innej osoby i przekazywany fatalny STA nie zamykają tego połączenia.

## Obsługiwany profil komend

`observe(Direction, const Message&)` zwraca `StateResult` (`optional<StateError>`). Pusty wynik oznacza przyjęcie; stałe błędy to disconnected, invalid_message, invalid_type, invalid_state, invalid_sequence, wrong_source i unsupported_command. Błąd nie zmienia zapisanych danych; właściciel wybiera odpowiedź/zamknięcie. Nie oznacza to zalecenia bezterminowego kontynuowania po złośliwym wejściu. ISID przypisuje pamięć SID przed zmianą stanu; wyjątki alokacji propagują się.

| Stan | Komendy |
| --- | --- |
| PROTOCOL | STA, SUP, wychodzący SID |
| IDENTIFY | STA, INF, wychodzący QUI |
| VERIFY | STA, wychodzący GPA, przychodzący PAS, wychodzący QUI |
| NORMAL | STA, SUP, INF, MSG, SCH, RES, CTM, RCM, wychodzący QUI |
| DISCONNECTED | Żadne, także keepalive i zatwierdzenia |

Polityka kierunków/typów jest jawnym podzbiorem huba BASE, nie pełną autoryzacją rozszerzeń:

- Przychodzące SUP/PAS używają H; wychodzący SUP używa I. SID/GPA/QUI są wyłącznie wychodzące I; PAS wyłącznie przychodzący.
- Przychodzący INF używa B; wychodzący I lub B. Przed zatwierdzeniem tożsamości dozwolony jest jedynie wychodzący IINF.
- STA/MSG dopuszczają B/D/E/F oraz H przychodzący lub I wychodzący. Przed NORMAL status używa jedynie HSTA/ISTA.
- SCH/RES/CTM/RCM dopuszczają B/D/E/F; uprawnienia routingu, istnienie celu, aktywność/pasywność i semantyka pól należą do przyszłego dispatchera. Przyjęcie nie wykonuje komendy.
- Każdy przychodzący B/D/E/F musi mieć źródło równe przypisanemu SID. Wychodzący relay może wskazywać inne osoby. Typy C klient–klient, UDP U i przychodzący hubowy I są odrzucane.
- Nieznane komendy i GET/GFI/SND zwracają unsupported_command bez skutków ubocznych. Parser nadal zachowuje ich składnię; obsługa negocjowanych rozszerzeń i ramek binarnych wymaga osobnego projektu.

Poprawnym wejściem jest wyłącznie wiadomość z parsera. Moduł dodatkowo sprawdza zgodność nazwy/kind i dostęp do pozycyjnych SID/STA/QUI, ale nie waliduje ponownie całej składni ręcznie tworzonych obiektów `Message`. `KeepAlive` kierować do `keepalive()`; `ParseError` obsługiwać przez dotychczasową politykę błędnych ramek bez `observe`. Keepalive nigdy nie zatwierdza ani nie awansuje logowania.

## Przyszła integracja transportu

`to_client` oznacza ukończone uporządkowane wysłanie, nie dodanie do kolejki. Przed wysłaniem przyszły dispatcher musi sprawdzić wiadomość względem stanu i ograniczyć wyjście. Zdarzenia zatwierdzać w kolejności ukończenia wysyłki; po błędzie częściowego wysłania rozłączyć. Nie wywoływać obserwacji ISID/GPA/QUI po samym zakolejkowaniu. Szeregować wyniki odbioru i ukończenia wysyłek, aby nie awansować przed odpowiedzią; przedwczesny pipeline nie może pomijać bramek. Wydanie nie udostępnia API kontroli przed wysłaniem ani adaptera transportu.

Integracja wymaga też alokatora unikalnych SID, usług funkcji/hash i tożsamości, projektu haseł, usuwania PD przed przekazaniem, ograniczonej listy klientów, terminów handshake, dispatchera i kolejek wyjścia. Dotychczasowy odbiorczy transport nie może poprawnie ukończyć tych przejść, więc podłączenie egzekwującego automatu pozostaje kolejnym etapem. Liczniki API nadal oznaczają zaakceptowaną składnię, nie autoryzowane komendy ani zalogowane osoby.

## Weryfikacja

`adc.state` używa sprawdzeń aktywnych w Release dla obu ścieżek logowania, macierzy komenda/stan/kierunek obejmującej 15 komend BASE i nieznane rozszerzenie, przedwczesnych/powtórzonych zatwierdzeń i odpowiedzi, podszytych źródeł B/D/E/F, izolacji instancji, statusów/QUI, stanu terminalnego i kolejności parser → automat. Obejmuje przedwczesny pipeline, odzyskanie po błędnej składni i INF UTF-8 podawany bajtowo. Dotychczasowy ncdc osobno testuje rzeczywisty serwer odbiorczy; nie dowodzi podłączenia automatu do Network ani pełnego logowania. Zobacz [TESTING.md](TESTING.md).
