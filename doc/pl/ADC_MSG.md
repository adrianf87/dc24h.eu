# ADC MSG — alpha-0.4.1

**Bieżący etap — alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizuje wybór zakresu MSG i dostarczanie do ograniczonych kolejek. Plany są powiązane z domeną macierzystego Chat Core i ID życia sesji; prywatne wiadomości nigdy nie trafiają zastępczo na czat publiczny.

**Etap dostarczony w alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) dodaje DMSG tylko do celu i EMSG do celu oraz nadawcy, pojedyncze wiadomości do siebie, powiązanie celu z ID życia i kontrolę PM. Czat publiczny pozostaje oddzielony; grupy/historia/moderacja to kolejne etapy.

**Etap dostarczony w alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) dodaje osobny adapter polityki i rozsyłania BMSG oraz jedną wspólną niemutowalną ramkę na publikację. Offsety i logiczne budżety odbiorców pozostają niezależne; zachowano bramki źródła/stanu/prywatności i FIFO.

**Etap dostarczony w alpha-0.4.2:** [Chat Core](CHAT_CORE.md) dodaje osobny silnik wiadomości niezależny od protokołu oraz publiczne BMSG, z kopią dla nadawcy i /me. Czat prywatny/grupowy/feature/hub, moderacja i historia pozostają osobnymi etapami.

Gałąź alpha-0.4.1 utworzono bezpośrednio z main `b528248dbe6984c6554821d758f0f5d0cbc285fb`, po scaleniu Statistics. Dotychczasowy kod, historia i gałęzie są zachowane. Domyślny język serwera to EN/US UTF-8; dostępne są dokumentacja PL i dotychczasowe katalogi runtime.

## Zakres

