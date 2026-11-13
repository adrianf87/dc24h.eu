# ADC RES — alpha-0.5.8

**Bieżący etap — alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Etap dostarczony w alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) dodaje niezależny silnik C++20 koordynacji połączeń: ograniczone własne żądania, role aktywne/pasywne IPv4/IPv6 i niezmienne plany jednej próby związane z sesjami, anulowaniem i ponownymi kontrolami. Wspólna klasyfikacja adresów Users zachowuje Search. Routing CTM/RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

**Etap dostarczony w alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

**Etap dostarczony w alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) dodaje limity liczby i bajtów zapytań oraz wysyłanych/odbieranych wyników na czas życia połączenia, ograniczone powiadomienia, politykę gości/globalną, zaufane uprawnienia sesji i konfigurowalne limity aktywnych wyszukiwań. Kontrola poprzedza routing/rejestrację tokenu. Opcjonalny wymagany token uzupełnia kontrolę właściciela/generacji; bezpośrednie UDP klientów jest poza kontrolą huba.

**Aktualizacja — alpha-0.5.10:** [Active/Passive](ACTIVE_PASSIVE.md) przenosi sprawdzone I4/I6/U4/U6 do publicznego modelu Users i dodaje metadane dróg odpowiedzi TCP/UDP dla każdej rodziny do Search Core oraz niezmiennych planów. Pasywne DRES nadal przechodzi przez hub, a aktywne URES bezpośrednio między klientami. Testowane są zmiany INF, własność adresów i rzeczywiste wysyłanie/odbieranie UDP przez ncdc.

**Aktualizacja — alpha-0.5.9:** [Result Routing](RESULT_ROUTING.md) przekazuje sprawdzone DRES wyłącznie do gotowego celu przez ADC/ADCS. Plan wiąże czas życia sesji oraz — gdy podano TO — generację aktywnego wyszukiwania właściciela celu. Nadal obsługiwane są wyniki bez tokenu. Współdzielone ograniczone kolejki zachowują oryginalne pola bez rozsyłania publicznego, echa do nadawcy i treści w administracji.

Weryfikacja lokalna 2026-11-13: **108/108 zestawów CTest przeszło w każdej konfiguracji Debug, Release i ASan/UBSan**, z wymaganymi narzędziami integracyjnymi i domyślną kontrolą wycieków sanitizera. Obejmuje rzeczywiste ncdc ADC/ADCS, MariaDB i Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja rzeczywistego API C++ → adapter → React. Sprawdzono metadane wersji, UTF-8, zgodność plików językowych/linków dokumentacji, składnię zmienionych plików Python, przykładowe INI i białe znaki. Są to wyniki lokalne; zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.

Gałąź utworzono bezpośrednio z main `f5c80aa0142b4ae6aa751ccde9a9b9e95dad1a9b` (zawiera scalone alpha-0.5.7). Zachowano wcześniejsze gałęzie alpha-0.5.1–alpha-0.5.7 i historię. Ten etap odbiera i waliduje wyniki; przekazywanie wyników pozostaje osobnym przyszłym etapem.

## Parser i własność

`adc/res.hpp` udostępnia `parse_res(body)` zwracające `variant<ResRequest, ParseError, ResError>` oraz `validate_res(message)` zwracające `variant<ResRequest, ResError>`. Ciało nie zawiera LF. Błędy ramki, escape, UTF-8/NFC i nagłówka zachowują ParseError. Niezmienny ResRequest posiada sprawdzony Message, `search::ItemData`, opcjonalną liczbę wolnych slotów i opcjonalny zdekodowany token. Kopiowanie/przenoszenie zachowuje własność. Kolejność pól, nieznane pola i ich powtórzenia, zapis liczb oraz bajty tokenu przechodzą przez zwykłą serializację ADC bez zmian. Nie dodano osobnego serializatora.

Początkowy profil huba obsługuje **wyłącznie DRES**, z poprawnymi SID źródła/celu, bez selektorów funkcji i parametrów pozycyjnych. Ogólny parser nadal rozpoznaje pozostałe typy. B/F/E w NORMAL otrzymują odwracalny błąd semantyczny; C/U/H/I są poza istniejącym profilem sesji huba. Nie jest to deklaracja pełnej obsługi wyników ADC. Ręcznie zbudowane Message, również pola nieznane, są sprawdzane przez ograniczoną wspólną serializację i ponowne parsowanie.

## Pola i limity aplikacji

