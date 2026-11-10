# Limits — alpha-0.3.12

Etap powstał bezpośrednio z main `4e54b2e0c18f39542cb550d07d4a527ca11a1e10` po scaleniu alpha-0.3.11 User Disconnect. Zachowano poprzednie etapy/historię; alpha-0.3.12 nie jest scalana automatycznie.

## Ustawienia i efektywna pojemność

Serwer ma jedno życie połączenia, jeden model Session na przyjęty transport i najwyżej jednego publicznego gościa User na Session. Limity są wspólne dla wszystkich nasłuchów ADC i ADCS, bez osobnych pul transportów i wyjątków dla kont.

| Ustawienie | Domyślnie | Zakres | Własność/rozliczanie |
| --- | --- | --- | --- |
| `[network] max_connections`, `--max-connections N` | 1024 | 1..65535 | Istniejące obiekty połączeń Network, także oczekujące TLS |
| `[users] max_sessions`, `--max-sessions N` | 65535 | 1..65535 | Wszystkie modele SessionManager aż do usunięcia |
| `[users] max_users`, `--max-users N` | 65535 | 1..65535 | Publiczne rejestracje od identyfikacji do usunięcia |

Efektywne sesje = min(max_connections, max_sessions). Efektywni użytkownicy = min(efektywne sesje, max_users). Wyższe pułapy są poprawne, ale nie podnoszą niższych. Istniejące konfiguracje zachowują działanie: domyślne efektywne limity sesji i użytkowników wynoszą 1024. Zero jest błędem, nie oznacza braku ograniczenia. Pojemność nie jest z góry alokowana do wysokości pułapu.

Przykład: max_connections=100, max_sessions=60 i max_users=40 pozwalają utrzymywać najwyżej 60 połączeń/modeli i 40 publicznych rejestracji. Pozostałe miejsca sesji pozwalają negocjować, ale poprawna tożsamość nie omija limitu użytkowników. max_connections=2 z nowymi limitami 65535 nadal oznacza najwyżej dwie sesje/osoby.

Zachowano kolejność domyślne → jawny INI → CLI. Zakresy sprawdzane są przed ograniczeniem pułapów, więc błędna wysoka wartość nie ukryje się za niższym limitem. Programowe limity Network, LoginHub i SessionManager są analogicznie walidowane. Dwujęzyczne `--help`, przykład INI i efektywne wartości logu startowego opisują te ustawienia. Brak dynamicznego przeładowania i zmiany limitów podczas pracy.

## Przyjęcie i zwolnienie

Po accept Network sprawdza limit połączeń i efektywny limit sesji SessionManager przed przyjęciem per-IP oraz alokacją tokena transportu, TLS, obserwacji, Peer protokołu i modelu Session. Nadmiarowe gniazdo jest od razu zamykane bez danych aplikacji i zdarzenia cyklu życia. Dotyczy to TCP i niedokończonego TLS. Połączenie TCP w jądrze mogło już się udać. Odrzucenia zwiększają istniejący ogólny licznik rejected i używają ograniczanych ostrzeżeń; nie są odrzuceniami limitów security ani udanymi przyjęciami per-IP.

`users::Limits::bounded_by` oblicza sprawdzone efektywne pułapy. Autorytatywne są size/capacity i identified_users/user_capacity SessionManager. Nie dodano osobnego licznika rezerwacji, który mógłby rozjechać się z właścicielami. `SessionManager::add` zwraca dotychczasowe capacity_reached przed alokacją modelu i przesunięciem granicy ID. Konstruktor SessionManager(capacity) zachowuje równe limity sesji/użytkowników.

`SessionManager::identify` sprawdza stan/czas/tożsamość i dotychczasowe kolizje SID/CID/nicka, po czym przy pełnym rejestrze zwraca nowe typowane RegistryError::user_capacity_reached przed alokacją indeksów i mutacją modelu. UserRegistry i NickManager mają efektywną pojemność użytkowników. Wstawianie indeksów pozostaje transakcyjne; błąd alokacji wycofuje rezerwacje, umożliwiając ponowienie na dostępnym miejscu. Zarejestrowany użytkownik może aktualizować dane/nick przy pełnym rejestrze bez zajmowania drugiego miejsca.