Etap implementuje kodek polecenia BASE `MSG text` z [sekcji ADC 5.3.5](https://adc.sourceforge.io/ADC.html#_msg). Korzysta ze wspólnego parsera, dekodera strumienia, serializatora, typu Sid i maszyny stanów. Dostarczanie czatu, wybór odbiorców, echo, rozwiązywanie grup PM i uprawnienia pozostają kolejnymi etapami. Bez zmian schematu SQL, JSON API, konfiguracji, funkcji WebAdmin i kluczy tłumaczeń; zaktualizowano metadane wydania.

| Nagłówek | Reprezentacja |
| --- | --- |
| `BMSG source text` | Broadcast z SID źródła |
| `DMSG source target text` | Direct z SID źródła i celu |
| `EMSG source target text` | Echo z SID źródła i celu |
| `FMSG source selectors text` | Feature z zachowaniem selektorów |
| `HMSG text` | Klient do huba |
| `IMSG text` | Hub do klienta |

CMSG/UMSG są odrzucane przez kodek tego profilu huba; ogólny parser składni pozostaje neutralny i może wcześniej odczytać taki nagłówek. Kierunek, NORMAL i własność źródła sprawdza osobno maszyna stanów. Sam nagłówek nie potwierdza istnienia odbiorcy ani dopasowania funkcji.

Tekst to jeden wymagany niepusty parametr po odkodowaniu, bez dodawanego nicka. Nie jest przycinany ani normalizowany. Same spacje można zapisać przez escaping. `IMSG ME1` zawiera tekst, nie flagę. Wymagane są UTF-8/NFC; błędne dane są odrzucane. Spacja, LF i backslash używają wspólnego escaping ADC. Surowe ASCII controls, w tym CR, NUL, TAB i DEL, są zabronione; zakodowany LF jest prawidłową treścią.

## Flagi i rozszerzenia

| Pole | Walidacja |
| --- | --- |
| `PM<group-SID>` | Dokładnie cztery wielkie znaki base32 SID, wynik jako opcjonalny Sid |
| `ME<integer>` | Dziesiętna liczba ADC int64 o wartości 0 lub 1, wynik jako opcjonalny bool |

Duplikaty PM/ME są odrzucane, także jednakowe wartości i ME0 ME1. Puste wartości znanych pól, błędny SID, niecałkowite/przepełnione/niedozwolone ME są odrzucane. ME01 i ME-0 zachowują oryginalny zapis podczas parsowania/serializacji; lokalny builder emituje ME0/ME1. Brak ME różni się od jawnego false. PM może wskazywać grupę różną od nadawcy; sama składnia nie uwierzytelnia ani nie rozwiązuje grupy. Późniejsza polityka zwykłych prywatnych rozmów musi egzekwować wymagane powiązanie ze źródłem.

Nieznane poprawne pola pozostają w kolejności wejścia, także powtórzone i puste. Są nieinterpretowanymi danymi rozszerzeń. Ten etap nie udostępnia rozsyłania ani sanitizera; przyszły dispatcher musi ustalić politykę prywatności/rozszerzeń przed przekazywaniem. Treść i wartości pól nie trafiają do logów ani API.

## Interfejs C++

Plik `include/dc24h/adc/msg.hpp` udostępnia:

- `parse_msg(body) -> variant<Message, ParseError, MsgError>`: jedna ramka bez końcowego LF. Keepalive/inne polecenie nie jest MSG. Błędy składni pozostają ParseError, błędy semantyki są MsgError. Message posiada teksty i positional_count=1.
- `validate_msg(message) -> variant<MsgView, MsgError>`: sprawdza również ręcznie skonstruowane obiekty, nagłówki, tekst, kodowanie i rozmiar ramki. MsgView pożycza tekst, a PM/ME kopiuje jako wartości typowane. Zmiana/zniszczenie Message unieważnia widok tekstu.
- `serialize_msg(message) -> variant<string, MsgError>`: waliduje semantykę i zwraca jedną samodzielną ramkę zakończoną LF. Błąd nie zwraca częściowego wyjścia. Poprawne nagłówki, tekst, nieznane pola i kolejność flag są zachowywane.
- `make_msg(text, MsgOptions) -> Message`: sprawdzany lokalny builder, domyślnie IMSG. Opcje obejmują typ nagłówka, typowane SID źródła/celu/grupy, ME i selektory funkcji. Błędne opcje powodują invalid_argument; błędy alokacji propagują się. Bez obcinania tekstu i zmian stanu serwera.

Obowiązują istniejące limity aplikacji: 16384 bajty zakodowanej treści ramki bez LF, 128 parametrów z tekstem i 128 selektorów funkcji. Nie są to limity narzucone przez ADC. Nagłówek, separatory i rozwinięte sekwencje escaping wchodzą do budżetu. Wynik ma najwyżej 16385 bajtów z LF. Nie dodano osobnego limitu długości czatu. Walidacja może alokować; wspólny serializator używa parsera do kontroli spójności.

## Integracja

ConnectionStateMachine przed semantyką MSG sprawdza kierunek/typ, NORMAL i powiązany SID. Poprawne MSG nie zmienia stanu logowania. Błędne flagi powodują StateError::invalid_message, istniejącą odpowiedź fatal ISTA 240 z FOURCC, własne IQUI i ograniczone sprzątanie. Błędny stan zachowuje 244, podszycie źródła 240. Błędna ogólna składnia jest odrzucana według dotychczasowych limitów i umożliwia odzyskanie po następnym LF.

Poprawne MSG nadal otrzymuje recoverable ISTA 125 „Command not implemented”. Potwierdza to obsługę składni/stanu bez deklarowania ukończonego dostarczania czatu. Żadna wiadomość nie jest rozsyłana, PM nie tworzy rozmowy ani rezerwacji. Dotychczasowe limity, statystyki i gotowość pozostają w mocy.

## Testy

`adc.msg` obejmuje oficjalne przykłady, sześć nagłówków, PM/ME, rozszerzenia, własność i widoki, buildery, niespójne obiekty, Unicode/escaping, dokładne granice rozmiaru i parametrów, fragmentację, odzyskiwanie po błędach/EOF i kolejność bramek stanów/źródła. Ograniczony tryb testowy --roundtrip sprawdza przechwycone ramki prawdziwego ncdc.

`adc.msg.process` używa prawdziwych połączeń ADC/ADCS: fragmentowane Unicode, nagłówki B/D/E/F/H, PM/ME, odzyskiwanie składni, błędy fatal, podszycie źródła, przedwczesny MSG, sprzątanie i prywatność logów/obserwatora. ncdc wysyła zwykły tekst, `/me` i `/msg` przez ADC/ADCS; ramki plaintext przechodzą roundtrip bajt w bajt, a ścieżka szyfrowana potwierdza dalszą pracę bez ujawniania treści. Patrz [podręcznik ncdc](https://dev.yorhel.nl/ncdc/man).

Przed publikacją wymagane są pełne regresje CTest i WebAdmin. Lokalna weryfikacja nie potwierdza obciążenia produkcyjnego ani wszystkich wspieranych systemów.
