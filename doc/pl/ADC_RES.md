# ADC RES — alpha-0.5.8

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
