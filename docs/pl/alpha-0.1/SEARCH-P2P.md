# Wyszukiwanie ADC i sygnalizacja połączeń P2P

[English (en-US)](../../us/alpha-0.1/SEARCH-P2P.md)

E4 kieruje wyszukiwania plików, wyniki i żądania otwarcia połączenia P2P. ADC i ADCS mają wspólną listę użytkowników online. Strand serwera odczytuje możliwości klientów z aktualnych publicznych profili `INF` i przekazuje zweryfikowane ramki przez istniejące kolejki wyjściowe. Handshake między klientami i zawartość plików przechodzą przez ich bezpośrednie połączenie.

## Wyszukiwanie i wyniki

Zalogowany klient w stanie `NORMAL` może wysyłać `SCH`. `BSCH` otrzymują wszyscy zalogowani, także nadawca. `FSCH` otrzymują klienci, których `SU` zawiera wszystkie rozszerzenia oznaczone `+` i żadne oznaczone `-`. `DSCH` trafia do adresata, a `ESCH` dodatkowo wraca do nadawcy.

Hub sprawdza terminy zawierające (`AN`), wykluczające (`NO`), rozszerzenia (`EX`), granice rozmiaru (`GE`, `LE`, `EQ`), typ (`TY`) i skrót TIGR (`TR`). Terminy tekstowe można powtarzać. Opcjonalne `TO` identyfikuje wyszukiwanie i jest kopiowane do jego wyników. Pola o poprawnej składni mogą towarzyszyć obsługiwanym filtrom, także gdy hub ich nie rozpoznaje.

