# INF Updates — alpha-0.3.10

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

Etap powstał z main `c3cc9c098512b9a9a8299dbd8d1c98e455073eaf` po scaleniu alpha-0.3.9 User List. Poprzednie etapy są obecne. Gałąź alpha-0.3.10 zachowuje main/historię i nie jest scalana automatycznie.

## Kontrakt przyrostowy

[Specyfikacja INF ADC](https://adc.sourceforge.io/ADC.html#_inf) opisuje zmianę wskazanych pól, zachowanie pominiętych i anulowanie przez samą nazwę pola. `adc::InfUpdates` realizuje to dla gotowych publicznych gości. Pożycza SessionManager w wątku reaktora; nie posiada gniazd, trwałego rejestru, bazy, zegara ani poświadczeń. Wywołujący podaje ClientSession należącą do lokalnego SessionId. Początkową identyfikację nadal obsługuje UserLogin, a pełne listy UserList.

Wyjściem jest teraz przyrostowy BINF, zamiast całego publicznego snapshotu z anulowaniami. Pola zachowują kolejność zwalidowanego wejścia. Wskazane znane wartości pochodzą ze sprawdzonego kandydata, także po normalizacji adresu, nigdy z surowych wartości klienta. Znane jawnie podane wartości niezmienione nadal są rozsyłane dla zgodności. Anulowanie jest rozsyłane jako sama nazwa tylko wtedy, gdy stary snapshot posiadał pole. Pominięte pola pozostają w metadanych/profilu i nie wchodzą do przyrostu. Nieznane pola, anulowania nieobecnych wartości i puste wejście nie tworzą publicznego pola; całkowicie pusta/nieznana aktualizacja nie jest rozsyłana. PID jest zabroniony w aktualizacji i nigdy nie jest przechowywany/rozsyłany.

Późniejszy gość nadal dostaje pełne aktualne ClientInfo, nie historię przyrostów. Zachowuje to pola ADC poza ogólnym profilem User, np. adresy, deklarowane prędkości i liczniki hubów. Zero pozostaje wartością liczbową; samo SS/SF/SL usuwa opcjonalną wartość. Deklaracje klienta nie są dowodem zawartości udziału ani uprawnień.

## Przygotowanie i zatwierdzenie

`InfUpdates::apply(id, client, parsed_message, now)` wymaga istniejącej gotowej Session i NORMAL. Sprawdza, czy projekcja obecnego ClientInfo daje tę samą tożsamość/profil User w rejestrze. Wykrywa obcego klienta lub niezależną zmianę metadanych objętych modelem. Nie uwierzytelnia niezależnie identyfikatora transportu i nie porównuje pól właściwych tylko ADC z drugą przechowywaną kopią.

ClientSession nie udostępnia modyfikowalnych metadanych. Prywatne prepare_update dostępne tylko przez friend waliduje kandydata istniejącym InfValidator bez zmiany metadanych. Sprawdza BINF/źródłowy SID, wymaganą tożsamość, niezmienność CID, brak PID, unikalność pól, znaki/UTF-8/NFC i rozmiary, liczby/funkcje, adres partnera i limity deklaracji uprawnień. Składnia ramki/argumentów należy do parsera; nieprawidłowe lub nienormalizowane ramki mogą być odrzucone wcześniej. INF w NORMAL nie wymaga dodatkowej zmiany fazy protokołu.

Pełny kandydat jest projektowany do ograniczonego User. Publiczny przyrost powstaje i jest sprawdzany przez serializację przed zmianą modelu. SessionManager transakcyjnie zmienia User/profil i indeks nicka przy niezmiennych SID/CID i monotonicznym czasie. Dopiero po sukcesie nierzucone przeniesienie zastępuje ClientInfo; zwrot własnego wyniku także nie rzuca. Odrzucenie lub błąd alokacji przed zatwierdzeniem zachowuje razem ClientInfo, User/profil i klucze/nick, również gdy wcześniejsze pole było poprawne, a późniejsze błędne. Inny użytkownik zachowuje rezerwację. Czas jest dostarczony bez ukrytego odczytu zegara.

`InfUpdateResult` zawiera AppliedInfUpdate z opcjonalnym własnym komunikatem lub InfUpdateFailure. Błąd rozróżnia ClientSessionError, RegistryError, SessionError oraz wewnętrzne identity_mismatch/invalid_public_user/invalid_output. Zachowano mapowanie ADC: zajęty nick 222, CID 224, błędny nick 221, adres 246, inne błędne wymagane dane INF zwykle 243, stan 244 i naruszenia modelu/wewnętrzne 210. Opisy są stałe i nie zawierają danych klienta.

Samodzielne ClientSession::observe zachowuje dotychczasowe testy złożenia protokołu INF. Działający adapter połączony z SessionManager musi używać InfUpdates dla gotowej sesji; niezależne observe i późniejsza zmiana User tracą wspólną transakcję i mogą dać wykrytą niespójność. Accepted/identified/czekające na powitanie/closing/closed nie mogą korzystać z aktualizatora.

## Rozsyłanie i ograniczenia

LoginHub kieruje INF gotowej sesji do InfUpdates przed ogólnym observe. Zachowuje wczesne sprawdzenie kolizji nicka. Po sukcesie rozsyła zweryfikowany własny przyrost do nadawcy i przyjętych niezamykanych klientów, również identified z powitaniem w kolejce. FIFO umieszcza późniejszą aktualizację za własnym INF odbiorcy. Początkowa lista używa zatwierdzonych pełnych metadanych, więc późny gość i istniejący klienci osiągają zgodność po zmianach/anulowaniach. Aktualizacja nie emituje kolejnego zdarzenia logowania/gotowości.

LoginHub przechwytuje bad_alloc samego apply i wymusza natychmiastowe internal_error bez kolejnej alokacji statusu. Błędna aktualizacja nie jest publikowana, stara tożsamość/profil pozostają do sprzątania. Nadal obowiązują limity klienta/huba, przypięty przód TLS, terminy wyjścia i sprzątanie z IQUI. Rozsyłanie następuje po zatwierdzeniu: wolny odbiorca może być rozłączony po zapełnieniu kolejki. Nie ma atomowego doręczenia/wycofania dla całej sieci ani gwarancji po wyczerpaniu pamięci systemu. Alokacje rozsyłania po zatwierdzeniu są poza ochroną apply.

Pamięć tymczasową ograniczają istniejące 16 KiB ramki, 128 parametrów, 4096 bajtów ClientInfo oraz ograniczenia User/profilu/indeksów. Kandydat/projekcja/przyrost są alokowane przed zatwierdzeniem. Nie dodano ustawienia limitu, zależności, migracji SQL, pola API JSON, funkcji WebAdmin ani tłumaczeń runtime. Pozostają EN/US UTF-8 i istniejące languages/us/languages/pl. Konta, role, trwałe dane i czat/wyszukiwanie/transfery pozostają planami.

## Weryfikacja

`users.inf_updates` sprawdza wskazane pola, normalizację IP, polski nick/opis, zera, anulowania, zachowanie pominiętych pól, echo znanych wartości niezmienionych, pomijanie nieznanych/pustych aktualizacji i własność snapshotów. Odrzucenia obejmują błędne wielopolowe wartości, kolizję nicka, usunięcie wymaganej nazwy, PID, uprawnienia/IP/funkcje/tekst/duplikaty, fałszywe źródło, wsteczny czas, brak/pending/closing i niespójne metadane adaptera. Kampania alokacji C++ ponawia każdy punkt walidacji/projekcji/przyrostu/indeksu, potwierdzając zachowanie obu modeli i kluczy przy błędzie oraz brak wpływu na innego użytkownika. Nie wstrzykuje się błędów alokatora C ani systemu.

`users.inf_updates.process` sprawdza jednakowe przyrosty dla mieszanych ADC/ADCS, filtrowanie danych prywatnych, normalizację, anulowania, pomijanie nieznanych pól, pełny snapshot późniejszego gościa, brak rozesłania błędnej zmiany przed odrzuceniem i reconnect z tym samym CID. Test działa w obu kierunkach. Rzeczywisty ncdc zmienia opis w działającej sesji przez ADC i ADCS; przekazywane jest także opcjonalne HN wskazane w tej samej ramce, bez pozostałego całego profilu. Wcześniejsze testy rejestrów/list uwzględniają teraz przyrosty, zachowując zgodność list i routing niezmiennego CID. Lokalne wyniki i ograniczenia opisuje [TESTING.md](TESTING.md).
