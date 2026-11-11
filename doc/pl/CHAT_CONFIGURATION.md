# Chat Configuration — alpha-0.4.10

Etap bazuje bezpośrednio na main `6d61edfdd2057a77f8c4722a1495cfa37bea7b19`, po scaleniu Anti-Flood. Kod i diagnostyka protokołu używają EN/US UTF-8; pomoc/błędy operatora i dokumentacja obsługują też polski. Wcześniejsze gałęzie/historia są zachowane.

## Ustawienia startowe

`chat::Settings` w `chat/settings.hpp` grupuje dotychczasowe FloodLimits, sprawdzoną `Policy` klienta i powitanie. Parser, walidacja po scaleniu ustawień i start Network korzystają z jednego modelu. Network kopiuje wartości do LoginHub; adaptery posiadają własne niemutowalne kopie polityki. Ustawienia są startowe: zmień INI i uruchom serwer ponownie. Nie dodano flag CLI, przeładowania, mutacji HTTP ani edycji ustawień przez WebAdmin.

| Klucz `[chat]` | Domyślnie | Wartość / znaczenie |
| --- | --- | --- |
| `max_text_bytes` | 16384 | 1–16384 odkodowanych bajtów tekstu UTF-8 |
| `public_enabled` | on | on/off; BMSG klienta |
| `private_enabled` | on | on/off; DMSG i EMSG klienta |
| `actions_enabled` | on | on/off; rzeczywiste akcje ME1 w B/D/E |
| `welcome_enabled` | on | on/off; początkowe powitanie IMSG klienta |
| `welcome_message` | Welcome to dc24h.eu. Guest access is enabled. | Niepusty tekst UTF-8/NFC do 4096 bajtów przy włączonym powitaniu |
| `max_messages_per_second` | 5 | 1–1000000 |
| `message_burst` | 10 | 1–1000000 |
| `max_bytes_per_second` | 32768 | 1–16777216 |
| `byte_burst` | 131072 | 16384–16777216 |

Wartości domyślne zachowują dotychczasowe działanie. Cztery nazwy/zakresy Anti-Flood pozostają takie same; zobacz [Anti-Flood](ANTI_FLOOD.md). W C++ opcje Network udostępniają teraz `chat::Settings`: wcześniejsze płaskie pola limitów są w `.chat.flood`, polityka klienta w `.chat.policy`, a powitanie ma osobne pola.

```ini
[chat]
max_text_bytes = 16384
public_enabled = on
private_enabled = on
actions_enabled = on
welcome_enabled = on
welcome_message = Welcome to dc24h.eu. Guest access is enabled.
max_messages_per_second = 5
message_burst = 10
max_bytes_per_second = 32768
byte_burst = 131072
```

Fizyczny wiersz INI `[chat] welcome_message` ma limit 8192 bajtów, łącznie z kluczem, otaczającymi białymi znakami i opcjonalnym CR; sama wartość powitania nadal ma limit 4096 bajtów. Pozostałe wiersze zachowują limit 4096 bajtów, a cały plik 65536 bajtów.

Klucze rozróżniają wielkość liter, są pojedyncze i nie dopuszczają duplikatów. Boolean przyjmuje tylko małe `on` i `off`; `true`, `1` i wielkie litery są błędne. Dotychczasowy parser INI usuwa otaczające białe znaki i zachowuje wewnętrzne spacje. Powitanie jest literalnym pojedynczym wierszem INI: cudzysłowy, backslash, `\n`, odwołania do zmiennych i tekst PM/ME nie są rozwijane. `\n` wyświetla więc backslash i literę n. Nie ma szablonów ani interpretera komend.

Puste powitanie jest dozwolone tylko przy `welcome_enabled=off`. Inne niepuste wartości są sprawdzane także przy wyłączeniu: długość, UTF-8/NFC i znaki sterujące. `chat::validate_settings`, `validate_policy` i `validate_welcome` odrzucają błędną konfigurację przed otwarciem portów; wejście nie jest naprawiane ani obcinane. Przykładowe INI i pomoc US/PL zawierają wartości domyślne.

## Polityka klienta i diagnostyka

Limit tekstu liczy odkodowane bajty UTF-8, nie punkty kodowe, grafemy, nagłówki ani flagi. Emoji w czterech bajtach mieści się w limicie czterech bajtów; dodanie znaku ASCII przekracza limit. Białe znaki są zachowane. Pełny limit zakodowanej treści ADC 16384 bajtów plus LF nadal jest osobny i nie można go zwiększyć konfiguracją; rozwinięcia escape i metadane są do niego wliczane.

Bezpośrednie adaptery publiczny/prywatny i MessageRouter używają polityki we wspólnym `validate_chat`. Pełna walidacja tekstu/SID/flag oraz gotowość i własność nadawcy pozostają pierwsze. Następnie limit skonfigurowanej długości poprzedza uprawnienia kanału/akcji. Legalna wiadomość ponad limit otrzymuje typowany `text_limit`; wyłączona operacja `disabled`. Polityka dotyczy wszystkich przyjętych klientów, bez ufania CT i tworzenia wyjątków dla operatorów/kont.

| Próba | Odpowiedź / dostarczenie |
| --- | --- |
| Poprawny tekst B/D/E ponad skonfigurowany limit | Odwracalne `ISTA 100 Chat\stext\slimit\sexceeded FC<FOURCC>` |
| Wyłączony czat publiczny, prywatny lub ME1 | Odwracalne `ISTA 125 Chat\soperation\sis\sdisabled FC<FOURCC>` |
| ME0 przy wyłączonych akcjach | Zwykła wiadomość dozwolona, jeżeli kanał jest włączony |
| ME01/inny poprawny zapis całkowitej jedynki | Traktowany jako ME1 i odrzucony przy wyłączonych akcjach |
| Niepoprawna składnia, źródło, stan lub znane pola | Dotychczasowe zachowanie parsera/stanu/fatalnych błędów, nie odmowa konfiguracji |

