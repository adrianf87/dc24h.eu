# Client Capabilities — alpha-0.6.6

**Bieżący etap — alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) dodaje startową politykę CTM/RCM/gości, zaufane nadania gotowych sesji i ponowną autoryzację przygotowania/wysyłki/echa. Wspólne kontrole IP i portu zachowują działanie zarządzanych profili. Testy natywne, mieszane ADC/ADCS i ncdc obejmują odmowy, podszywanie i dalszą komunikację.

**Etap dostarczony w alpha-0.6.7:** [Transfer Tokens](TRANSFER_TOKENS.md) ujednolica ograniczoną walidację tokenów i własne wartości w Core, CTM/RCM i błędach klientów. Unicode, skrajne spacje i ukośniki pozostają dokładne przy routingu i granicznych handshake ncdc. Niezależne żądania mogą powtarzać token; nie dodano rejestru ani domniemanej korelacji.

**Weryfikacja lokalna alpha-0.6.6 (2026-11-13):** Przeszły wszystkie 147/147 testów CTest w Debug, Release i ASan/UBSan, w tym zmiany trybu rzeczywistego ncdc i handshake klientów ze zwykłym TCP/TLS. Poprawne są build/typecheck WebAdmin, 12 testów Node, 10 testów Chromium i integracja z rzeczywistym API C++; brak ostrzeżeń kompilatora. Przeszły kontrole metadanych, UTF-8, zgodności języków/linków, składni zmienionych plików Python, przykładowego INI i białych znaków. Nie oczekiwano na zakończenie GitHub Actions. Pełna macierz wspieranych systemów i obciążenie produkcyjne pozostają niesprawdzone.

Etap utworzony bezpośrednio z main `1aacfbd` po alpha-0.6.5. Ujednolica publiczne możliwości TCP, role klientów według rodziny adresów i wybór ścieżki przez inicjatora. Zachowano kod i historię; gałąź numerowana jest publikowana osobno od main.

## Deklaracje i migawki

`transfers/capabilities.hpp` udostępnia własną migawkę `ClientCapabilities` o stałym rozmiarze: opcjonalne bajty adresów nasłuchu IPv4/IPv6 i deklarowane `Protocols`. `client_capabilities(Profile)` korzysta ze sprawdzonego publicznego profilu INF, bez alokacji, I/O, DNS, sondowania i zachowywania referencji do profilu. `Core::capabilities(SessionId)` zwraca migawkę tylko dla gotowej sesji z publicznym użytkownikiem. Nieistniejąca, zamykana lub oczekująca na powitanie sesja nie ma migawki.

Aktywność TCP wymaga dokładnego publicznego SU TCP4/TCP6 i użytecznego adresu liczbowego tej rodziny. Brak adresu, adres nieokreślony, multicast, broadcast IPv4, mapowany IPv6 i lokalny IPv6 wymagający zakresu nie dają nasłuchu. Adresy prywatne i loopback są dozwolone. Migawka opisuje deklarację, nie potwierdza osiągalności ani własności adresu. Adapter INF osobno sprawdza własność i zastępuje I40.0.0.0 / I6:: adresem połączonego klienta.

`mode(family)` zwraca `active`, gdy istnieje nasłuch, w przeciwnym razie zachowawczo `passive`. Klient może być aktywny w jednej rodzinie i pasywny w drugiej. Nieprawidłowa natywna rodzina nie ma nasłuchu. Cechy i porty UDP, historyczne ADC0, SUP i TLS huba nie nadają aktywności TCP. Dokładne publiczne SU ADCS deklaruje TLS klientów niezależnie od trybu TCP; wybór protokołu i wymagany TLS zachowują zasady alpha-0.6.5.

## Reguły koordynacji

Dla jednej rodziny adresów:

| Źródło | Cel | CTM | RCM |
| --- | --- | --- | --- |
| Aktywne | Aktywne | Dozwolone | Odrzucone |
| Aktywne | Pasywne | Dozwolone | Odrzucone |
| Pasywne | Aktywne | Odrzucone | Dozwolone |
| Pasywne | Pasywne | Odrzucone | Odrzucone |

`connection_address(origin, peer, kind, family)` implementuje tę regułę we wspólnym miejscu. Zwracane bajty opisują nasłuch źródła CTM albo celu RCM. Nieprawidłowy rodzaj/rodzina nie daje ścieżki.

