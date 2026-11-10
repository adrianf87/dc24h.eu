# ADC Parser — alpha-0.2.2

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
