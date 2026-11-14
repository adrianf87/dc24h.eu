# Protocol Negotiation — alpha-0.6.5

**Bieżący etap — alpha-0.6.8:** [Connection Security](CONNECTION_SECURITY.md) dodaje startową politykę CTM/RCM/gości, zaufane nadania gotowych sesji i ponowną autoryzację przygotowania/wysyłki/echa. Wspólne kontrole IP i portu zachowują działanie zarządzanych profili. Testy natywne, mieszane ADC/ADCS i ncdc obejmują odmowy, podszywanie i dalszą komunikację.

**Etap dostarczony w alpha-0.6.7:** [Transfer Tokens](TRANSFER_TOKENS.md) ujednolica ograniczoną walidację tokenów i własne wartości w Core, CTM/RCM i błędach klientów. Unicode, skrajne spacje i ukośniki pozostają dokładne przy routingu i granicznych handshake ncdc. Niezależne żądania mogą powtarzać token; nie dodano rejestru ani domniemanej korelacji.

**Etap dostarczony w alpha-0.6.6:** [Client Capabilities](CLIENT_CAPABILITIES.md) ujednolica własne migawki trybów TCP IPv4/IPv6 i deklaracji protokołów. Wspólny wybór jawnego CTM/RCM zachowuje polecenie klienta; osobny pomocnik inicjatora wybiera CTM lub RCM i odmawia dla dwóch pasywnych klientów. Sprawdzono zmiany INF, ponowną kontrolę starych planów i zmianę trybu rzeczywistego ncdc.

**Lokalna walidacja alpha-0.6.5 (2026-11-13):** Wszystkie **141/141 testów CTest** przeszły osobno w Debug, Release i ASan/UBSan z wymaganymi narzędziami i domyślnym wykrywaniem wycieków. Sprawdzono wybór/własność/błędy protokołu, jawny routing nazw znanych/rozszerzeń, fragmentowane błędy klientów IPv4/IPv6 mieszanych ADC/ADCS, cztery zestawy rzeczywistego ncdc z TLS klientów i weryfikacją certyfikatu, wcześniejsze zwykłe połączenia i regresję terminala ncdc, Search/UDP, MariaDB i Apache HTTPS. Każdy z 11 aktywnych scenariuszy ncdc przeszedł również trzy kolejne uruchomienia ASan/UBSan (33 wyniki poprawne) z rezerwacją portów. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, języki/linki, składnia zmienionych plików Python, przykładowy INI i białe znaki. Pełna macierz wspieranych Debian/Ubuntu i obciążenie produkcyjne pozostają niesprawdzone; zdalne CI raportuje oddzielnie GitHub Actions.

Etap rozpoczyna się z main `9a041df4a65090580f5b009acdcd6758cadbf1dd`, zawierającego poprawkę CI alpha-0.6.4. Gałąź zachowuje historię i jest publikowana oddzielnie od main.

## Jawny protokół klientów

Transfer Core posiada teraz wartości `transfers::Protocol` wewnątrz Request. Zachowano tekstowy accessor `Request::protocol()`; `protocol_value()` udostępnia sprawdzoną niezmienną wartość. Adaptery CTM/RCM i współdzielone pakiety połączeń udostępniają tę samą klasyfikację bez alokacji. Kopiowanie lub przenoszenie żądania nie pozostawia pożyczonego zapisu protokołu w innym planie.

| Dokładny identyfikator | Rodzaj | Wymaganie szyfrowania |
| --- | --- | --- |
| `ADC/1.0` | `adc_1_0` | Zwykły TCP między klientami |
| `ADCS/1.0` | `adcs_1_0` | Wymagany TLS między klientami |
| Inny poprawny identyfikator | `extension` | Określane przez klientów; bez domniemania zwykłego TCP/TLS |

Dopasowanie jest dokładne i rozróżnia wielkość liter. Prefiksy, inne wersje, małe litery oraz starsze `ADCS/0.10`/`ADC0/0.10` pozostają rozszerzeniami. Rozszerzenie nie jest automatycznie zamieniane na zwykły TCP, TLS ani kanoniczną znaną nazwę. `protocol_name()` nie zwraca nazwy rozszerzenia lub niepoprawnego enuma; nieznane wymaganie szyfrowania oznacza `peer_defined`.