| Pole | Walidacja |
| --- | --- |
| FN | Wymagana pełna wirtualna nazwa w udziale: niepusty drukowalny tekst NFC, do 4096 zdekodowanych bajtów UTF-8. Końcowy `/` oznacza katalog. Dozwolone nazwy z początkowym `/` lub względne; sam korzeń, puste segmenty oraz `.`/`..` są odrzucane. Nazwa jest metadanymi, nigdy ścieżką otwieraną w systemie. |
| SI | Wymagana nieujemna liczba całkowita 64-bit ze znakiem, także zero; przepełnienie, ułamek i początkowy plus są błędami. Rozmiar katalogu pozostaje jawnymi metadanymi. |
| SL | Opcjonalna nieujemna liczba wolnych slotów, 64-bit ze znakiem; brak różni się od zera. |
| TO | Opcjonalny dla zgodności z SCH bez tokenu; podany token musi być niepustym drukowalnym tekstem NFC, do 128 zdekodowanych bajtów UTF-8, bez modyfikacji. Puste TO jest błędem. |
| TR | Dla pliku wymagany kanoniczny korzeń Tiger Base32: 39 wielkich symboli bez dopełnienia, zachowane wszystkie 192 bity. Niezerowe bity dopełnienia są błędem. Katalog nie ma hasha pliku i w tym profilu odrzuca TR. |

Każde znane pole jest pojedyncze; identyczne duplikaty również są odrzucane. Nieznane poprawne pola zachowywane są bez interpretacji. Łączny limit zdekodowanych parametrów wynosi 8192 bajty, wraz z dwubajtowymi nazwami pól i nieznanymi wartościami; API lokalnych obiektów sprawdza go przed kopiowaniem i pracą Unicode. Nadal obowiązują wspólne limity 128 parametrów i 16384 bajtów ciała ramki. Parser nie sprawdza istnienia, zawartości ani hasha pliku, obecności celu, aktywności tokenu, uprawnienia odpowiadającego klienta ani dopasowania filtrów.

Znaczenia pól pochodzą z [ADC RES / SCH](https://adc.sourceforge.io/ADC.html) i [TIGR](https://adc.sourceforge.io/ADC-EXT.html). Opcjonalne TO odpowiada istniejącej obsłudze SCH bez tokenu; specyfikacja wymaga skopiowania TO podanego w zapytaniu. Zasady ścieżek, katalogów i limitów powyżej są lokalną polityką. Klient odpowiada za bezpieczne przetwarzanie otrzymanych nazw.

## Rzeczywisty odbiór i izolacja

LoginHub uruchamia walidację po kontroli stanu połączenia, gotowości sesji i własności SID źródła przez TCP/TLS. Błędny stan/źródło zachowują pierwszeństwo błędów krytycznych 244/240 również dla błędnego semantycznie wyniku. Błąd danych otrzymuje stałe `ISTA 100 Invalid\ssearch\sresult FCDRES`; poprawny wynik — `ISTA 125 Search\sresult\srouting\snot\simplemented FCDRES`. Połączenie pozostaje użyteczne. Dla innych zakresów wspieranych przez sesję odpowiedź wskazuje właściwy FOURCC.

Odpowiedź nie potwierdza przekazania ani korelacji. Wyniki nie są przekazywane, publikowane, zapisywane w rejestrze/bazie, eksportowane przez API ani zliczane jako czat. Nazwy, tokeny i korzenie nie trafiają do logów ani statusu. Cel nie otrzymuje wyniku. Błąd alokacji podczas odbioru zamyka tylko źródło z istniejącą przyczyną błędu wewnętrznego; inne sesje i kolejki pozostają użyteczne. Nadal obowiązują limity ramek, tempa i wyjścia transportu; nie dodano osobnej polityki przeciw zalewaniu wynikami.

## Weryfikacja i dalsze prace

`adc.res` sprawdza typowane wartości, pliki/katalogi, pola opcjonalne, odtwarzanie ramki, Unicode/escape, błędy pól i hashy, granice, wrogie obiekty, kopie/przenoszenie, błędy alokacji i izolację odbioru. `adc.res.process` używa fragmentowanych rzeczywistych ramek ADC/ADCS, błędów odwracalnych z następującymi znacznikami, braku przekazania do celu, pierwszeństwa stanu/źródła i prywatności API/logów/czatu. Rzeczywiste `adc.feature.ncdc` / `adcs.feature.ncdc` wymagają teraz nowej odpowiedzi dla poprawnego wyniku po dopasowaniu zindeksowanego pliku i wysłaniu DRES przez ncdc. Dotychczasowe testy SCH, TTH, tokenów i funkcji pozostają włączone.

Korelacja/routing wyników, polityka niezamówionych odpowiedzi, kwalifikacja odbiorcy, limity liczby wyników, deduplikacja/historia, UDP/SUDP, transfery i administracja pozostają przyszłymi etapami. Nie dodano ustawienia INI/CLI, wpisu katalogu operatora, zależności, endpointu API, migracji bazy ani strony wyszukiwania WebAdmin. Kod pozostaje EN/US; dokumentacja ma wersje `doc/us` i `doc/pl`, a katalogi komunikatów `languages/us` i `languages/pl`.