LoginHub sprawdza users_full po początkowym zweryfikowanym INF i kontroli duplikatu CID, przed zatwierdzeniem gościa i przygotowaniem powitania/listy. Wysyła stałe `ISTA 211 Hub\suser\slimit\sreached`, własne IQUI i ograniczone opróżnienie/zamknięcie. [ADC STA](https://adc.sourceforge.io/ADC.html#_sta) przypisuje 11 do Hub full i severity 2 do fatalnego rozłączenia. Odrzucony użytkownik nie otrzymuje powitania/listy, nie jest rejestrowany ani ready i nie daje publicznego join/quit. Manager powtarza bramkę przy rejestracji, chroniąc bezpośrednich wywołujących. Pojedynczy wątek reaktora zapewnia sekwencyjność sprawdzenia i zatwierdzenia bez wyścigu przyjęć. Weryfikacja tożsamości/parsowanie mogą wcześniej alokować; nie jest to nowy limiter przed Tiger.

Miejsca sesji obejmują accepted, negotiating, identified, ready, closing i zakończone modele aż do remove. Miejsca użytkowników obejmują identified oczekujących na powitanie, ready i rejestracje closing/closed aż do remove. Utrata gotowości, rozpoczęcie końcowego opróżnienia i samo zakończenie cyklu życia nie zwalniają pojemności. Historyczna gotowość i pending per-IP pozostają niezależne. UserDisconnect zwalnia protokół/SID, kolejkę i publiczne indeksy w dotychczasowej kolejności; usunięcie modelu oddaje odpowiadające miejsce. Błąd anonimowego połączenia zwalnia tylko miejsce sesji. Odrzucone przyjęcie nie zajmuje miejsca użytkownika.

## Dotychczasowe zabezpieczenia i ograniczenia

Nadal działają limity aktywnych/pending per-IP, globalny pending, tempo przyjęć/tożsamości/ramek, łączne budżety negocjacji, bajty wyjścia i terminy cyklu życia. Mogą one odrzucić połączenie przed nową bramką użytkowników; podniesienie jednego pułapu nie wyłącza pozostałych. CID pozostaje unikalny, a lokalne SessionId nie są używane ponownie. Etap nie wprowadza reguł wielu sesji konta, trwałych kwot, automatycznego skalowania ani miejsc zarezerwowanych dla operatora.

Domyślny runtime pozostaje EN/US. languages/us i languages/pl zawierają nowe ostrzeżenie limitu sesji, diagnostykę walidacji i pomoc. Opisy statusów ADC pozostają stałe EN/US, bez treści klienta. Nie dodano zależności, migracji SQL, pola JSON API ani funkcji WebAdmin. Zaktualizowano metadane/dane testowe wersji; istniejące liczniki API active/accepted służą integracji.

## Weryfikacja

`users.limits` sprawdza zakresy/wartości domyślne/efektywne pułapy, programowe odrzucenie przed ograniczeniem, stary konstruktor, odmowę sesji bez zużycia ID i limity użytkowników w identified/ready/closing/closed. Dowodzi odmowy bez alokacji i częściowych indeksów, rezerwacji do remove, rename/kolizji przy pełnym rejestrze, rollback/retry alokacji z zachowaniem innego użytkownika i odmowy LoginHub przed powitaniem/listą ze stabilnym przypiętym buforem innego odbiorcy. Późniejsze przyjęcie odzyskuje miejsce bez odrzuconych/starych użytkowników na liście.

`users.limits.process` używa prawdziwych mieszanych gniazd ADC/ADCS. Osobno ustawia jako niższy limit połączeń i sesji, utrzymuje anonimowe TLS, odrzuca gniazda obu nasłuchów bez przyjęcia TLS, sprawdza API active/accepted/security i ograniczane ostrzeżenia, potem potwierdza sprzątanie/ponowne użycie. Limit użytkowników wielokrotnie daje ISTA 211 → własne IQUI bez publicznego join/quit, zachowuje działanie istniejących klientów przy pełnym rejestrze, zamyka właściciela i ponownie przyjmuje ten sam CID/nick z nowym SID. Każde przyjęte życie ma jedno zdarzenie końcowe; prywatne odrzucone nicki nie trafiają do logu.

Testy konfiguracji obejmują zakresy INI/CLI, duplikaty/brak/błędne wartości, pierwszeństwo, programowe Settings i pomoc/ostrzeżenie US/PL. Testy Network odrzucają błędne surowe limity. Rzeczywisty ncdc ADC/ADCS pracuje z max_sessions=3/max_users=2, z obserwatorem zapełnia limit użytkowników, aktualizuje dane i łączy się ponownie po sprzątaniu. Zachowano regresje security/pending, timeoutów, list/aktualizacji, rozłączeń, MariaDB, API i Apache. Błędy C++ wstrzykiwane są w badane alokacje rejestru, bez dowolnych awarii systemu/pozostałych alokatorów. [TESTING.md](TESTING.md) podaje lokalne polecenia/wyniki.
