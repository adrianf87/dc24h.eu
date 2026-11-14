# STA / QUI — alpha-0.2.9

**Bieżący etap — alpha-0.6.9:** [Error Handling](ERROR_HANDLING.md) ujednolica diagnostykę połączeń i wspólny niezmienny routing STA do stałej listy gotowych sesji. Przygotowanie, kolejki odbiorców, diagnostyka zamykania i wyjściowa walidacja STA izolują błędy pamięci; kierowany fatalny status klienta zachowuje sesje huba. Sprawdzają to testy natywne, mieszane ADC/ADCS i ncdc z resetem/błędami/dalszą komunikacją.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

Etap dodaje typowaną walidację statusów/rozłączeń, diagnostykę protokołu, przekazywanie STA i ograniczone wyjście kończące połączenie w rzeczywistym reaktorze C++ ADC/ADCS. Podstawa to main `da1521502c4a8936b3427a7d38fea6b904686b39`, po scaleniu logowania alpha-0.2.8. Historia i wcześniejsze gałęzie zostają zachowane; nowa gałąź nie scala się automatycznie.

## Statusy i walidacja

`adc/status.hpp` definiuje StatusCode z niezależnym poziomem i numerem błędu. Poziom 0 oznacza sukces i wymaga numeru 00 (`000`); 1 oznacza błąd odwracalny, 2 fatalny. Nieznane numery w dwóch ostatnich grupach są zachowywane, np. 199 i 299. Wartości 001, 300 czy 999 są składniowo trzema cyframi, ale nie przechodzą walidacji semantycznej. Parser nadal sprawdza składnię; automat stosuje semantykę przed zmianą stanu.

`validate_status` i `validate_quit` zwracają pożyczone widoki lub stałe StatusError. Sprawdzają wspólną składnię parsera/serializera, tekst/NFC i zakodowaną długość, command/kind, parametry, powtarzające się nazwy oraz dziedziny znanych flag. Opis STA ma limit 2048 zdekodowanych bajtów UTF-8, wartość flagi STA 1024, QUI 2048; nadal maksymalnie 16384 bajty zakodowanej treści i 128 parametrów. To lokalne limity alpha, nie limity specyfikacji sieciowej.

FC w STA ma cztery wielkie znaki funkcji/komendy, FM/FB dwuznakową nazwę pola, TL nieujemną liczbę signed 64-bit, TO/PR niepusty widoczny ASCII, I4/I6 numeryczny adres odpowiedniej rodziny. Flagi są sprawdzane, gdy występują; moduł nie wymyśla nieznanych danych transferu/konta i nie wymaga wszystkich flag specyficznej komendy do odczytania zdalnego statusu. Nieznane pola pozostają ograniczonymi metadanymi bez uprawnień.

QUI jest wyłącznie komunikatem IQUI huba. Pozycyjny SID i opcjonalny inicjator ID używają wspólnego kodeka SID. TL dopuszcza -1 albo nieujemną liczbę sekund signed 64-bit. MS to tekst widoczny dla osoby, DI to flaga obecności. RD jest odczytywane jako ograniczone niepuste metadane, bez wykonywania przekierowania lub interpretacji URL. Fabryki tworzą własne Message i odrzucają błędne lokalne opcje bez ucinania. Serwer obecnie nie emituje RD, ID, DI ani TL: przekierowania, kicki/bany i ograniczenia reconnect nie są włączone w tym etapie.

## Statusy huba i przekazywane

HSTA 000 i HSTA 1xx są przyjmowane w każdym żywym stanie, bez automatycznej odpowiedzi, zatwierdzenia tożsamości i odnawiania terminów poza dotychczasową polityką bezczynności transportu. HSTA 2xx zamyka nadawcę bez odbijania jego opisu i pętli odpowiedzi; pozostali przyjęci użytkownicy otrzymują samo IQUI. Opisy klienta i wartości flag nie trafiają do logów operatora.

