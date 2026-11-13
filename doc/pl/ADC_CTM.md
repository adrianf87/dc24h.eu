# ADC CTM — alpha-0.6.2

**Bieżący etap — alpha-0.6.6:** [Client Capabilities](CLIENT_CAPABILITIES.md) ujednolica własne migawki trybów TCP IPv4/IPv6 i deklaracji protokołów. Wspólny wybór jawnego CTM/RCM zachowuje polecenie klienta; osobny pomocnik inicjatora wybiera CTM lub RCM i odmawia dla dwóch pasywnych klientów. Sprawdzono zmiany INF, ponowną kontrolę starych planów i zmianę trybu rzeczywistego ncdc.

**Etap dostarczony w alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) dodaje do Transfer Core własne wartości protokołu i jawne rozróżnienie zwykłego ADC/1.0 oraz wymagającego TLS ADCS/1.0. Czysty wybór preferencji/możliwości nie obniża wymaganego TLS. CTM/RCM zachowują wybraną nazwę i zgodność rozszerzeń, a odpowiedzi błędów klientów zachowują tokeny. Rzeczywisty ncdc sprawdza TLS klientów z weryfikacją certyfikatu przez oba transporty huba; zachowano regresję pełnego terminala.

**Etap dostarczony w alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) łączy przyjmowanie i dostarczanie D/E CTM oraz RCM we wspólnym routerze SID, jednym typie niezmiennego pakietu i jednej ścieżce ograniczonych kolejek. Odbiorcami są ustalone okresy życia gotowych sesji: D tylko cel, E cel i niezależne echo źródła. Token nie wybiera odbiorców; zachowano role, walidację, błędy i prywatność. Testy równoległych par używają tego samego tokenu bez mieszania odbiorców.

**Etap dostarczony w alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Lokalna walidacja alpha-0.6.2 (2026-11-13):** Wszystkie **128/128 zestawów CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami i domyślnym wykrywaniem wycieków, w tym błędy/kolejki/czasy życia CTM, fragmentowane IPv4/IPv6 ADC/ADCS, rzeczywisty handshake ncdc CTM–CSUP/CINF/CGET, poprzednie Search/UDP, MariaDB i Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, języki/linki dokumentacji, składnia zmienionych plików Python, INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie pozostają niesprawdzone.

Etap powstał bezpośrednio z main `4513c4c6fda20269a78b2ad711ea16d204f412a9` po scaleniu Transfer Core. Zachowano kod/historię; gałąź funkcji jest publikowana oddzielnie od main.

## Działanie