Zachowano kontrakt Core: tekst protokołu NFC o długości 1..32 bajtów. Kodeki ADC dodatkowo wymagają widocznego ASCII i zachowują oryginalne bajty, argumenty pozycyjne, token, zapis portu i rozszerzenia. Jawny protokół otrzymanego CTM/RCM decyduje o przekazaniu. Hub nie wybiera zastępstwa, nie tworzy odpowiedzi, nie ponawia żądania bez TLS i nie odrzuca poprawnego nieznanego rozszerzenia w imieniu klienta. Sesje, role rodzin adresów, ustaleni odbiorcy SID, zużycie próby, kolejki i kolejność błędów wynikają nadal z [Connection Routing](CONNECTION_ROUTING.md).

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html) przypisuje decyzję o nieobsługiwanym protokole odbiorcy i wymaga zachowania protokołu/tokena w odpowiedzi RCM. [Rozszerzenie ADCS](https://adc.sourceforge.io/ADC-EXT.html) określa TLS przed handshake ADC klientów oraz identyfikator `ADCS/1.0`. Te granice wyznaczają etap; hub koordynuje żądania i nie kończy TLS między klientami.

## Wybór dla zaufanego inicjatora

`advertised_protocols(Profile)` wyznacza bazowy ADC i dokładną deklarację `ADCS` w publicznym INF SU. Sam `ADC0` nie oznacza nowoczesnego ADCS/1.0. Aktywność TCP/UDP, transport ADC/ADCS do huba i funkcje SUP nie dowodzą TLS klientów. Deklaracje możliwości nie są sprawdzonym uwierzytelnieniem ani dowodem zaszyfrowania. Czysta funkcja profilu nie ustala gotowości sesji; przyjmowanie przez Core nadal ją sprawdza.

`negotiate_protocol(origin, peer, preference)` jest bezstanowym API bez alokacji dla zaufanego wywołującego tworzącego nowe żądanie. Uwzględnia oba przekazane zbiory możliwości:

| Preferencja | Wybór |
| --- | --- |
| `prefer_adc` | Wspólny ADC, w przeciwnym razie wspólny ADCS |
| `prefer_adcs` | Wspólny ADCS, w przeciwnym razie wspólny ADC |
| `require_adcs` | Wspólny ADCS albo `encryption_unavailable` |

Brak wspólnego protokołu daje `no_common_protocol`; niepoprawny enum preferencji daje `invalid_preference`. Zbiory możliwości mogą jawnie wyłączyć bazowy ADC. Jest to API inicjatora, a nie automatyczna polityka huba dla otrzymanych CTM/RCM. Routing zachowuje jawne żądania także po zmianie deklaracji; nie zastępuje wyboru klienta preferencją funkcji. Błąd handshake TLS lub certyfikatu należy do klientów i nie upoważnia huba do obniżenia szyfrowania.

## Odpowiedzi błędów klientów

Walidator STA traktuje teraz TO w błędach połączeń 41/42 jako ten sam nieinterpretowany token NFC co CTM/RCM (1..128 bajtów). Usuwa to odrzucanie poprawnych tokenów z escapowanymi spacjami, Unicode i ukośnikiem odwrotnym. PR w tych odpowiedziach korzysta z tej samej ograniczonej składni widocznego ASCII co żądania (1..32 bajty). Zachowano kontrolę powtórzeń, limity ramek/pól, granicę serializacji obiektów natywnych i usuwanie nieznanych pól. Inne kody statusu zachowują wcześniejsze reguły tokenów; etap nie dodaje obowiązkowych pól ani rejestru tokenów oczekujących.

Routing DSTA/ESTA pozostaje oddzielny, wymaga gotowego właściciela źródła i wskazuje jawny SID celu. Błędy nie są przypisywane do domniemanego żądania, CID, właściciela tokena ani zapisanej negocjacji. Osoby postronne nic nie otrzymują, a podszycie źródła nadal daje dotychczasowy fatalny błąd. Tylko wybrani odbiorcy widzą protokół/token; logi, SQL, API/WebAdmin i liczniki Chat/Search nie otrzymują treści lub historii połączeń.

## Weryfikacja i granice

`transfers.protocol` sprawdza dokładną klasyfikację, własność, limity/NFC, bezpieczeństwo rozszerzeń, kombinacje możliwości/preferencji, odmowę obniżenia wymaganego TLS, niepoprawne preferencje i wybór/dispatch bez alokacji. D/E CTM i RCM zachowują zamiar i odbiorców po zmianie danych wywołującego lub deklaracji. Natywne błędy klientów obejmują tokeny, limity, powtórzenia, znaki sterujące i usuwanie nieznanych pól.

`transfers.protocol.process` używa IPv4/IPv6 i mieszanych użytkowników ADC/ADCS, obu zakresów i komend, znanych/nieznanych/starszych identyfikatorów, zmiennych deklaracji, odpowiedzi CTM i błędów klientów 141/142. Sprawdza zachowanie bajtów, własność źródła, izolację odbiorców, działanie czatu i prywatność API/logów.

Cztery nowe zestawy rzeczywistego ncdc (`adc.ctm.adcs.ncdc`, `adcs.ctm.adcs.ncdc`, `adc.rcm.adcs.ncdc`, `adcs.rcm.adcs.ncdc`) używają dokładnego ADCS/1.0 oraz gniazd TLS 1.2/1.3 z weryfikacją na podstawie tymczasowego lokalnego certyfikatu ncdc. Drugi klient również podaje tymczasowy certyfikat. RCM obejmuje ncdc łączący się do nasłuchu TLS i ncdc odpowiadający jako aktywny nasłuch TLS; CTM dochodzi do CSUP/CINF/CGET przez TLS. Rzeczywista odmowa protokołu ncdc zachowuje token Unicode. Testy zwykłych połączeń klientów przez oba transporty huba i regresja pełnego terminala pozostają włączone.

Helper aktywacji ncdc rezerwuje numery TCP/UDP przydzielone przez system z SO_REUSEADDR do ogłoszenia gotowych endpointów klienta. W przeciwieństwie do systemowego bind(0), port 0 w ncdc wybiera losowy, potencjalnie zajęty numer. Nienasłuchująca rezerwacja TCP i tymczasowa rezerwacja UDP chronią numery przed równoległymi testami/gniazdami wychodzącymi podczas przekazania; obie są zwalniane przed ruchem klientów. Odpowiedzi CTM muszą podawać zarezerwowany port TCP.

Jawne lokalne zaufanie do certyfikatu w testach nie stanowi produkcyjnej implementacji KEYP. Projekcja/polityka przypinania KP klientów, produkcyjne zaufanie certyfikatów, korelacja/terminy tokenów oczekujących, polityka bezpieczeństwa transferów i przesył danych plików pozostają kolejnymi etapami. Nie dodano zależności, klucza konfiguracji, uprawnienia, migracji bazy, endpointu, metryki transferów ani strony WebAdmin. Zobacz [Kompilacja](BUILD.md), [Testy](TESTING.md), [Transfer Core](TRANSFER_CORE.md), [ADC CTM](ADC_CTM.md) i [ADC RCM](ADC_RCM.md).
