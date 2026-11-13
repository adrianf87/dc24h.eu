# Result Routing — alpha-0.5.9

**Bieżący etap — alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

**Etap dostarczony w alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) dodaje limity liczby i bajtów zapytań oraz wysyłanych/odbieranych wyników na czas życia połączenia, ograniczone powiadomienia, politykę gości/globalną, zaufane uprawnienia sesji i konfigurowalne limity aktywnych wyszukiwań. Kontrola poprzedza routing/rejestrację tokenu. Opcjonalny wymagany token uzupełnia kontrolę właściciela/generacji; bezpośrednie UDP klientów jest poza kontrolą huba.

**Aktualizacja — alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) przenosi sprawdzone I4/I6/U4/U6 do publicznego modelu Users i dodaje metadane dróg odpowiedzi TCP/UDP dla każdej rodziny do Search Core oraz niezmiennych planów. Pasywne DRES nadal przechodzi przez hub, a aktywne URES bezpośrednio między klientami. Testowane są zmiany INF, własność adresów i rzeczywiste wysyłanie/odbieranie UDP przez ncdc.

Weryfikacja lokalna 2026-11-13: **110/110 zestawów CTest przeszło w każdej konfiguracji Debug, Release i ASan/UBSan**, z wymaganymi narzędziami integracyjnymi i domyślną kontrolą wycieków. Obejmuje mieszane ADC/ADCS, awarie alokacji, rzeczywiste odbieranie/generowanie wyników ncdc, MariaDB i Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja rzeczywistego API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych/linków dokumentacji, składnię zmienionych plików Python, przykładowe INI i białe znaki. Są to wyniki lokalne; zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.

Gałąź utworzono bezpośrednio z main `c837ecaec3d803953c083e23d78dd621c346c19f`, po scaleniu alpha-0.5.8. Zachowano kod, wcześniejsze gałęzie i historię. Ten etap dodaje rzeczywiste przekazywanie wyników przez hub; gałąź publikowana jest osobno od main.

## Zasady routingu

`adc/result_routing.hpp` definiuje ResultRouter, RoutedResult, niezmienny ResultFrame i typowany ResultRoutingError. Adapter korzysta z istniejących kontroli gotowych sesji i aktywnych tokenów Search Core; nie tworzy nowego rejestru wyszukiwań/wyników. Core i SessionManager muszą żyć dłużej niż adapter i jego wywołania. Całość działa na wątku właściciela reaktora.

Przygotowanie sprawdza gotowość odpowiadającego klienta i własność SID źródła przed [parserem ADC RES](ADC_RES.md). Obsługiwane jest wyłącznie DRES. SID celu jest rozwiązywany do gotowej sesji o konkretnym czasie życia; brak celu, sama identyfikacja lub zamykanie powodują odmowę bez zastępczego rozsyłania. Wynik do siebie trafia do kolejki raz. Nie ma broadcastu, rozsyłania według funkcji, echa do nadawcy ani automatycznego UDP.

Jeden niezmienny współdzielony pakiet posiada sprawdzony Message i dane ramki. Zachowuje kolejność pól, nieznane pola/powtórzenia, zapis liczb, token i kanoniczny hash przez zwykłą serializację ADC. Semantyka i limity FN/SI/SL/TO/TR pozostają zgodne z alpha-0.5.8. Nazwy są wirtualnymi metadanymi; hub nie otwiera, nie hashuje ani nie indeksuje plików.

### Tokeny i okna wyszukiwania

Podane TO musi dokładnie wskazywać aktywne wyszukiwanie należące do czasu życia sesji celu. Ten sam token może niezależnie występować u różnych właścicieli; nie zmienia się wielkości liter ani Unicode. Nieznany, wygasły, anulowany token lub token innego właściciela powoduje odwracalny stały błąd. Plan zapamiętuje lokalną generację ActiveSearch i termin ważności; dispatch odrzuca anulowaną/zastąpioną generację również po ponownym użyciu właściciela i tokenu. Odbiór wyniku nie przedłuża terminu ani nie kończy wyszukiwania; możliwe są kolejne odpowiedzi do wygaśnięcia/anulowania/rozłączenia.

TO pozostaje opcjonalne dla zgodności z SCH bez tokenu. Takie wyniki są kierowane po sprawdzonym źródle/celu bez korelacji wyszukiwania. Usuniętego TO nie da się odróżnić od poprawnej odpowiedzi bez tokenu; jest to jawna polityka zgodności, a nie ochrona przed niezamówionymi wynikami. Aktywne TO potwierdza tylko okno korelacji, nie dowodzi otrzymania zapytania przez nadawcę, dopasowania filtrów ani posiadania pliku. Token nie jest poświadczeniem. Nie dodano listy uprawnionych nadawców, uwierzytelniania plików/filtrów, limitu liczby wyników ani deduplikacji. Opóźniona ramka z identycznym ponownie użytym TO jest nierozróżnialna od odpowiedzi nowej generacji; lokalna generacja chroni wyłącznie przygotowane już plany.