BSTA/DSTA/ESTA/FSTA w NORMAL przechodzą kontrolę źródłowego SID i semantyki przed przekazaniem. D trafia tylko do gotowego celu; E również do nadawcy, jeden raz przy takim samym celu; B do wszystkich gotowych, także nadawcy. F porównuje wszystkie wymagane/wykluczane selektory z INF SU odbiorcy, nie z jego HSUP. Sesje z niewysłanym powitaniem, zamykane i przerwane są pomijane. Nieobecny/niegotowy cel daje odwracalne ISTA 140. Przekazywany status fatalny dotyczy relacji klient–klient i nie zamyka sesji z hubem. Nie implementuje transferów ani uprawnień do wykonywania komend.

Przekazywany status zachowuje tylko znane FC/FM/FB/TL/TO/PR/I4/I6. Nieznane pola, także nazwane PD lub podobne do PAS, są pomijane. Opis i zachowane wartości są publicznymi danymi klienta, nie zaufanymi poleceniami huba. Oryginalny Message nie jest zmieniany. Próba IQUI/HQUI/BQUI klienta nie rozłącza ofiary; odrzucany jest nadawca. Zwykłe wyjście klienta odbywa się przez zamknięcie gniazda albo fatalne HSTA.

## Stała diagnostyka lokalna

Lokalne fatalne odpowiedzi mają stałe angielskie opisy i dotychczasowe kody INF/SUP/SID: 221 niepoprawny nick, 222 zajęty nick, 224 zajęty CID, 227 błędna tożsamość, 243 brak/błąd informacji klienta, 245 brak funkcji, 246 błędny adres, 247 brak wspólnego hash, 211 limit sesji i 210 błąd wewnętrzny. Błąd stanu/kolejności ma teraz 244 z FOURCC komendy w FC; pozostałe błędy protokołu/źródła/semantyki mają 240. Brak funkcji wskazuje BASE lub usunięty TIGR przez FC. Parametry klienta nie są wstawiane do opisów.

Poprawne stanowo niezaimplementowane komendy NORMAL, również nieznane rozszerzenia, dają odwracalne 125 z FC. Obsługa czatu/wyszukiwania/transferów pozostaje planowana. Błędne składniowo ramki nadal są ignorowane w ramach budżetu błędów, bez odpowiedzi na każdą ramkę. Przekroczenie limitu ramek/błędów inicjuje jeden stały fatalny komunikat o limicie zasobów, o ile transport pozwala go wysłać.

## Kolejność kończenia i własność

Przy lokalnym odrzuceniu lub upływie terminu logowania/bezczynności/ramki/limitu:

1. Dopuszczanie zamyka się raz; dalsze komendy nie zmieniają stanu/metadanych ani nie odnawiają kończenia.
2. Kolejka zachowuje pierwszą ramkę, która może być częściowo wysłana lub przypięta przez SSL WANT. Późniejsze zwykłe ramki są anulowane z dokładnym odjęciem bajtów.
3. Hub dopisuje fatalne ISTA, a przy przypisanym SID końcowe własne IQUI ze stałym MS. Jeżeli zachowany początek to oczekujące ISID, zostaje wysłane przed STA/QUI. Klient bez otrzymanego i oczekującego SID dostaje tylko ISTA.
4. Pełne własne QUI jest ostatnią ramką aplikacyjną. Żadne dokończenie powitania, odpowiedź ani relay nie dopisze za nim danych. Rezerwacja sesji trwa podczas opróżniania; ramki końcowe celowo nie uruchamiają zwykłego obserwatora fatalnego STA, który zbyt wcześnie zwolniłby SID.
5. Faktyczne sprzątanie transportu ogłasza jedno samo IQUI pozostałym przyjętym osobom, usuwa należące do sesji indeksy nick/CID/SID, anuluje resztę wyjścia i zwalnia dzierżawę SID. Powtórne sprzątanie jest bezpieczne; FIFO zachowuje stare INF, usunięcie i późniejszą nową obecność.

