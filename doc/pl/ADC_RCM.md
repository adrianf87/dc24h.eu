# ADC RCM — alpha-0.6.3

**Bieżący etap — alpha-0.6.6:** [Client Capabilities](CLIENT_CAPABILITIES.md) ujednolica własne migawki trybów TCP IPv4/IPv6 i deklaracji protokołów. Wspólny wybór jawnego CTM/RCM zachowuje polecenie klienta; osobny pomocnik inicjatora wybiera CTM lub RCM i odmawia dla dwóch pasywnych klientów. Sprawdzono zmiany INF, ponowną kontrolę starych planów i zmianę trybu rzeczywistego ncdc.

**Etap dostarczony w alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) dodaje do Transfer Core własne wartości protokołu i jawne rozróżnienie zwykłego ADC/1.0 oraz wymagającego TLS ADCS/1.0. Czysty wybór preferencji/możliwości nie obniża wymaganego TLS. CTM/RCM zachowują wybraną nazwę i zgodność rozszerzeń, a odpowiedzi błędów klientów zachowują tokeny. Rzeczywisty ncdc sprawdza TLS klientów z weryfikacją certyfikatu przez oba transporty huba; zachowano regresję pełnego terminala.

**Etap dostarczony w alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) łączy przyjmowanie i dostarczanie D/E CTM oraz RCM we wspólnym routerze SID, jednym typie niezmiennego pakietu i jednej ścieżce ograniczonych kolejek. Odbiorcami są ustalone okresy życia gotowych sesji: D tylko cel, E cel i niezależne echo źródła. Token nie wybiera odbiorców; zachowano role, walidację, błędy i prywatność. Testy równoległych par używają tego samego tokenu bez mieszania odbiorców.

**Lokalna walidacja alpha-0.6.3 (2026-11-13):** Wszystkie **132/132 zestawy CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami i domyślnym wykrywaniem wycieków, w tym błędy/kolejki/czasy życia RCM, fragmentowane IPv4/IPv6 ADC/ADCS, rzeczywisty ncdc RCM→CTM i handshake obu ról, wcześniejsze CTM/Search/UDP, MariaDB i Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, języki/linki, składnia zmienionych plików Python, INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie pozostają niesprawdzone.

Etap powstał bezpośrednio z main `4f86b20c407047ae89c537e2c77b7dad7d833633` po scaleniu ADC CTM. Zachowano kod/historię; gałąź jest publikowana oddzielnie od main.

## Działanie

Hub obsługuje sprawdzone **DRCM** do celu oraz **ERCM** do celu, następnie nadawcy. Korzysta z [Transfer Core](TRANSFER_CORE.md), [CTM](ADC_CTM.md) i dotychczasowych ograniczonych kolejek. Nie ma broadcastu, ramki dla obserwatora i potwierdzenia sukcesu huba.

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html) definiuje RCM z pozycyjnym protokołem/tokenem jako żądanie pasywnego klienta do aktywnego. Obsługujący klient odpowiada CTM, zachowując protokół/token. D wskazuje cel, E także nadawcę. Hub nie tworzy tej odpowiedzi i nie wybiera portu klienta.

```text
DRCM ABCD EFGH ADC/1.0 opaque-token
ERCM ABCD EFGH ADCS/1.0 opaque\stoken XXfuture
```