Podstawą są [routing komunikatów i SCH/RES ADC](https://adc.sourceforge.io/ADC.html) oraz hashe [TIGR](https://adc.sourceforge.io/ADC-EXT.html). Kontrola aktywnego tokenu, profil wyłącznie DRES i limity są lokalną polityką serwera.

## Dispatch, kolejki i błędy

Plan należy do domeny tworzącego ResultRouter, także po ponownym użyciu adresu obiektu. Kopie współdzielą jedną próbę przekazania. Błąd obcej domeny lub braku sink nie zużywa próby; pozostałe przypadki zużywają ją przed ponowną walidacją/wywołaniem. Nie można ponawiać odrzuconej, nieaktualnej lub zakończonej wyjątkiem próby. Sink nie może ponownie wywoływać dispatch ani rzucać wyjątków; wyjątek propaguje się po zużyciu próby.

Dispatch ponownie sprawdza własność/gotowość źródła, czas życia/gotowość celu i opcjonalną generację wyszukiwania. Po utworzeniu sink nie alokuje pamięci. DispatchReport odróżnia jeden zakolejkowany wynik od odmowy kolejki; sukces oznacza przyjęcie do kolejki, nie dostarczenie/odczyt przez klienta. Kontrola odbywa się przy przyjmowaniu do kolejki; późniejsze wygaśnięcie nie usuwa już zakolejkowanych wyników FIFO.

Ograniczone kolejki LoginHub obsługują teraz SharedResultFrame obok istniejących pakietów. Częściowy zapis TCP/TLS zachowuje przypięty początek i niezależny offset. Pełne logiczne obciążenie ramki pozostaje do skonsumowania/usunięcia, także podczas ponowień TLS. Obowiązują limity klienta i łącznego wyjścia. Przepełnienie lub błąd alokacji kolejki zamyka wyłącznie odbiorcę; odpowiadający klient i pozostałe sesje pozostają użyteczne. Takie awarie nie wymagają nowej alokacji odpowiedzi do nadawcy. Błąd alokacji przygotowania zamyka wyłącznie źródło. Dla wyniku do siebie źródło i odbiorca są naturalnie tym samym właścicielem awarii.

| Warunek | Zachowanie protokołu |
| --- | --- |
| Poprawne DRES, gotowy cel i opcjonalne aktywne TO | Oryginalna sprawdzona ramka trafia wyłącznie do celu; bez potwierdzenia sukcesu i echa. |
| Błąd semantyczny/niewspierany profil B/F/E | `ISTA 100 Invalid\ssearch\sresult FC<FOURCC>` |
| Brak celu lub jego gotowości | `ISTA 140 Search\sresult\srecipient\sunavailable FCDRES` |
| Podane TO nieaktywne dla celu | `ISTA 100 Search\sresult\stoken\sis\snot\sactive FCDRES` |
| Błędny stan/źródło | Dotychczasowe krytyczne 244/240 mają pierwszeństwo również dla błędnych danych. |
| Błąd składni/UTF-8/escape | Dotychczasowa polityka parsera/transportu, bez odbijania treści. |

Diagnostyka nie zawiera nazw, hashy, tokenów ani kluczy celu. Wyszukiwanie nie zwiększa liczników czatu. Treści nie trafiają do SQL, logów, prywatnego API statusu ani WebAdmin. Pozostają ogólne limity ramek/tempa; nie dodano osobnego ogranicznika wyników, ustawienia, wpisu katalogu operatora, migracji, endpointu ani kontrolki wyszukiwania. Języki runtime pozostają w `languages/us` i `languages/pl`, a dokumentacja w `doc/us` i `doc/pl`.

## Weryfikacja i dalsze prace

`search.results` obejmuje dokładny cel/wynik do siebie, oryginalne pola i własność, aktywne/obce/brakujące tokeny i wielkość liter, anulowanie/ponowne użycie/wygaśnięcie, zamknięcie źródła/celu i ponowne użycie SID, domeny routera i wspólną próbę, odmowę/wyjątek sink, dispatch bez alokacji oraz awarie przygotowania/źródła/kolejki odbiorcy. Testy LoginHub sprawdzają przypięte początki, częściowe zapisy, rozliczanie bajtów, zapełnienie, izolację i sprzątanie.

`search.results.process` wysyła fragmentowane ramki przez mieszane ADC/ADCS z obserwatorem. Sprawdza niezależne tokeny właścicieli, brak echa/broadcastu/duplikatów, zgodność bez tokenu i plików/katalogów, odwracalne błędy, rozłączenie/ponowne połączenie, pierwszeństwo stanu/źródła i prywatność API/logów/czatu. `adc.res.process` potwierdza teraz rzeczywiste przekazanie poprawnych wyników przy zachowaniu testów błędów.

Rzeczywiste `adc.feature.ncdc` / `adcs.feature.ncdc` sprawdzają oba kierunki: wynik aktywnego wyszukiwania ncdc widoczny w jego interfejsie (obecny klient pomija TO w zapytaniu, sprawdzając zgodność bez tokenu) oraz DRES wygenerowane przez zindeksowany udział ncdc po FSCH i przekazane do klienta wyszukującego. Przechwycone dane proxy TLS pozostają zaszyfrowane. Dotychczasowe testy nazw/filtrów/TTH, logowania/czatu/ponownego połączenia i regresji wyszukiwania pozostają włączone.

Kolejne etapy obejmują kwalifikację odpowiadających klientów, bardziej restrykcyjną politykę wyników bez tokenu/niezamówionych, limity liczby/tempa, deduplikację/historię, UDP/SUDP, negocjację transferów i administrację. Wydanie nie deklaruje tych funkcji ani walidacji obciążenia produkcyjnego.