`select_connection(origin, peer, kind)` zachowuje jawne polecenie i wybiera pierwszą dostępną rodzinę: IPv4, potem IPv6. Osobny pomocnik `select_connection(origin, peer)` dla zaufanego inicjatora preferuje CTM w dowolnej aktywnej rodzinie źródła, potem RCM w rodzinie pasywnego źródła i aktywnego celu. Dla dwóch pasywnych klientów brak wyboru. Nie generuje tokena, nie wybiera portu, nie zmienia protokołu, nie kolejkuje ramki i nie otwiera gniazda. Inicjator może osobno użyć `negotiate_protocol` z obiema migawkami i jawną preferencją; wymagane ADCS nie przechodzi po cichu na zwykły ADC.

Connection Router używa pomocnika zachowującego jawne polecenie, a Transfer Core tej samej reguły adresu przy przygotowaniu i wysyłce. CTM zachowuje kolejność błędów i kontroli aktywności źródła; RCM rozwiązuje dokładny gotowy cel przed sprawdzeniem ról. Przychodzące CTM nie zmienia się w RCM. CTM/RCM nie mają pola rodziny adresów: wybór rodziny/adresu to metadane dopuszczenia, nie potwierdzenie zgodnej wychodzącej trasy sieciowej celu. Klienci wybierają gniazda i zgłaszają błędy. Brak adresu celu nie staje się nową regułą uprawnień CTM.

## Zmiany INF i życie sesji

Przyjęte zmiany INF SU i I4/I6 transakcyjnie aktualizują profil. Nowe migawki i żądania używają aktualnego stanu; zachowane migawki mają własne bajty. Usunięcie wybranego nasłuchu, zmiana adresu lub przejście źródła RCM na aktywne anuluje stary plan przed wywołaniem odbiorcy. Inna dostępna rodzina nie ratuje starego planu. Nowe żądanie może wybrać pozostałą rodzinę. Zmiana niepowiązanej rodziny, UDP lub deklaracji TLS nie anuluje nadal poprawnej ścieżki ani nie zmienia jawnego protokołu klientów.

Pozostają jedna próba wysyłki, własność źródła, wiązanie z życiem gotowej sesji, ponowna kontrola echa i stała publiczność D/E. D trafia tylko do celu, E niezależnie także do źródła. Treść i tokeny nie trafiają do SQL, API/WebAdmin ani liczników Chat/Search. Nie dodano rejestru możliwości ani historii transferów. Korelacja tokenów, NAT traversal, relay między pasywnymi klientami, gniazda klientów i bajty plików pozostają przyszłymi pracami.

## Weryfikacja

`transfers.capabilities` sprawdza wszystkie 32×32 kombinacje TCP4/TCP6/UDP4/UDP6/ADCS, role CTM/RCM według rodziny, kolejność wyboru, nieprawidłowe enumy, nieużyteczne adresy, własność migawek, brak alokacji przy wyborze/wysyłce, gotowość i stare/nowe plany po zmianach.

`transfers.capabilities.process` obejmuje obie rodziny adresów, mieszane ADC/ADCS, oba kierunki/zakresy/polecenia, fragmentowane nagłówki, wszystkie pary ról, usuwanie/przywracanie adresu, anulowanie SU, deklarację innej rodziny, izolację obserwatora, dalszą komunikację i prywatność API/logów.

Cztery scenariusze rzeczywistego ncdc używają obu transportów huba i zwykłego/TLS handshake klientów. ncdc zmienia tryb pasywny→aktywny→pasywny→aktywny bez ponownego połączenia; RCM między pasywnymi dostaje prywatną odwracalną odmowę, a przywrócenie aktywności pozwala na RCM→CTM z zachowanym tokenem/protokołem i rzeczywistym CSUP/CINF. Lokalne testy TLS jawnie ufają jednorazowym certyfikatom. Nie jest to produkcyjny pinning.

Zobacz [Testy](TESTING.md), [Protocol Negotiation](PROTOCOL_NEGOTIATION.md), [Connection Routing](CONNECTION_ROUTING.md), [specyfikację ADC](https://adc.sourceforge.io/ADC.html), rozdziały 5.2.2 i 5.3.8–5.3.9, oraz [rozszerzenia ADC](https://adc.sourceforge.io/ADC-EXT.html).
