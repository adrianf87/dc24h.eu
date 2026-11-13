# Active/Passive — alpha-0.5.10

**Bieżący etap — alpha-0.5.11:** [Search Security](SEARCH_SECURITY.md) dodaje limity liczby i bajtów zapytań oraz wysyłanych/odbieranych wyników na czas życia połączenia, ograniczone powiadomienia, politykę gości/globalną, zaufane uprawnienia sesji i konfigurowalne limity aktywnych wyszukiwań. Kontrola poprzedza routing/rejestrację tokenu. Opcjonalny wymagany token uzupełnia kontrolę właściciela/generacji; bezpośrednie UDP klientów jest poza kontrolą huba.

Weryfikacja lokalna 2026-11-13: **114/114 zestawów CTest przeszło w każdej konfiguracji Debug, Release i ASan/UBSan**, z wymaganymi narzędziami i domyślną kontrolą wycieków. Zakres obejmuje niezależne tryby odpowiedzi, atomową projekcję adresów, IPv4/IPv6 ADC/ADCS, rzeczywiste wysyłanie/odbieranie UDP i powrót do huba w ncdc, MariaDB oraz Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, 4 testy Node, 6 testów Chromium i integracja rzeczywistego API C++. Sprawdzono metadane, UTF-8, zgodność języków/linków dokumentacji, składnię zmienionych plików Python, przykładowe INI i białe znaki. Wyniki są lokalne; zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.

Gałąź utworzono bezpośrednio z main `3655cf9175de72fac1ef2e4e3bfdd5ff023469a5`, po scaleniu alpha-0.5.9. Zachowano poprzedni kod i gałęzie; ten etap publikowany jest osobno od main.

## Drogi odpowiedzi wyszukiwania

