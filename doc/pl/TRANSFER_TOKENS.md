# Transfer Tokens — alpha-0.6.7

**Bieżący etap — alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) dodaje startową politykę CTM/RCM/gości, zaufane nadania gotowych sesji i ponowną autoryzację przygotowania/wysyłki/echa. Wspólne kontrole IP i portu zachowują działanie zarządzanych profili. Testy natywne, mieszane ADC/ADCS i ncdc obejmują odmowy, podszywanie i dalszą komunikację.

**Weryfikacja lokalna alpha-0.6.7 (2026-11-14):** Przeszły wszystkie 153/153 testów CTest w Debug, Release i ASan/UBSan, w tym cztery scenariusze rzeczywistego ncdc z tokenem granicznej długości przez oba transporty huba i zwykłe/TLS połączenia klientów. Brak ostrzeżeń kompilatora. Poprawne są build/typecheck WebAdmin, 12 testów Node, 10 testów Chromium i integracja z API C++. Przeszły kontrole metadanych, UTF-8, zgodności języków/linków, składni zmienionych plików Python, przykładowego INI i białych znaków. Nie oczekiwano na koniec GitHub Actions; pełna macierz wspieranych systemów i obciążenie produkcyjne pozostają niesprawdzone.

Etap oparty bezpośrednio na main `9df87da184aab7376de0587bbb76121eccc9fe4b` po Client Capabilities. Wydziela wspólną politykę tokena połączenia i własny typ wartości oraz sprawdza dokładne przekazywanie tokenów. Gałąź numerowana jest publikowana osobno od main, z zachowaniem kodu i historii.

## Wartość i walidacja

`transfers/token.hpp` definiuje `max_token_bytes = 128`, `TokenError`, `validate_token(string_view)` i `Token::create(string_view)`. Fabryka zwraca `variant<Token, TokenError>`, a walidator opcjonalny typ błędu. `Token::value()` udostępnia niezmienny własny napis, równość porównuje dokładne bajty. Po poprawnym utworzeniu można zmienić lub usunąć dane wejściowe. Kopia ma własny napis; przeniesienie podlega zwykłym zasadom C++ dla przeniesionych obiektów.

| Zdekodowane dane | Wynik |
| --- | --- |
| Puste | `TokenError::empty` |
| Ponad 128 bajtów | `TokenError::too_long` |
| Nieprawidłowe UTF-8, brak NFC, znaki sterujące/formatujące lub separatory linii/akapitów | `TokenError::invalid_text` |
| 1..128 poprawnych bajtów NFC UTF-8 | Przyjęcie bez zmian |

Limit dotyczy zdekodowanych **bajtów**, nie znaków, i jest lokalną polityką, nie maksimum całego ADC. Sprawdzenie długości poprzedza kontrolę normalizacji i własne kopie. Walidacja UTF-8/NFC korzysta ze wspólnego ograniczonego walidatora tekstu, który może alokować pamięć tymczasową. Błędy alokacji propagują się; walidacja nie jest opisywana jako pozbawiona alokacji. Pusty lub zbyt długi token jest odrzucany przed kopiowaniem. Błąd alokacji własnego napisu również nie zmienia wejścia ani sesji.

Token jest nieinterpretowany i rozróżnia wielkość liter. Początkowe/końcowe spacje, sam ciąg spacji, ukośniki odwrotne, złożone Unicode, emoji, dane wyglądające jak liczby i prefiksy TO/PD są dozwolone przy zachowaniu tej samej polityki. Brak przycinania, normalizacji, zmiany wielkości liter, konwersji na liczbę, interpretacji prefiksu i zastępowania tokena. Zaufany inicjator podaje własny token; hub go nie generuje.

## Jedna polityka koordynacji

`Request` posiada teraz `Token` i dodaje `token_value()`. Dotychczasowe `token()` oraz dostęp do `max_token_bytes` przez request.hpp zachowują zgodność źródłową. Pozostają kontrole rodzaju/rodziny/portu i kolejność błędów; błędy fabryki tokena mapują się na `RequestError::invalid_token`. Natywny protokół sprawdza swoją fabrykę jednokrotnie. Zależne komponenty C++ wymagają przebudowania; stabilne binarne ABI nie jest obiecywane.

Wspólny pomocnik ADC wywołuje `validate_token` dla tokenów pozycyjnych CTM/RCM i błędów połączenia STA 41/42. Te błędy STA przyjmują ten sam token w nazwanym TO; powtórzone TO pozostaje błędem. Pozostałe kody STA zachowują wcześniejsze reguły. Kontrola składni, ucieczek i UTF-8 nadal poprzedza lub otacza walidację semantyczną: błędna ramka natywna lub sieciowa może najpierw zwrócić błąd kodeka. Nie zmieniono kolejności błędów ani polityki parsera.