Limity 256 KiB/klient i 16 MiB/hub obejmują także powiadomienia końcowe. Jeżeli nawet one się nie mieszczą, przypięty początek nie jest przepisywany, a połączenie zamyka się natychmiast. EOF/reset, błędy zapisu, zatrzymane wyjście, nieukończony/błędny TLS i zatrzymanie procesu także mogą anulować powiadomienia: dostarczenie na niesprawnym transporcie nie jest gwarantowane. Shutdown nadal pomija rozsyłanie obecności i nie czeka na klientów. TLS close_notify/reload nie jest dodawany.

## Terminy, logi i administracja

INI `[security] close_timeout=2` dopuszcza 1–3600 sekund, bez nowego przełącznika CLI. Ogranicza całe końcowe opróżnianie od pierwszego zaobserwowanego przejścia do zamykania, bez odnawiania przez postęp odczytu/zapisu. Timery wejścia/logowania/bezczynności nie inicjują kolejnych powiadomień podczas zamykania. Limity wyjścia nadal ograniczają pamięć; oczekujący odczyt TLS wymagający zapisu może zakończyć zależność przed wysłaniem powiadomień, a zwrócone dane są odrzucane. Nieblokujący reaktor nadal sprawdza stop i terminy okresowo.

Upływ bezczynności, logowania i niedokończonej ramki próbuje teraz powiadomić protokołem przed zamknięciem. Zatrzymane wyjście i niedostępny TLS nadal zamykają się natychmiast. Licznik bezpieczeństwa zlicza zamknięcie raz także przy zbiegu limitu ramek i przepełnienia wyjścia. Nowe stałe opcjonalne zdarzenia INFO `peer_status` i `close_timeout` opisują fatalny raport klienta i wyczerpany czas kończenia, bez tekstów statusu/QUI, nicków lub adresów. Dotychczasowe ostrzeżenia operatora zachowują tłumaczenia US/PL; opisy sieciowe pozostają angielskie.

Schemat API v1 i WebAdmin pozostają bez zmian. Aktywne połączenia to gniazda, także w końcowym opróżnianiu, nie liczba gotowych użytkowników. Liczniki parsowania nie oznaczają autoryzacji. Brak nowych migracji SQL, banów/kont, operacji administracyjnych i polityki haseł.

## Testy

`adc.status` sprawdza wszystkie trzycyfrowe kody, granice tekstu/pól, nieznane błędy, znane/nieznane/powtarzane flagi, opcje QUI, brak zmiany stanu przy błędach, sukces bez przyjęcia, częściowe wysyłki końcowe, stabilny początek, anulowane wyjście, oczekujące ISID, jednokrotne sprzątanie, B/D/E/F i fatalny raport klienta. `adc.status.process` uruchamia rzeczywisty mieszany TCP/OpenSSL: końcowe ramki/EOF, fałszywe QUI, błędne STA, routing, reconnect, prywatność i powiadomienia o terminach/limitach. Dotychczasowe testy wolnego odbiorcy i blokady SSL pozostają wymagane.

`adc.status.ncdc` i `adcs.status.ncdc` używają tymczasowych PTY rzeczywistego ncdc: klient widzi powód zajętego nicka, dotychczasowa osoba pozostaje połączona, a po jej odejściu ncdc łączy się ponownie do NORMAL i otrzymuje odwracalną odpowiedź braku implementacji komendy. TLS kończy się w C++. Dotychczasowe pełne logowanie/reconnect i testy SUP/SID/INF/Tiger pozostają osobnymi regresjami. Pełna konfiguracja rejestruje 40 testów CTest. Dowolne awarie alokacji, wszystkie zależności TLS WANT_READ, zmierzone obciążenie/fuzz/side channel i pozostałe OS wymagają osobnej weryfikacji.

`adc.close.process` używa wyłącznie testowego `--wrap=accept4`, małych buforów zapisu serwera i okien odbioru klientów. Wymusza oczekujące zapisy TCP i SSL, uruchamia zamykanie przez bezczynność, obserwuje dokładnie dwa close_timeout mimo zablokowanego wyjścia i sprawdza logowanie nowego gościa. Nie zmienia dostrajania produkcyjnego ani sysctl hosta; wymaga linkera zgodnego z GNU.

Podstawa: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), rozdziały 3.3, 5.1, 5.3.1 i 5.3.12.