W [ADC](https://adc.sourceforge.io/ADC.html) możliwości odbierania TCP i UDP są niezależne, osobno dla IPv4 i IPv6. Zapytania nadal przechodzą przez hub według dotychczasowych reguł B/F/D/E SCH. Klient wybiera transport odpowiedzi na podstawie aktualnego publicznego INF wyszukującego: DRES przez hub albo URES bezpośrednio na port UDP klienta. Pasywny odpowiadający klient może wysłać UDP bez ogłaszania własnej możliwości odbierania UDP. Aktywność TCP nie oznacza aktywności UDP ani odwrotnie.

Wydanie udostępnia te deklaracje jako typowane metadane Search Core i sprawdza rzeczywisty ruch klientów. Nie dodaje hubowi nasłuchu/proxy UDP, multicast ani automatycznego wysyłania pakietów. URES jest komunikatem między klientami z nagłówkiem CID, a nie DRES z SID/celem. URES na strumieniu TCP/TLS huba pozostaje poza obsługiwanym profilem. Hub nie może walidować ani liczyć pakietów, które go omijają.

## Model publiczny i metadane odpowiedzi

`users::ProfileData` posiada teraz numeryczne napisy `ipv4` / `ipv6` i opcjonalne 64-bitowe `udp4_port` / `udp6_port`. Konstrukcja profilu sprawdza ograniczoną składnię, rodzinę, brak NUL/strefy/dopisanego portu i zakres portów 0..65535 przed zawężeniem typu. Bajty adresów wchodzą do limitu rozmiaru profilu. Brak lub zero portu nie oznacza użytecznego UDP.

Projekcja ADC `user_from_info` kopiuje I4/I6/U4/U6 wyłącznie ze sprawdzonego ClientInfo. Nadal obowiązuje własność adresu połączenia z jądra: adres zerowy staje się rzeczywistym kanonicznym adresem, a inny IP/rodzina jest odrzucany. Nie dodano zaufanego nadpisania drugiej rodziny. Transakcje INF zatwierdzają razem ClientInfo, User/profil i publiczną zmianę. Anulowanie pustym polem, port zerowy i zmiana SU aktualizują tryb bez ponownego łączenia; błędy danych/alokacji nie publikują częściowej zmiany.

`search/connectivity.hpp` dodaje AddressFamily, ReplyRoute, UdpEndpoint o stałym rozmiarze (bajty sieciowe adresu i port) oraz ReplyPaths. `reply_paths(profile)` nie alokuje pamięci, nie używa DNS, gniazd, zegarów ani testów osiągalności:

| Deklaracja | Model |
| --- | --- |
| Użyteczne I4 + dokładne SU TCP4 | `tcp4=true`; nie oznacza portu UDP. |
| Użyteczne I6 + dokładne SU TCP6 | `tcp6=true`; nie oznacza portu UDP. |
| Użyteczne I4 + dokładne SU UDP4 + U4 1..65535 | Własny opis `udp4`; droga IPv4 to UDP4. |
| Użyteczne I6 + dokładne SU UDP6 + U6 1..65535 | Własny opis `udp6`; droga IPv6 to UDP6. |
| Brak funkcji/adresu/portu albo port zerowy | Dla tej rodziny droga odpowiedzi prowadzi przez hub. |

Nieznana rewizja ani podnapis funkcji nie przyznają obsługi. Adresy nieokreślone, multicast i ograniczony broadcast IPv4 nie tworzą celu; IPv6 ze strefą/link-local lub mapowane IPv4 nie są użytecznymi celami IPv6. Lokalny/prywatny unicast jest poprawny dla lokalnych hubów. Adres numeryczny jest deklaracją, a nie dowodem osiągalności lub poświadczeniem.

`Core::reply_paths(owner)` zwraca bez alokacji własny obraz gotowego User albo brak dla nieistniejącej/niegotowej/zamykanej sesji. `Plan::reply_paths()` zapamiętuje możliwości źródła przy przyjęciu zapytania; kopie/przeniesienia zachowują obraz, a późniejsze INF wpływa na nowe odczyty/plany. Dotychczasowe wybieranie odbiorców SCH, funkcje i oryginalne pola ramki pozostają bez zmian. Metadane przyjęcia nie zastępują aktualnego INF klienta, nie wybierają gniazda huba i nie anulują kolejki po zmianie trybu. Faktyczny transport wybierają klienci; DRES jest dozwolone również do aktywnych UDP.

## Tryb pasywny i UDP

Dotychczasowy ResultRouter nadal waliduje i kieruje DRES do gotowego celu, sprawdza aktywne TO właściciela, gdy podano token, i zachowuje zgodność bez TO. Publiczne UDP nie wyłącza wyników przez hub. Brak lub zero portu pozwala klientowi wrócić do huba bez nowego ustawienia serwera i przepisywania SCH.

Dla aktywnego wyszukiwania hub udostępnia istniejący sprawdzony publiczny adres/port/funkcje i przekazuje SCH. Odpowiadający klient wysyła URES do tego celu ze swoim CID, nazwą/rozmiarem/hashem i skopiowanym TO, jeśli było podane. Utrata/duplikaty datagramów, podszywanie i korelacja po stronie klienta są poza kontrolą przyjęcia TCP huba. Zmiana celu nie cofa datagramu wysłanego na wcześniejszy adres/port.

ADCS chroni połączenie z hubem; **nie szyfruje** osobnego datagramu UDP. Nowe testy jawnie używają zwykłego UDP, także z hubem ADCS. [SUDP](https://adc.sourceforge.io/ADC-EXT.html), NAT, hole punching, sondowanie osiągalności i negocjacja transferów nie są implementowane przez ten etap huba. Nie deklaruje się poufności UDP ani walidacji bezpośrednich wyników przez hub.

## Weryfikacja i zakres

`search.connectivity` obejmuje szesnaście kombinacji TCP4/TCP6/UDP4/UDP6, bajty adresów obu rodzin, granice portów i tryby wyłączone, błędne adresy/strefy/rodziny, nieużyteczne cele, dokładne rewizje funkcji, gotowość sesji, obrazy planów, zmiany/anulowanie INF, odrzucanie spoofingu/portów i odczyt bez alokacji oraz atomowe błędy projekcji.

`search.connectivity.process` używa rzeczywistych sesji IPv4/IPv6 ADC i ADCS z obserwatorem, kanonicznych deklaracji, osobnych gniazd UDP klientów, powrotu do DRES po wyzerowaniu/anulowaniu portu i odrzucania podmienionego IP. Surowe peery emulują klientów; ich datagramy nie przechodzą przez parser huba. Dotychczasowe testy funkcji, wyników, tokenów, sesji, kolejek i prywatności pozostają włączone.

`adc.activepassive.ncdc` i `adcs.activepassive.ncdc` korzystają z rzeczywistego zindeksowanego udziału ncdc. Pasywny ncdc wysyła URES na lokalne gniazdo UDP aktywnego wyszukującego z poprawnym CID/tokenem/plikiem/hashem. Port zerowy wyszukującego przywraca DRES przez hub. Następnie ten sam ncdc przechodzi w tryb aktywny: ogłasza port, wysyła zapytanie z tokenem i wyświetla odebrany wynik UDP. Pozostają wcześniejsze próby pasywne, ponowne łączenie i funkcje. Rzeczywisty klient testowany jest z IPv4; surowe peery obejmują IPv6.

Nie dodano opcji CLI/INI, endpointu API, migracji, komunikatu operatora ani kontrolki WebAdmin. Metadane sieciowe są w publicznym INF i wewnętrznych modelach Users/Search, poza logami, licznikami czatu i administracyjnym statusem. Zachowano EN/US kodu, dokumentację US/PL i `languages/us` / `languages/pl`. Testy lokalne nie potwierdzają obciążenia produkcyjnego, NAT/firewall ani pełnej macierzy systemów/klientów.
