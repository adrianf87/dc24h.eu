# Konfiguracja transferów — alpha-0.6.13

**Lokalna weryfikacja alpha-0.6.13 (2026-11-14):** Pełne lokalne przebiegi przeszły **179/179 zestawów CTest w każdym z trybów Debug i Release** na Debianie 13 z wymaganymi integracjami, w tym cztery nowe scenariusze konfiguracji/negocjacji klientów ncdc oraz wcześniejsze ADC/ADCS, MariaDB i Apache. Obie kompilacje bez ostrzeżeń. Poprawne build/typecheck WebAdmin, **23 testy Node**, **14 testów Chromium** i rzeczywista integracja C++ API/React niestandardowych ustawień oraz ruchu. Sprawdzono mobilny PL, przykładowe INI, składnię zmienionego Pythona, metadane, UTF-8/parytet języków/linki lokalne i białe znaki. Workflow GitHub wyłącznie na Ubuntu bez zmian. Pozostałe systemy, sanitizery tego etapu i obciążenie produkcyjne pozostają niezweryfikowane; bez oczekiwania na zdalny Check status.

Etap oparty bezpośrednio na main `cc48fbd90d8f023bf1295f726a34ce9e7e158426` po scaleniu alpha-0.6.12. Uzupełnia konfigurację startową limitów koordynacji połączeń i dozwolonych protokołów klientów. Zachowuje workflow GitHub wyłącznie na Ubuntu oraz wcześniejsze etapy.

## Polityka startowa

Sekcja `[transfers]` przyjmuje teraz również `allowed_protocols`. Wszystkie ustawienia są odczytywane przy starcie; edycja INI nie zmienia działającego huba. Sprawdź plik przez `dc24h --config /pełna/ścieżka/hub.ini --check-config`, bez uruchamiania nasłuchu i odczytu haseł bazy, a następnie zastosuj zmiany przez świadomy restart. Nie ma przeładowania konfiguracji, edytora w przeglądarce ani zapisu przez API.

```ini
[transfers]
allowed_protocols = ADC/1.0,ADCS/1.0
ctm_enabled = on
rcm_enabled = on
guest_initiate = on
guest_receive = on
request_rate = 32
request_burst = 64
request_byte_rate = 131072
request_byte_burst = 262144
incoming_rate = 128
incoming_burst = 256
incoming_byte_rate = 524288
incoming_byte_burst = 1048576
```

| Ustawienie | Domyślnie | Znaczenie / zakres |
| --- | --- | --- |
| `allowed_protocols` | `any` | `any`, `none` lub 1–16 dokładnych nazw oddzielonych przecinkami |
| `ctm_enabled`, `rcm_enabled` | `on` | Niezależne przełączniki komend, wyłącznie `on` / `off` |
| `guest_initiate`, `guest_receive` | `on` | Prawo inicjowania przez źródło / odbierania przez cel |
| `request_rate`, `request_burst` | 32, 64 | Próby źródła na sekundę / pojemność liczby prób; 1–1000000 |
| `request_byte_rate`, `request_byte_burst` | 131072, 262144 | Zdekodowane bajty źródła na sekundę / pojemność bajtów |
| `incoming_rate`, `incoming_burst` | 128, 256 | Łączne próby do celu na sekundę / pojemność liczby prób; 1–1000000 |
| `incoming_byte_rate`, `incoming_byte_burst` | 524288, 1048576 | Łączne zdekodowane bajty do celu na sekundę / pojemność bajtów |

Tempo bajtów przyjmuje 1–16777216, pojemność bajtów 16384–16777216. Tempo i pojemność są niezależne; pojemność może być mniejsza od tempa. Liczba prób i bajty są obciążane razem, wspólne dla CTM/RCM i D/E w obrębie czasu życia połączenia, odnawiane monotonicznie i nie zerują się przez zmianę protokołu/uprawnień/INF. Echo E nie obciąża drugi raz budżetu wejściowego. Są to limity komunikatów koordynacji, nie przepustowości plików ani liczby równoczesnych transferów. Pozostają zasady ograniczania powiadomień i [limitów transferów](TRANSFER_LIMITS.md).

`any` zachowuje zgodność z rozszerzeniami i nie pomija walidacji, uprawnień ani limitów. `none` odrzuca każde CTM/RCM, które przeszło wcześniejsze kontrole. Lista stosuje dokładne bajty z rozróżnianiem wielkości liter, zachowuje kolejność i usuwa wyłącznie spacje/tabulatory ASCII wokół elementów. Nazwa spełnia dotychczasową granicę CTM/RCM: 1–32 widoczne bajty ASCII. Przecinki rozdzielają nazwy; małe `any` / `none` są zarezerwowanymi wartościami całego pola i nie mogą występować w liście. Puste elementy, duplikaty, zbyt długie listy/nazwy i niepoprawny tekst zatrzymują start. Brak rozwijania wzorców: `*` jest dosłowną nazwą rozszerzenia. `ADC/1.0` różni się od `adc/1.0` i `ADC/1.00`.