Hub waliduje i przekazuje **DCTM** do gotowego celu oraz **ECTM** do celu, następnie nadawcy, przez [Transfer Core](TRANSFER_CORE.md). Obserwatorzy nie dostają ramki. RCM nadal jest jawnie niezaimplementowane. Poprawne CTM nie dostaje potwierdzenia sukcesu huba; przyjęcie do kolejki nie dowodzi połączenia i transferu pliku.

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html) definiuje CTM z pozycyjnymi polami protokołu, portu i tokenu, wysyłane przez aktywnego klienta TCP. D trafia do celu, E także do nadawcy. Nazwa protokołu używa widocznych znaków jednobajtowych. [ADCS klient-klient](https://adc.sourceforge.io/ADC-EXT.html) jest odrębne od TLS z hubem.

```text
DCTM ABCD EFGH ADC/1.0 1515 opaque-token
ECTM ABCD EFGH ADCS/1.0 01515 opaque\stoken XXfuture
```

Hub zachowuje sprawdzone pola/kolejność, zapis liczby, token i nieznane rozszerzenia przez kanoniczne escape ADC oraz jeden niezmienny współdzielony pakiet. Nie dopisuje tokenu i nie zmienia protokołu/portu. Klienci biorą adres z publicznego INF, otwierają własne połączenie i negocjują protokół. Hub nie otwiera portu CTM, nie pośredniczy w danych, nie wykonuje DNS/sondowania i nie czyta plików klientów.

## Typowany kodek

`adc/ctm.hpp` udostępnia własny CtmRequest, parse_ctm i validate_ctm. Pierwsza funkcja zachowuje ParseError dla błędów surowej składni; semantyka używa CtmError. Walidacja natywnego Message sprawdza nagłówki/układ, serializuje i ponownie parsuje wspólnym kodekiem, następnie sprawdza pola. Niespójny obiekt C++ nie omija reguł ramki.

| Dane | Reguła |
| --- | --- |
| Nagłówek/zakres | D lub E CTM, zgodna komenda/kind, poprawne SID źródła/celu, brak selektorów; domyślny lub jawny positional_count 3. |
| Protokół | Wymagane 1..32 bajty widocznego ASCII (33..126; DEL jest zakazany przez kodek). Poprawne nieznane nazwy są zachowane. |
| Port | Liczba całkowita int64, potem zakres 1..65535. Bez składni plusa; poprawne zera wiodące pozostają w ramce. |
| Token | Wymagane 1..128 bajtów po dekodowaniu, UTF-8 już w NFC, bez znaków sterujących/formatujących/separatorów wiersza i akapitu. Spacje/backslash są zachowane przez escape. |
| Rozszerzenia | Wspólna składnia pól nazwanych; nieznane/powtórzone pola zachowują kolejność i treść. Rozszerzenie TO nie zastępuje tokenu pozycyjnego. |
| Limity | Najwyżej 128 parametrów, 2048 bajtów zdekodowanych parametrów i dotychczasowy limit ramki 16384. Kontrola przed kopiowaniem. |

To limity lokalne, nie maksima ADC. Zaufany natywny Request Transfer Core pozostaje neutralny wobec protokołu; adapter ADC narzuca bardziej ścisłą nazwę z ramki. Nie dodano listy dozwolonych protokołów, downgrade TLS, twierdzenia o obsłudze przez klienta i rejestru tokenów. `ADC/1.0`, `ADCS/1.0` i poprawne nieznane nazwy są przekazywane tak samo; wsparcie, TLS i odpowiedź błędu ustalają klienci. Token CTM nie jest tokenem Search, sekretem ani autoryzacją połączenia w hubie.

## Dopuszczenie i routing

Dotychczasowe kontrole NORMAL/gotowości/własności źródła mają pierwszeństwo. CtmRouter waliduje żądanie, sprawdza użyteczną aktywną ścieżkę TCP nadawcy, rozwiązuje gotowy cel przez Transfer Core i odrzuca cel będący nadawcą. Wymagane są dokładne publiczne SU TCP4/TCP6 i użyteczny adres. UDP/porty UDP, SUP, deklaracje przywilejów i transport ADCS z hubem nie nadają aktywności TCP.

Transfer Core udostępnia bezalokacyjne `tcp_address(owner, family)` dla gotowego czasu życia. CtmRouter wybiera IPv4, gdy aktywne, w przeciwnym razie IPv6, do związania migawki dopuszczenia. Gdy nie ma żadnej, odmawia. To **wewnętrzna metadana kontroli**: CTM nie ma pola rodziny i hub nie narzuca adresu wybieranego przez odbiorcę z INF. Plan nie zmienia sam rodziny po przygotowaniu; nowe żądanie widzi aktualny INF.

`adc/ctm_routing.hpp` definiuje CtmRouter, RoutedCtm i niezmienny SharedCtmFrame. Plan zachowuje oba czasy życia, klucze i wybrany adres nadawcy. Kopie współdzielą jedną próbę Transfer Core, a token domeny routera blokuje także obcy router i ponowne użycie jego pamięci. Błąd domeny/brak sink nie zużywają próby. Pozostałe dispatch zużywają ją przed kontrolą gotowości/źródła/celu oraz aktywności/adresu. Rozłączenie, zamykanie, SID i zmiana adresu nie przekierują starego planu. Anulowanie jest jawne; dispatch nie alokuje po utworzeniu sink, także dla maksymalnych tekstów.

Najpierw wstawiany jest cel. Echo E jest osobną kopią, więc może zostać przyjęte mimo odmowy kolejki celu. Przed nim ponownie sprawdzane są życie/klucz/wybrany adres źródła; zamknięcie lub zmiana źródła w sink celu anuluje echo. Nieudane przygotowanie i niedostępny cel nie generują oryginalnego echa. Raport liczy przyjęte/odrzucone/anulowane kopie, bez potwierdzenia od klienta. Brak broadcastu/fallbacku.

LoginHub przechowuje współdzielony pakiet w dotychczasowym ograniczonym FIFO, z niezależnymi pozycjami częściowego wysyłania i pełnym logicznym naliczaniem bajtów. Dopisanie CTM nie przemieszcza przypiętego początku TCP/TLS. Błąd alokacji przygotowania izoluje źródło. Limit/alokacja kolejki celu izoluje ten cel; awaria echa dotyczy jego odbiorcy. Wyjątek zaufanego callbacku propaguje się po zużyciu próby, więc nie wolno ślepo powtarzać częściowej pracy.

## Błędy, prywatność i limity

| Warunek | Działanie |
| --- | --- |
| Źródło przed logowaniem/niegotowe | Dotychczasowe fatalne 244. |
| Podszycie pod SID | Dotychczasowe fatalne 240 przed CTM. |
| Błąd semantyczny, B/F lub cel do siebie | Odwracalne `100 Invalid connection request FC<FOURCC>`. |
| Brak aktywnej ścieżki TCP nadawcy | Odwracalne `100 CTM sender is not TCP active FC<FOURCC>`. |
| Brak/zamykanie celu | Odwracalne `140 CTM recipient unavailable FC<FOURCC>`. |
| RCM | Dotychczasowe odwracalne `125 Command not implemented`. |

Błędy surowej składni pozostają w kodeku transportu. Inne niepoprawne kierunki nagłówka zachowują odmowę maszyny stanów; poprawny obiekt typowany nie nadaje praw wysyłania. Diagnostyka nie odbija protokołu/tokenu/rozszerzeń. CTM nie trafia do liczników Chat/Search, treści logów, SQL, API i eksportu panelu; nadal obowiązują ogólne liczniki ramek/bajtów ADC. Brak historii transferów, ustawień, metryk, endpointu, zależności i migracji. Nadal działają limity ramek/IP/wyjścia; osobne prawa i antyflood transferów należą do późniejszego etapu.

## Testy i kolejne etapy

`adc.ctm` obejmuje roundtrip/własność, granice protokołu/portu/tokenu/bajtów/natywnego obiektu, zakresy, kolejność cel/echo i wspólny pakiet, kopie/anulowanie/replay/wyjątki, SID, zamknięcie źródła podczas wstawiania celu, zmiany adresu/rodziny, fault injection, dispatch bez alokacji, przypięte wyjście i izolację awarii.

`adc.ctm.process` sprawdza fragmentowane D/E IPv4/IPv6 ADC/ADCS, zmiany TCP/UDP w INF, nieznany/ADCS protokół, stałe błędy/powrót, obserwatora, reconnect celu, pierwszeństwo źródła/stanu i rozdział API/logów/Chat/Search. Starszy test granicy Transfer Core oczekuje teraz odmowy pasywnego CTM, nadal sprawdzając niegotowe RCM.

Rzeczywiste `adc.ctm.ncdc` / `adcs.ctm.ncdc` włączają aktywność TCP ncdc i żądają listy plików innego klienta. Odbiorca dostaje prawdziwe DCTM przez hub, korzysta z portu/tokenu w oddzielnym TCP do ncdc, kończy CSUP/CINF i widzi CGET. Czat nadal działa. To dowód koordynacji i handshake klientów; test nie twierdzi, że pobrano zawartość pliku lub wynegocjowano TLS klient-klient.

Kolejne etapy wymagają RCM, ograniczonej korelacji tokenów i timeoutów, praw/antyflood transferów, polityki protokołu/TLS i monitoringu. Hub nie serwuje GET/SND, nie wykonuje NAT i nie uwierzytelnia slotów/treści klientów. Wyniki są w [CHANGELOG](../../CHANGELOG.md); zdalne CI, pełna macierz systemów/klientów i obciążenie wymagają osobnego sprawdzenia.
