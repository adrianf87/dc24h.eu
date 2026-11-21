# Obecność użytkowników i czat

[English (en-US)](../../us/alpha-0.1/PRESENCE-CHAT.md)

W etapie E3 nasłuchy ADC i ADCS mają wspólną listę użytkowników. Strand serwera zarządza rejestrem i ustala kolejność logowań, aktualizacji, wiadomości oraz rozłączeń. Dla przyjętego użytkownika przechowuje SID, publiczny profil, klucz porównywania nicku, ogłoszone rozszerzenia i stan limitu częstotliwości. Rejestr połączeń wiąże SID z sesją. Strand gniazda obsługuje ramki, TLS, stan sesji ADC, limity czasu i zapisy.

## Logowanie i obecność

1. Hub negocjuje `BASE` i `TIGR`, przydziela SID oraz wysyła własne `IINF`. W stanie `IDENTIFY` klient wysyła początkowe `BINF`. Sesja E1 sprawdza SID i zgodność CID ze skrótem TIGR obliczonym z PID.
2. Strand serwera sprawdza pola publiczne oraz unikalność nicku i CID wśród użytkowników online. Odrzucenie logowania skutkuje wysłaniem błędu krytycznego `ISTA` do tego połączenia.
3. Hub kolejkuje `BINF` obecnych użytkowników, a na końcu `BINF` dołączającego klienta. Sesja klienta przechodzi do `NORMAL`, a hub powiadamia pozostałych o dołączeniu. Kolejki gniazd zachowują kolejność późniejszych logowań, zmian i rozłączeń.

Opcja `--max-nick-codepoints` domyślnie dopuszcza 32 znaki mierzone w punktach kodowych i przyjmuje wartości od 1 do 256. E1 wymaga poprawnego UTF-8 w NFC. Nick musi być niepusty i składać się z punktów kodowych powyżej U+0020, z wyłączeniem znaków sterujących, formatujących i separatorów Unicode. Hub zachowuje pisownię nicku, a do porównań stosuje pełne składanie wielkości liter Unicode i normalizację NFC. Dzięki temu `Straße` i `STRASSE` kolidują. Porównanie jest niezależne od ustawień językowych i nie utożsamia podobnie wyglądających liter z różnych alfabetów.

W stanie `NORMAL` klient aktualizuje własny profil za pomocą `BINF`. Pole bez wartości usuwa atrybut opcjonalny. CID pozostaje stały, a zmiana nicku podlega tym samym regułom co przy logowaniu. Hub rozsyła wyłącznie zmienione pola publiczne. Po odrzuceniu aktualizacji obowiązuje poprzedni profil. Zarówno pełny profil publiczny, jak i aktualizacja mieszczą się w limicie `--input-bytes`.

Pola publiczne to `ID`, `NI`, `DE`, `VE`, `AP`, `AW`, `SU`, `I4`, `I6`, `U4`, `U6`, `SS`, `SF`, `SL`, `HN`, `HR`, `HO`, `US`, `DS`, `AS` i `AM`. Strand gniazda usuwa PID i pozostałe pola prywatne przed przekazaniem profilu do rejestru. Hub pomija także nieznane pola i role deklarowane przez klienta. Ogłoszone pole `I4` lub `I6` przyjmuje rzeczywisty adres połączenia TCP z odpowiadającej rodziny.

Po zamknięciu sesji hub usuwa wpis, zwalnia SID i wysyła pozostałym użytkownikom jedno `IQUI <sid>`. Utrata TCP, zamknięcie TLS, limity czasu oraz błędy krytyczne protokołu lub transportu korzystają z tej samej ścieżki porządkowania. Klient może zakończyć sesję, zamykając połączenie lub wysyłając krytyczne `HSTA`. Przy kontrolowanym zamknięciu serwera każdy zalogowany klient otrzymuje własne `IQUI` jako ostatnią ramkę, jeśli gniazdo może opróżnić kolejkę. Nowe połączenie negocjuje SID od nowa i może skorzystać ze zwolnionego nicku oraz CID. [ADC 1.0.4, rozdz. 5.1 i 5.3](https://adc.sourceforge.io/ADC.html)

## Czat i routing

Klient wysyła `MSG` w stanie `NORMAL`, posługując się SID przydzielonym jego połączeniu. Hub sprawdza zdekodowaną treść i flagi, a przed przekazaniem koduje znaki zgodnie z ADC. Flaga `ME1` oznacza wyświetlanie komunikatu w trzeciej osobie.

| Ramka | Odbiorcy i wymagania |
| --- | --- |
| `BMSG <from> <text>` | Wszyscy zalogowani, w tym nadawca. |
| `FMSG <from> <selectors> <text>` | Zalogowani, których `SU` zawiera wszystkie wymagane rozszerzenia (`+`) i żadne wykluczone (`-`). |
| `DMSG <from> <to> <text> PM<from>` | Wyłącznie zalogowany odbiorca. |
| `EMSG <from> <to> <text> PM<from>` | Zalogowany odbiorca i nadawca. Wiadomość do siebie jest wysyłana jeden raz. |

Czat prywatny między dwiema osobami wymaga flagi `PM<from>` w ramce skierowanej do konkretnego SID. Hub odrzuca flagę `PM` w ramkach publicznych i filtrowanych. Niedostępny odbiorca, cudzy SID nadawcy lub nieprawidłowe flagi powodują błąd odwracalny wysyłany nadawcy. Błąd nie zawiera treści wiadomości ani SID odbiorcy. Logi serwera pomijają treści ramek. [ADC 1.0.4, rozdz. 3.3 i 5.3.5](https://adc.sourceforge.io/ADC.html)

## Limity

- `--max-message-bytes` domyślnie dopuszcza 1024 bajty zdekodowanego UTF-8 i przyjmuje wartości od 1 do 4096. Nadal obowiązuje limit E1 dla zakodowanego parametru i całej ramki.
- Każda sesja ma osiem tokenów wspólnych dla czatu i aktualizacji `INF` po zalogowaniu. Odzyskuje jeden token co 250 ms, do ośmiu. Polecenia ponad limit otrzymują błąd odwracalny. Strand gniazda czeka na odpowiedź stranda serwera dla najwyżej jednego polecenia.
- Limit `--pending-bytes` obejmuje ramki przekazywane między strandami, kolejkę gniazda i trwający zapis. Odbiorcy rozgłoszenia współdzielą niezmienne dane ramki. Przekroczenie limitu zamyka połączenie odbiorcy. Początkowa lista musi się zmieścić w dostępnej kolejce, aby klient został przyjęty.
- Etap obejmuje sesje gości i czat prywatny między dwiema osobami. ADCS szyfruje połączenie z hubem. Czat grupowy, konta trwałe, wyszukiwanie i sygnalizacja P2P wykraczają poza E3.

## Test z dziesięcioma klientami

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug -R '^AdcPresence\.TenAdcsClientsTrackPresenceAndRouteChat$'
ctest --preset debug -R '^(UserPresence|AdcPresence)\.'
```

CTest generuje tymczasowe certyfikaty TLS i łączy dziesięciu niezależnych klientów z efemerycznym portem loopback. Test integracyjny sprawdza listę każdego klienta, czat publiczny, dostarczanie wiadomości prywatnych z kopią do nadawcy i bez niej, zmiany `INF`, reset TCP, pojedyncze `IQUI`, ponowne połączenie z nowym SID i pusty rejestr na końcu. Pozostałe testy E3 obejmują nieprawidłowe dane, równoczesne konflikty nicków i limity wyjścia. [Konfiguracja TCP i ADCS](TCP-ADCS.md)