Wyniki przez hub przechodzą jako `DRES` lub `ERES`. Wynik zawiera `FN`, `SI` i w przypadku pliku `TR`. Opcjonalne `SL` podaje liczbę wolnych slotów. Hub przyjmuje wyniki dla konkretnego szukającego od użytkowników online, którym zakolejkował to wyszukiwanie. `DRES` trafia do szukającego, a `ERES` dodatkowo wraca do nadawcy. Szukający, który pominął `TO`, może otrzymać wynik bez `TO` w tym samym ograniczonym oknie. Klienci z ogłoszonym adresem i portem UDP mogą też przekazywać sobie `URES` bezpośrednio. [ADC 1.0.4, rozdz. 3.3 i 5.3.6–5.3.7](https://adc.sourceforge.io/ADC.html) · [Rozszerzenia ADC, TIGR](https://adc.sourceforge.io/ADC-EXT.html)

Klient `AAAB` kieruje wyszukiwanie do użytkowników aktywnych po TCP w IPv4 i dostaje odpowiedź od `AAAC`.

```text
FSCH AAAB +TCP4 ANrelease ANnotes TOquery-1
DRES AAAC AAAB FN/Docs/release\snotes.txt SI37 SL1 TRW6AIUW3CLDF6OGHNVE4JPDDJ2P74IWRCF2O36TA TOquery-1
```

## Klienci aktywni i pasywni

Na potrzeby routingu klient jest aktywny po TCP, jeśli zgłasza `TCP4` z adresem `I4` lub `TCP6` z adresem `I6` pochodzącym z gniazda huba. Klient aktywny może wysłać `DCTM <from> <to> <protocol> <port> <token>`. Drugi klient łączy się z ogłoszonym adresem i portem. Klient pasywny wysyła `DRCM <from> <to> <protocol> <token>` do aktywnego. Ten odpowiada przez `DCTM` z tym samym protokołem i tokenem. `ECTM` i `ERCM` wysyłają też kopię do nadawcy. Hub przyjmuje żądania zwrotne kierowane do aktywnego klienta, także jako połączenia zapasowe między użytkownikami aktywnymi.

```text
DRCM AAAB AAAC ADC/1.0 connect-1
DCTM AAAC AAAB ADC/1.0 41300 connect-1
```

Klient `AAAB` otwiera połączenie TCP do `AAAC` na porcie `41300` i podaje `TOconnect-1` w swoim bezpośrednim `CINF`. Klienci zarządzają tokenem i sprawdzają nim oczekiwane połączenie. `ADC/1.0` oznacza zwykłe TCP. `ADCS/1.0` i starsze `ADCS/0.10` oznaczają TLS, jeśli obie strony zgłaszają zgodne wsparcie. Hub publikuje poprawne składniowo odciski SHA-256 z pola `KP`, dzięki czemu klienci mogą sprawdzić certyfikat drugiej strony. Tryb TLS połączenia z hubem i tryb TLS połączenia P2P są niezależne. [ADC 1.0.4, rozdz. 5.2.2 i 5.3.8–5.3.9](https://adc.sourceforge.io/ADC.html) · [Rozszerzenia ADC, KEYP i ADCS](https://adc.sourceforge.io/ADC-EXT.html)

## Ograniczenia Alpha 0.1

- SID nadawcy jest przypisany do gniazda. Hub odrzuca błędne porty i tokeny, niedostępnych adresatów i wyniki od klientów spoza danego wyszukiwania. Tokeny mieszczą się w 256 zdekodowanych bajtach i nie zawierają znaków sterujących ASCII. Błędy routingu trafiają wyłącznie do nadawcy.
- Przekazanie wyszukiwania pozwala danemu odbiorcy odesłać do pięciu wyników dla klienta pasywnego w UDP lub do dziesięciu dla klienta aktywnego w UDP. Uprawnienie trwa 60 sekund dla każdego odbiorcy. Wyszukiwania z tym samym tokenem lub bez niego współdzielą ograniczoną pulę wyników. Klient może korzystać z 16 równoczesnych pozycji tokenów, wliczając pozycję bez tokenu.
- Hub śledzi żądanie zwrotne przez 60 sekund. Dopóki oczekuje na odpowiedź dla danej pary, `CTM` musi zgadzać się z protokołem i tokenem żądania. Bez oczekującego żądania aktywny użytkownik może wysłać samodzielne `CTM`.
- Wyszukiwanie i sygnalizacja zaczynają od ośmiu żądań na klienta. Budżet wyszukiwań odnawia się o jedno żądanie na sekundę, a sygnalizacji o cztery. Wyniki mają osobny limit częstotliwości. Przekazywanie korzysta z dotychczasowych limitów wyjścia. Rozłączenie usuwa powiązany stan, więc następna sesja z tym samym SID go nie odziedziczy.
- Dopasowywanie plików, bezpośrednie UDP, zestawianie połączeń i przesyłanie plików należą do klientów. E4 kieruje komunikaty dla protokołów opisanych powyżej. Rozszerzenia przechodzenia przez NAT i pośrednictwo w transferze wykraczają poza ten etap. Dwaj pasywni klienci potrzebują aktywnego punktu końcowego, aby zestawić bezpośrednie połączenie.

## Weryfikacja lokalna

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug -R '^(AdcProtocol|PeerSignaling|AdcPeerRouting)\.'
ctest --preset debug
```

Testy integracyjne używają efemerycznych portów loopback i tymczasowych certyfikatów TLS. Sprawdzają wyszukiwanie i żądanie zwrotne między ADC i ADCS, a potem połączenie klientów na osobnym gnieździe. Sprawdzają również izolację przy próbie podszycia się, błędnym tokenie i rozłączeniu.

Aby powtórzyć sprawdzenie na klientach, uruchom hub na loopback z `--enable-adc --adc-port 0` i otwórz dwa niezależne profile ncdc z różnymi nickami i katalogami pobierania. W obu profilach udostępnij mały lokalny plik testowy. W jednym wykonaj `/set active_ip 127.0.0.1`, `/set active_port 0` i `/set active true`. Drugi pozostaw pasywny. Do próby ADC ustaw `/set tls_policy disabled` w obu profilach. Połącz oba profile z hubem, w każdym wpisz `/search <nazwa>` i na wyniku naciśnij `d`. Porównaj pobrane bajty z plikiem udostępnionym w drugim profilu. Powtórz przez ADCS z `/set tls_policy force` w obu profilach, aby sprawdzić TLS między klientami. [Konfiguracja huba na loopback](TCP-ADCS.md)