CTM/RCM zachowują sprawdzone pola protokołu/portu/tokena/rozszerzeń, ich kolejność i kanoniczne ucieczki. Dodatkowe nazwane TO nie zastępuje tokena pozycyjnego. `ConnectionFrame::token()` zwraca dokładną zdekodowaną wartość pozycyjną z niezmiennego własnego Message. Wartość Transfer Core i ramka w kolejce nie zależą więc od późniejszej zmiany danych wywołującego. Wysyłka i odczyt tokena nie alokują po przygotowaniu planu i odbiorcy.

## Przekazywanie i własność

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html), rozdziały 5.3.8–5.3.9 i 6.2, wymaga zachowania protokołu/tokena w odpowiedzi na RCM oraz tokena w CINF łączącego się klienta. Handshake wykonują klienci; hub przekazuje CTM/RCM i błędy klientów, nie odbiera ich CINF ani bajtów plików.

D trafia do dokładnego życia gotowej sesji celu, E dodatkowo niezależnie do źródła. Token nie wybiera odbiorcy, nie wiąże SID, nie autoryzuje połączenia i nie identyfikuje konta. Niezależne żądania i pary klientów mogą używać identycznych bajtów. Nie odtwarzają one SID rozłączonego klienta ani nie zmieniają celu przygotowanego planu. Jednorazowy plan nadal zapobiega ponownemu wysłaniu tej samej przygotowanej operacji, niezależnie od równości tokenów.

Etap nie dodaje rejestru oczekujących tokenów, polityki unikalności, domniemanej korelacji RCM→CTM, limitu czasu, pamięci odtworzeń ani uprawnień opartych na tokenie. Tokeny wyszukiwania mają osobną wcześniejszą przestrzeń/politykę i rejestr; tokeny połączenia do niego nie trafiają. Oczekiwanie na właściwy token w połączeniu bezpośrednim sprawdzają klienci. Pełne uwierzytelnianie klientów, produkcyjny pinning TLS, NAT traversal, relay i transfer plików pozostają przyszłymi pracami.

Błędy walidacji i odbiorcy przekazują ograniczone ogólne opisy statusu bez treści tokena. Dane połączenia pozostają poza logami, SQL, API/WebAdmin i licznikami Chat/Search. Dotychczasowe kontrole gotowości/źródła/stanu/kolejki/adresu pozostają bez zmian.

## Weryfikacja

`transfers.tokens` sprawdza typowane błędy, dokładne granice bajtów wielobajtowych i wzrost długości po ucieczkach, zachowanie spacji/prefiksów/wielkości liter, własność/kopiowanie/przenoszenie, granice przed kopiowaniem, awarie alokacji, zgodność Request/CTM/RCM/błędów klientów, powtórzone TO, izolację danych wywołującego, niezmienną ramkę, odbiorców D/E, wysyłkę bez alokacji, odmowę odtworzenia kopii planu i legalne ponowne użycie tokena w nowym żądaniu.

`transfers.tokens.process` uruchamia obie rodziny adresów i mieszane ADC/ADCS. Równoległe pary CTM i RCM celowo powtarzają tokeny. Sprawdza oba zakresy, RCM→CTM, odpowiedzi statusu, fragmentację nagłówków, rozszerzenia TO, odwracalne błędy tokenów, ponowne logowanie/izolację SID, odrzucanie niejednoznacznego statusu i prywatność API/logów.

Cztery nowe scenariusze ncdc (`adc.tokens.ncdc`, `adc.tokens.adcs.ncdc`, `adcs.tokens.ncdc`, `adcs.tokens.adcs.ncdc`) używają obu transportów huba i zwykłych/TLS gniazd klientów z weryfikacją certyfikatu. Rzeczywisty aktywny klient zwraca w CTM token o 128 zdekodowanych bajtach, ze skrajnymi spacjami, ukośnikiem, polskim tekstem i emoji; drugi klient kończy CSUP/CINF z tym tokenem. Nieobsługiwany protokół następnie daje rzeczywiste DSTA z dokładnie tą samą wartością TO. Pozostają wcześniejsze testy tokenów generowanych przez ncdc i pozostałe scenariusze interoperacyjności.

Zobacz [Testy](TESTING.md), [Transfer Core](TRANSFER_CORE.md), [Connection Routing](CONNECTION_ROUTING.md), [Protocol Negotiation](PROTOCOL_NEGOTIATION.md), [ADC CTM](ADC_CTM.md) i [ADC RCM](ADC_RCM.md).
