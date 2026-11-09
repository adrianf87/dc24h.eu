# Protokół ADC — alpha-0.1.3

Implementacja obejmuje parser składni, nie kompletny hub. Punktem odniesienia
jest [ADC 1.0.4](https://dc-protocols.github.io/ADC.html), rozdział 3.
Rozpoznawanie nazw BASE nie oznacza wykonania komend ani zgodności pełnej sesji.

## Kontrakt parsera

Biblioteka `dc24h_adc`, nagłówek `include/dc24h/adc/parser.hpp`, nie używa
Core, loggera, gniazd ani bazy danych.

- `parse_line(string_view)` przyjmuje jedną treść ramki **bez końcowego LF**.
  Zwraca wariant `KeepAlive`, `Message` albo `ParseError`.
- `Message` zawiera typ, oryginalną nazwę komendy, klasyfikację `Command`,
  opcjonalne source/target, selektory cech i odkodowane parametry w kolejności.
  Wszystkie wartości są własnością wyniku; nie wskazują bufora wejściowego.
- `recognize_command` rozpoznaje 15 komend: STA, SUP, SID, INF, MSG, SCH, RES,
  CTM, RCM, GPA, PAS, QUI, GET, GFI, SND. Nazwy rozszerzeń zachowuje jako `unknown`.
- `StreamDecoder::feed(bytes, sink)` dostarcza wyniki synchronicznie, w kolejności.
  Jedno wywołanie może dostarczyć zero, jeden lub wiele wyników. Nie przechowuje
  wiadomości po callbacku. Sink nie może ponownie wywołać tego samego dekodera;
  wyjątki callbacku propagują się do wywołującego.
- `finish()` zgłasza `truncated_frame` dla nieukończonej ramki i resetuje dekoder.
  Kolejne wywołanie nie zgłasza jej ponownie. Ramka odrzucona za rozmiar nie
  jest ponownie liczona jako ucięta.

## Sprawdzana składnia

Obsługiwane są nagłówki B/C/D/E/F/H/I/U, wymagane pola routingu, SID base32
oraz CID bez narzuconej długości konkretnego hasha. Parser sprawdza strukturę
selektorów cech w F i zachowuje ich kolejność. Rozdziela parametry pojedynczą
spacją i dekoduje `\s`, `\n`, `\\`; inne escape powodują odrzucenie ramki.
Pusty LF jest keepalive. Nie ignoruje pustych parametrów ani końcowej spacji.
Nieznana poprawna składniowo komenda pozostaje wiadomością, gotową do przyszłego
routingu według typu.

Biblioteka [utf8proc](https://juliastrings.github.io/utf8proc/doc/utf8proc_8h.html)
waliduje UTF-8 i pozwala porównać treść z NFC. Parser odrzuca tekst wymagający
normalizacji zamiast zmieniać identyfikatory lub dane klienta. Uwzględnia znaki
czterobajtowe, błędne kodowania, surogaty i sekwencje wykraczające poza Unicode.

## Lokalne limity i polityka

Te ograniczenia są decyzją dc24h.eu, nie limitami narzuconymi przez ADC:

| Ograniczenie | Wartość / zachowanie |
| --- | --- |
| Treść ramki, bez LF | Do 16384 bajtów |
| Parametry, bez routingu | Do 128 |
| Surowe ASCII control i DEL | Odrzucane, w tym NUL, tab i CR; brak akceptacji CRLF |
| Błędna ramka | Jeden błąd, następna linia nadal parsowana |
| Przekroczenie długości | Jeden błąd od razu; odrzucanie danych do LF |
| Zamknięcie z częściową ramką | Jeden błąd ucięcia |

Stan dekodera zawiera najwyżej 16 KiB nieukończonej treści; po przekroczeniu
limitu bufor jest czyszczony. Pamięć alokatora i chwilowe obiekty parsera mają
własny narzut. Nie powstaje kolejka wszystkich ramek otrzymanych w odczycie.

## Integracja TCP i obserwowalność

Każde połączenie `TcpServer` posiada niezależny dekoder. Network podaje mu
odczytane bajty, zlicza wyniki i zwalnia wynik po przetworzeniu. Nie wykonuje
poleceń i nie wysyła odpowiedzi. Przy zamknięciu połączenia, także przez timeout
lub zatrzymanie procesu, wywołuje `finish()`.

Log stopu i `Statistics` zawierają:

- `adc_frames` / `frames`: niepuste wiadomości poprawne składniowo.
- `adc_unknown` / `unknown_commands`: podzbiór `frames` z nieznaną nazwą komendy.
- `adc_invalid` / `invalid_frames`: błędne, za długie lub ucięte ramki.
- `adc_keepalives` / `keepalives`: puste LF.

Nie logujemy parametrów, PID ani hashy PAS. Nie ma logu dla każdej ramki,
co ogranicza możliwość zalania logów danymi klienta. Syntaktycznie poprawna
ramka nie potwierdza tożsamości nadawcy, zgodności stanu ani wykonania komendy.

## Poza zakresem tego wydania

Nie ma serializera, walidacji semantyki komend/pól (np. liczb w INF, wymaganych
parametrów SUP, hashy PID/CID), kontroli dozwolonych typów dla konkretnych
komend, automatu PROTOCOL/IDENTIFY/VERIFY/NORMAL, przydzielania SID, routingu,
uwierzytelniania, chatu, wyszukiwania ani transferów. Parser nie sprawdza
zgodności deklarowanego SID/IP z połączeniem. U pozostaje składnią obsługiwaną
przez bibliotekę; serwer nie otwiera gniazda UDP.

Test ncdc obejmuje wysłany przez rzeczywisty klient początkowy HSUP. Serwer
nie odpowiada ISUP/ISID, więc logowanie klienta nie dochodzi do NORMAL.
Szczegóły testów: [TESTING.md](TESTING.md).