Kody 100 i 125 używają odwracalnej powagi oraz kategorii ogólnej/odmowy dostępu z [ADC STA](https://adc.sourceforge.io/ADC.html). Stałe opisy nie zawierają tekstu klienta ani powitania. Odmowa nie publikuje pakietu publicznego/prywatnego ani echa EMSG i pozostawia gotową sesję. Kolejne dozwolone wiadomości mogą przejść. Przełączniki czatu nie wyłączają INF/QUI listy, keepalive ani zaufanych systemowych IMSG. PM nie staje się broadcastem, a akcje są odrzucane zamiast po cichu zmieniać się w zwykły tekst.

Anti-Flood pozostaje włączony w rzeczywistych połączeniach Network i obciąża poprawne próby B/D/E przed polityką/odmową i routingiem. Ponawianie odmów nie omija więc budżetów liczby/bajtów ani ograniczeń wyjścia. Gotowość odbiorcy, własne plany życia/domeny, FIFO i ponowna walidacja dispatch pozostają obowiązkowe. Zaufane niskopoziomowe Core/systemowe API są oddzielone od polityki klienta.

## Powitanie i własność

Włączone własne powitanie jest sprawdzane przy konstrukcji i przechowywane w jednym współdzielonym niemutowalnym pakiecie IMSG. Następuje po własnym BINF, wlicza się do pełnego sprawdzonego budżetu powitania i zawiera jeden escapowany tekst bez SID źródła i flag PM/ME. Jest niezależne od `max_text_bytes` oraz przełączników klienta. Końcowe LF własnego BINF nadal oznacza gotowość dokładnie raz; wysłanie IMSG jej nie powtarza.

Przy wyłączonym powitaniu nie ma ramki ani obciążenia bajtami, a zakończenie własnego BINF nadal oznacza gotowość. Algorytm listy i zdarzenia obecności pozostają zachowane. Wewnętrzny LoginLimits posiada `chat::Policy`, opcjonalne limity transportu Anti-Flood i opcjonalny własny `welcome_text`; brak tekstu oznacza wyłączenie. Dotychczasowe `adc::hub_welcome_text` pozostaje aliasem tekstu domyślnego.

Powitanie może być polskie lub w innym poprawnym UTF-8 NFC; nie ma negocjacji języka klienta. `--language` operatora nie zmienia tekstu na sieci. Powitanie nie trafia do logów/eksportów API, choć celowo jest widoczne dla dołączających klientów. Nie dodano zapisu trwałego, migracji bazy, pól API, zależności, moderacji, modelu ról ani endpointu administracyjnego.

## Weryfikacja

Wszystkie **88/88 zestawów CTest** przeszły lokalnie w Debug, Release i ASan/UBSan przy wymaganych narzędziach integracyjnych i domyślnym wykrywaniu wycieków. Obejmuje to testy jednostkowe/procesowe konfiguracji czatu, wcześniejsze regresje czatu/bezpieczeństwa/kolejek, rzeczywisty ncdc ADC/ADCS z własnym powitaniem, MariaDB i Apache HTTPS. Przeszły też typecheck/budowanie produkcyjne WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, linki dokumentacji, przykładowy INI i pomoc US/PL. Wyniki są lokalne; zdalne CI, pełna macierz systemów/klientów i gwarancje obciążenia produkcyjnego wymagają osobnej walidacji.

`chat.configuration` obejmuje zgodne wartości domyślne, zakresy, wyłączone/puste/niepoprawne powitania, limity bajtów UTF-8, politykę bezpośrednich adapterów, niezależność publicznego/prywatnego, ME0/aliasy akcji, niemutowalne kopie, pierwszeństwo źródła, odwracalne odmowy, opcjonalne/własne powitanie, niezależność systemowych wiadomości i sprzątanie kolejek.

`chat.configuration.process` używa obu transportów źródła: wszystkie przełączniki kanałów/akcji, logowanie bez powitania, dokładni odbiorcy/echo przyjętych wiadomości, uporządkowane znaczniki INF potwierdzające brak wycieku, granice Unicode/spacji, literalne własne powitanie, odzyskiwanie, fatalne podszywanie się i prywatność logów/API. Testy konfiguracji sprawdzają nowe klucze, ścisłe booleany, duplikaty, zakresy, literalny tekst INI, puste wyłączone powitanie i polski katalog błędów. Regresje graniczne dopuszczają 4081 bajtów oraz pełne 4096 bajtów ASCII/UTF-8 z LF, CRLF lub bez końcowego LF; odrzucają wartości 4097 bajtów i wiersze fizyczne 8193 bajtów; zachowują zwykłe limity wierszy i kontrolę duplikatów. Rzeczywiste ADC/ADCS sprawdza też dokładną dostawę pełnego powitania Unicode z escape i gotowość do dalszego czatu. Rzeczywisty ncdc ADC/ADCS wyświetla skonfigurowane powitanie przy zachowaniu testów czatu/akcji/flag/reconnect. Anti-Flood i ogólna walidacja nadal są testowane.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(chat.configuration|configuration|adc.ncdc|adcs.ncdc)' --output-on-failure
```

Bieżące wyniki opisuje [CHANGELOG.md](../../CHANGELOG.md). Zdalne CI, pełna macierz systemów/klientów i gwarancje obciążenia produkcyjnego wymagają osobnych dowodów.