`allowed_protocols = ADCS/1.0` dopuszcza wyłącznie nazwę protokołu TLS klientów. Transport ADC/ADCS huba, SU ADCS klienta i tryb aktywny/pasywny pozostają niezależne. Hub zachowuje jawne pola protokołu/portu/tokena i nie zamienia ADCS na ADC. Dopuszczenie nazwy nie potwierdza jej obsługi przez klienta ani negocjacji TLS. Tokeny pozostają nieinterpretowane; bez rejestru oczekujących transferów i obsługi plików.

## Egzekwowanie i statusy

`transfers::ProtocolPolicy` posiada ograniczoną listę tworzoną sprawdzaną fabryką oraz wyszukiwanie dokładnej nazwy bez alokacji. Transfer Core posiada własną kopię startową i sprawdza ją przy prepare oraz dispatch; tę samą kontrolę stosują samodzielne wywołania natywne i adaptery CTM/RCM. Zmiana pierwotnych ustawień wywołującego nie zmienia istniejącego Core. Zachowano kolejność kontroli źródła/stanu, składni żądania, gotowości celu, adresów/możliwości i przełączników/uprawnień. Zaufane uprawnienie sesji nie omija listy protokołów.

Odmowa polityki protokołu wysyła tylko do źródła odzyskiwalny `ISTA 125 Connection\sprotocol\snot\spermitted FC<komenda>`, bez dostarczenia do celu i echa E oraz bez protokołu/tokena w opisie. Zwiększa istniejący licznik odrzuconych prób, nie raportu nieobsługiwanego protokołu klienta. Limity źródła nadal ograniczają odrzucone próby; odmowa polityki nie zużywa budżetu odbiorcy. Kolejne dozwolone żądanie i czat pozostają dostępne. Już zakolejkowane ramki nie są wycofywane.

[Specyfikacja ADC](https://dc-protocols.github.io/ADC.html) definiuje odmowę dostępu jako błąd 25/FC, a błędy 41/42 jako raporty klienta o obsłudze protokołu lub połączeniu bezpośrednim. Takie raporty STA zachowują walidację/routing także przy PR wykluczonym przez politykę startową; hub nie traktuje ich jako nowego żądania ani zgody na wybór innego protokołu.

## Efektywne ustawienia API i WebAdmin

Zabezpieczone `GET /api/v1/status` dodaje `transfers.configuration` obok niezmienionych 22 liczników `transfers.statistics`. Obiekt zawiera cztery przełączniki logiczne, osiem całkowitych limitów i `allowed_protocols`: tekst `"any"`, pustą tablicę dla `none` lub uporządkowaną tablicę nazw. Cudzysłowy i ukośniki odwrotne w dozwolonych nazwach ASCII są kodowane w JSON. Własna kopia efektywnych ustawień powstaje przed przeniesieniem opcji Network, a następnie jest kopiowana przez wątek API; bez odczytu przeniesionych napisów i współdzielonej zmiennej polityki.

Strona **Połączenia** pokazuje 13 efektywnych ustawień w EN/US i PL wraz z informacją o restarcie/odczycie. Starsze API ze statystykami bez konfiguracji zachowuje statystyki i pokazuje niedostępne ustawienia. Błędna obecna konfiguracja unieważnia migawkę, bez dopisywania wartości domyślnych. Adapter sprawdza przełączniki, zakres każdego limitu i dokładne ograniczone tablice protokołów, odrzuca obce pola, kopiuje tablice i wyświetla nazwy jako tekst. Eksport obejmuje wyłącznie dozwolone pola konfiguracji; historia monitoringu nadal posiada same liczniki. Nie dodano tożsamości, adresów, ścieżek, poświadczeń ani tokenów żądań.

## Weryfikacja

`transfers.configuration` sprawdza politykę domyślną/pustą/listę, własność, granice 16 nazw/32 bajtów, duplikaty/błędne dane, dosłowne wzorce, przyjęcia CTM/RCM D/E w Core/routerze, zachowanie nazwy na łączu, brak alokacji w odczycie/dispatch i walidację INI. `transfers.configuration.process` obejmuje ADC/ADCS i IPv4/IPv6, role aktywny/pasywny, prywatne odmowy i izolację postronnych, wielkość liter/rozszerzenia/cudzysłowy, dokładne ustawienia API, brak zmiany działającej polityki po edycji, zastosowanie po restarcie, liczniki, raporty i prywatność. Dotychczasowe testy limitów zachowują granice liczb i ochronę przed floodem.

Cztery testy prawdziwego ncdc `*.transferconfiguration*.ncdc` stosują politykę wyłącznie ADC albo ADCS, prywatnie odrzucają nieznany/przeciwny protokół, a następnie kończą rzeczywistą negocjację CTM lub RCM→CTM z połączeniem jawnym albo TLS z weryfikacją certyfikatu, na obu transportach huba. Testy frontendowe obejmują zakresy, starsze API, własność, eksport, dosłowne wyświetlanie i polski widok mobilny. Integracja C++ API/React potwierdza niestandardowe limity i długie nazwy z cudzysłowem/ukośnikiem wraz z obserwacją ruchu. Końcowe wyniki lokalne zapisuje [historia zmian](../../CHANGELOG.md); bez oczekiwania na zdalny Check status i deklarowania jego sukcesu.