Aktywny odbiorca sam podaje port w CTM, które hub sprawdza i przekazuje dotychczasowym adapterem. Sockety klientów, CSUP/CINF, ewentualne TLS i komendy plikowe pozostają poza ścieżką danych huba. [ADCS pomiędzy klientami](https://adc.sourceforge.io/ADC-EXT.html) jest niezależne od transportu z hubem. Nie dodano downgrade, automatycznej negocjacji protokołu i potwierdzenia osiągalności.

## Typowany kodek i wspólne granice pól

`adc/rcm.hpp` zawiera własny RcmRequest, RcmError, parse_rcm i validate_rcm. Błędy surowej składni pozostają ParseError; błędy semantyczne RCM używają RcmError. Żądanie posiada sprawdzony oryginalny Message i dostęp do protokołu/tokenu. Nie ma portu, wymyślonej wartości ani automatycznej konwersji na CTM.

| Dane | Reguła |
| --- | --- |
| Nagłówek | D/E RCM, zgodna komenda/kind, poprawne SID, brak selektorów, domyślny lub jawny positional_count 2. |
| Protokół | Wymagane 1..32 bajty widocznego ASCII 33..126; poprawne nieznane nazwy pozostają. |
| Token | Wymagane 1..128 bajtów po dekodowaniu, UTF-8 już w NFC; bez sterujących/formatujących/separatorów wiersza i akapitu. Spacje/backslash przechodzą przez escape. |
| Rozszerzenia | Pola nazwane po dwóch pozycyjnych. Nieznane/powtórzone pola zachowują kolejność/treść. TO lub rozszerzenie podobne do portu nie zastępuje tokenu i nie tworzy portu. |
| Limity | 128 parametrów, 2048 bajtów zdekodowanych parametrów i dotychczasowy limit ramki 16384. |

Prywatny `src/adc/connection_fields.hpp` skupia wspólne reguły CTM/RCM: nagłówek D/E, liczby parametrów/bajtów, nazwę protokołu i token. CTM zachowuje osobną walidację portu oraz trzy pola pozycyjne; jego publiczne API/działanie nie zmienia się. Natywny Message przechodzi serializację/ponowne parsowanie przed skopiowaniem, aby błędny tekst/układ/rozszerzenia nie ominęły składni. Limity bajtów są lokalne, nie ogólne dla ADC. Błąd alokacji nie zmienia modeli i propaguje się do adaptera.

## Role i wybór rodziny

Dotychczasowe kontrole NORMAL/gotowości/SID źródła poprzedzają RCM. RcmRouter waliduje żądanie, wyszukuje gotowy cel i odrzuca połączenie do siebie. Nowe `resolve_target(key)` Transfer Core zwraca wyłącznie gotowy identyfikator życia, bez alokacji i kopiowania profilu.

Plan odwrotny wymaga rodziny, w której **źródło jest pasywne, a cel aktywny TCP**. Aktywność oznacza dokładne publiczne TCP4/TCP6 i użyteczny adres według dotychczasowej polityki Users. UDP/porty UDP nie nadają aktywności TCP. Transporty z hubem mogą być różne.

RcmRouter preferuje zgodne IPv4, potem IPv6; bez takiej rodziny odmawia. Aktywność jest osobna dla rodzin: źródło aktywne TCP4 może użyć pasywnego TCP6 z aktywnym celem TCP6. Całkowicie aktywne źródło/brak zgodnej rodziny oraz układ pasywny-pasywny są odrzucane. Wybór wiąże wewnętrzne metadane kontroli; RCM nie ma pola rodziny i hub nie narzuca adresu oraz nie wywodzi możliwości wychodzących.

Niezmienny plan Transfer Core zachowuje obie sesje/klucze, żądanie odwrotne i wybrany adres aktywnego celu. **Nie ma portu.** Zmiana adresu/rodziny nie zastępuje danych w starym planie; nowe żądanie widzi aktualny INF.

## Dispatch, kolejki i błędy

`adc/rcm_routing.hpp` udostępnia RcmRouter, RoutedRcm, niezmienny SharedRcmFrame i raport kopii przyjętych/odrzuconych/anulowanych. Domeny routera/Core odrzucają obcy plan, również przy ponownym użyciu pamięci. Kopie współdzielą jedną próbę. Obca domena/brak sink nie zużywają jej; pozostałe próby zużywają przed kontrolą gotowości/kluczy, pasywności źródła, aktywności celu i jego adresu. Rozłączenie, zamykanie i ponowne użycie SID nie przekierują starego żądania. Jawne anulowanie dotyczy tylko własnego przygotowanego planu.

D wywołuje sink celu raz. E robi to najpierw, potem niezależnie rozważa echo do źródła. Przed echem sprawdzane są życie/klucz i pasywność wybranej rodziny źródła; jego zamknięcie lub aktywacja w callbacku celu anuluje echo. Odmowa kolejki celu może współistnieć z udanym echem; echo nie potwierdza dostarczenia. Nieudane przygotowanie, niedostępny cel i błąd kontroli przed dispatch nie wysyłają oryginalnego echa. Wyjątek callbacku zużywa próbę także po częściowym przyjęciu pracy; nie wolno ślepo ponawiać.

Jeden pakiet jest współdzielony przez wybrane kolejki z osobnymi pozycjami wysyłania i pełnymi opłatami logicznych bajtów. Zachowano FIFO, częściowe wysyłanie, przypięte początki TLS i limity klienta/całości. Dispatch/lookup/cancel nie alokują po utworzeniu sink, także przy maksymalnym protokole/tokenie. Błąd przygotowania izoluje nadawcę, limit/alokacja kolejki celu ten cel; awaria echa dotyczy jego odbiorcy. Brak publicznego fallbacku.

## Diagnostyka i prywatność

| Warunek | Działanie |
| --- | --- |
| Źródło przed logowaniem/niegotowe | Dotychczasowe fatalne 244. |
| Podszycie pod SID | Dotychczasowe fatalne 240 przed RCM. |
| Błąd semantyczny, B/F RCM lub cel do siebie | Odwracalne `100 Invalid connection request FC<FOURCC>`. |
| Brak zgodnej rodziny pasywnego źródła/aktywnego celu | Odwracalne `100 RCM requires passive sender and active recipient FC<FOURCC>`. |
| Brak/zamykanie celu | Odwracalne `140 RCM recipient unavailable FC<FOURCC>`. |

Surowa składnia pozostaje w kodeku transportu, a inne błędne kierunki w maszynie stanów. Diagnostyka nie odbija protokołu/tokenu/rozszerzeń. RCM/CTM nie trafia do liczników Chat/Search, treści logów, SQL, API i eksportu panelu. Ogólne liczniki ramek/bajtów ADC nadal je obejmują. Brak metryk/historii/strony transferów, klucza INI, zależności, endpointu i migracji. Pozostają limity transportu/IP/ramek/wyjścia; nie dodano osobnych uprawnień/antyflood transferów.

Ten etap **nie dodaje rejestru oczekującego RCM–CTM**. Hub zachowuje sprawdzone dane każdej ramki; klienci tworzą/kopiują i weryfikują tokeny. Hub nie uwierzytelnia odpowiadającego i nie wymusza zgodności każdego CTM z wcześniejszym RCM, nie deduplikuje prób, nie wygasza negocjacji, nie waliduje plików i nie serwuje GET/SND. To oddzielne przyszłe moduły/polityki.

## Testy

`adc.rcm` sprawdza granice surowe/natywne/semantyczne/tekst/bajty, roundtrip/własność, brak portów, role rodzin i niezależność UDP, wspólny pakiet/kolejność celu/echa, kopie/replay/cancel, wyjątek po częściowym echu, aktywację/zamknięcie źródła, SID/adres/rodzinę celu, alokacje, dispatch/lookup bez alokacji oraz przypięte kolejki/izolację awarii. Dotychczasowe testy CTM chronią refaktoryzację wspólnej walidacji.

`adc.rcm.process` obejmuje fragmentowane IPv4/IPv6 ADC/ADCS D/E, odpowiedzi CTM klienta z identycznym protokołem/tokenem, zmiany INF, stałe błędy/powrót, reconnect, obserwatorów, pierwszeństwo źródła/stanu i rozdział API/logów/Chat/Search. Starsze regresje Transfer Core/CTM oczekują teraz odmowy niezgodnych ról zamiast niezaimplementowanego RCM. Test granicy ncdc ustawia oba klienty pasywnie i potwierdza diagnostykę oraz dalszy czat.

Rzeczywiste `adc.rcm.ncdc` / `adcs.rcm.ncdc` sprawdzają obie role. Pasywny ncdc wysyła DRCM; aktywny peer odpowiada DCTM z dokładnym protokołem/tokenem, przyjmuje oddzielne TCP ncdc i sprawdza CSUP/CINF z tokenem oraz CGET. Następnie aktywny ncdc odbiera DRCM pasywnego peera, wysyła zgodne DCTM i przyjmuje handshake z tokenem. Harness dopuszcza ograniczone informacyjne CSTA referrera przed CINF. Czat nadal działa. Testy potwierdzają koordynację/handshake, nie udane pobranie treści pliku ani certyfikację TLS klient-klient.

Wyniki są w [CHANGELOG](../../CHANGELOG.md). Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne wymagają osobnej weryfikacji.
