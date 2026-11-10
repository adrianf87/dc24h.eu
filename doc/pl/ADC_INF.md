# INF — alpha-0.2.6

**Bieżący etap — alpha-0.3.7:** [User Registry](USER_REGISTRY.md) utrzymuje ograniczone indeksy SID/CID i odwrotny indeks właścicieli prywatnie w SessionManager. Przyjęcie jest transakcyjne między tym rejestrem, NickManager i modelem Session. Wyszukiwanie User tylko do odczytu wskazuje z obu kluczy ten sam aktualny model; rezerwacje trwają do usunięcia sesji. Starsze opisy modułów zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

**Bieżący etap — alpha-0.3.3:** [User Core](USER_CORE.md) dodaje do Session publiczny model gościa i profilu, projekcję całego zwalidowanego INF oraz aktualizacje przez bramkę cyklu życia. Powstał bezpośrednio z main z Session Core; main łączy go teraz z wcześniej scalonym alpha-0.3.2 Session Manager. Starsze opisy zachowują zakres historyczny.

**Aktualny etap — alpha-0.3.1:** [Session Core](SESSION_CORE.md) dostarcza niezależny model Users i trwałe w obrębie reaktora ID sesji, podłączone do rzeczywistej gotowości, zamykania i czasów. Wcześniejsze opisy zachowują zakres pierwotnego etapu.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Od alpha-0.2.7 [weryfikacja CID/PID](ADC_IDENTITY.md) jest osobnym wspólnym modułem z TigerCid/VerifiedIdentity, bezpośrednimi czyszczonymi buforami i stałoczasowym końcowym porównaniem skrótu. Snapshot INF zachowuje wyłącznie publiczny wynik weryfikacji. Podstawowa obsługa z alpha-0.2.6 pozostaje zgodna; transport i polityka kont/rejestru są osobne.

## Zakres etapu

`adc/inf.hpp` dodaje InfValidator, snapshot ClientInfo tylko do odczytu i ClientSession nad [SidSession](ADC_SID.md). `adc/identity_hash.hpp` udostępnia Tiger/192 i kanoniczne base32. To moduły C++20 w dc24h_adc, bez nowych zależności od gniazd, logowania, SQL i HTTP. Produkcyjny Network nadal używa samego parsera, więc etap nie włącza logowania w sieci.

Źródła: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), identyfikacja/stany/INF; [rozszerzenie TIGR](https://adc.sourceforge.io/ADC-EXT.html); [algorytmy Libgcrypt](https://www.gnupg.org/documentation/manuals/gcrypt/Available-hash-algorithms.html). Implementacja używa GCRY_MD_TIGER1: standardowego Tiger/192 z kolejnością NESSIE, nie historycznej kolejności GCRY_MD_TIGER, Tiger2 ani drzewa Tiger. Do kompilacji zainstalować libgcrypt20-dev. Brak algorytmu/kontekstu odrzuca walidację; nie zmienia się globalnych trybów/FIPS i nie podstawia innego hash.

Moduł sprawdza własne metadane klienta i związek PID/CID. Nie zapewnia globalnej unikalności nicku/CID, własności konta, uprawnień, haseł ani routingu. Właściciel nadal podejmuje decyzję dopuszczenia przed zatwierdzeniem tożsamości. Nawet poprawne składniowo udziały/sloty/prędkości pozostają niezaufaną deklaracją.

## Pierwsza tożsamość i publiczne dane

Lokalny pierwszy BINF wymaga NI, ID i PD. ID/PD muszą mieć 39 wielkich znaków base32 bez paddingu dla 24 bajtów, z trzema zerowymi niewykorzystanymi bitami końca. Małe litery, padding, zła długość i alternatywne kodowanie z niezerowymi bitami są odrzucane. Hash odkodowanego PID musi być równy CID. Posiadanie poprawnej pary PID/CID nie uwierzytelnia konta.

PD jest pożyczony z odkodowanego Message właściciela i nigdy nie trafia do ClientInfo. Tymczasowe odkodowane bajty PID są czyszczone; kontekst Libgcrypt zamykany. Nie obiecujemy wymazania oryginalnego Message, buforów dekodera i wszystkich tymczasowych kopii kompilatora/bibliotek. Wejście należy do wywołującego; nie wolno logować/przekazywać surowego pierwszego INF. Błąd nie zawiera wartości, nicku, adresu ani PID. Publiczny snapshot zawiera tylko rozpoznane poprawne pola i nie może być domyślnie utworzony jako niesprawdzony poprzedni snapshot.

`ClientInfo::field(name)` zwraca opcjonalny pożyczony string_view. `integer(name)` podaje sprawdzony int64 tylko dla znanych liczb. `bytes()` liczy zachowane nazwy i odkodowane wartości. `public_message(Sid)` tworzy własny BINF Message z positional_count 0, posortowanymi polami i bez PD/nieznanych rozszerzeń. Wywołujący podaje SID właściciela. To dane odkodowane, nie serializacja sieciowa: przyszły serializer musi kodować spacje/backslash. Widoki nie przeżywają podmiany/zniszczenia snapshotu; kopie i eksportowane Message posiadają dane.

## Sprawdzany profil

To jawne reguły lokalne, nie deklaracja obsługi każdego pola/rozszerzenia ADC.

| Pola | Reguły |
| --- | --- |
| NI | Obowiązkowe; 1..64 bajty UTF-8, NFC, codepoint powyżej spacji ASCII; bez kontrolnych/formatujących/separatorów linii i akapitu Unicode |
| ID | Obowiązkowy kanoniczny CID długości Tiger; stały po pierwszej walidacji; aktualizacja tą samą wartością dozwolona |
| PD | Obowiązkowy początkowo; nieobecny w snapshotach i zabroniony w późniejszych aktualizacjach |
| I4 / I6 | Numeryczny adres tej samej rodziny i zgodny z rzeczywistym klientem; zerowy placeholder staje się jego kanonicznym adresem |
| U4 / U6 | Liczby 0..65535; zero nie potwierdza zdolności nasłuchu |
| SS, SF, US, DS, SL, AS, AM, HN, HR, HO, FS | Nieujemne signed 64-bit; brak sprawdzania prawdziwości/zależności; FS jako opcjonalna informacja |
| AW | Liczba 0..2 |
| CT | Klient może deklarować 0 lub informacyjny bot 1; rejestracja/operator/właściciel/hub/inne bity odrzucane, brak przyznania ról |
| SU | Do 128 różnych czteroznakowych funkcji po przecinku; bez pustych pozycji, końcowego przecinka, duplikatów i małych liter; niezależne od SUP |
| DE / RF | Do 2048 bajtów każde; RF jest biernym tekstem, bez pobierania URL |
| AP / VE | Do 128 bajtów każde |
| EM | Do 254 bajtów; ograniczony tekst, nie weryfikator składni email |
| LO | Do 64 bajtów |

Znany tekst wymaga poprawnego NFC UTF-8 i odrzuca kontrolne ASCII/DEL oraz kontrolne/formatujące/separatory linii i akapitu Unicode. Poza NI dozwolone są zwykłe spacje ASCII. Profil jest ostrzejszy od samego minimum codepoint protokołu. Liczby używają parse_integer: bez plusa/ułamka/overflow, dopuszczają zera wiodące i ujemne zero. Anulowanie to sama dwuznakowa nazwa. Pola opcjonalne są usuwane; NI/ID nie mogą zniknąć. Anulowanie PD nadal jest zabronionym wystąpieniem po pierwszym INF.

Konstruktor InfValidator otrzymuje zaufany numeryczny adres kernela, bez portu, nawiasów, nazwy hosta i strefy. Peer IPv4-mapped IPv6 jest normalizowany do IPv4. Deklaracja musi być tej samej rodziny; inna jest odrzucana nawet jako zero. Wąski profil nie ma wyjątku dla zaufanych osób ani osobnej usługi potwierdzającej adresy dual-stack. Anulowanie IP usuwa metadane, nie zaufany kontekst. Bezpieczeństwo transportu nadal opiera się na rzeczywistym adresie gniazda.

Nieznane nazwy są ignorowane i nieeksportowane. Duplikaty odrzucają cały INF, także dla nieznanych nazw, zamiast wybierać pierwszą/ostatnią wartość. Parser nadal zachowuje tokeny; to polityka konsumenta. Puste opcjonalne pola usuwają poprzednią wartość; nieobecne ją zachowują. Pełnego snapshotu nie buduje się wyłącznie z ostatniej aktualizacji.

## Limity i walidacja transakcyjna

Wejściem jest odkodowany BINF z parsera. Dodatkowo sprawdzane są komenda/typ/źródło, brak celu/selektorów F, do 128 parametrów, nazwy, duplikaty i limity tokenów przy ręcznym Message. Walidator nie odtwarza całego ramkowania/escape. Wywołujący przekazuje faktycznie powiązany SID i wynegocjowany SessionHash; nieobsługiwany hash jest odrzucany.

Zachowane nazwy/wartości mają razem do 4096 bajtów, plus ograniczony narzut mapy dla stałego zbioru znanych pól. SU ma do 639 bajtów. Pola mają własne limity; parser utrzymuje granicę ramki 16 KiB. Nieznane wartości nie są kopiowane. Zmiany dotyczą własnego kandydata, z kontrolą łącznego budżetu po każdej zapisanej zmianie. Przy granicy najpierw usunąć stare opcjonalne pole. Błąd nie zmienia poprzedniego snapshotu, także po wcześniejszych poprawnych polach w tej ramce. Wyjątek alokacji propaguje się przed zmianą stanu sesji.

ID jest stały po pierwszej walidacji. Nowy PID nie zmienia tożsamości w miejscu; potrzebna jest nowa sesja i walidacja początkowa. Sam InfValidator nie zna fazy połączenia. Argument previous musi należeć do tej sesji, nie innego klienta. Globalna unikalność zmian nicku/CID należy do przyszłego rejestru i musi poprzedzać zatwierdzenie/ogłoszenie w hubie.

## Kontrakt sesji

ClientSession wymaga wspólnej SidPool, zaufanego adresu i opcjonalnej SupportPolicy. Jest niekopiowalny/nieprzenoszalny, należy do wątku puli i nie daje modyfikowalnego dostępu do niższych warstw. Przygotowanie SUP/SID i kolejność ukończenia wysyłek są dotychczasowe.

Przychodzący INF jest dozwolony tylko w IDENTIFY i NORMAL. Adapter sprawdza kandydata przed obserwacją automatu, a snapshot przenosi dopiero po przyjęciu. Błędny/duplikowany/niespójny INF nie ustawia oczekującej tożsamości ani nie nadpisuje metadanych. Poprawny pierwszy INF pozostawia IDENTIFY. `approve_identity(mode)` dodatkowo wymaga snapshotu, ale wywołujący musi wcześniej sprawdzić unikalność, bany, politykę kont/gości i inne warunki. Ścieżka hasła nadal wymaga zewnętrznego uwierzytelniania. Aktualizacje NORMAL są atomowe; VERIFY je odrzuca. Parsowanie ani walidacja nie przyznają NORMAL automatycznie.

Wychodzący INF z nazwanym PD jest dodatkowo odrzucany; używać publicznego snapshotu. To nie pełna kontrola/serializacja wyjścia. Terminalne własne QUI, fatalny status i disconnect usuwają metadane i zwalniają SID przez niższą sesję; zdarzenia innych osób nie. Brak zmian HTTP/liczników, SQL, komunikatów operatora i konfiguracji.

`observe` zwraca opcjonalny variant<StateError, SupportError, SidError, InfError>. Stałe InfError: invalid_message, wrong_source, duplicate_field, missing_identity, invalid_cid, invalid_pid, cid_mismatch, invalid_nick, invalid_text, invalid_integer, invalid_ip, invalid_features, privilege_claim, field_limit, immutable_identity, unexpected_pid, unsupported_hash i crypto_unavailable. Proponowane fatalne statusy: 227 dla PID/hash, 221 dla nicku, 246 dla IP, 247 dla nieobsługiwanego hash, 210 dla braku dostawcy i 243 dla pozostałych pól. Funkcja nie wysyła STA/disconnect i nie zawiera wejścia. Właściciel musi ograniczać ponowienia i odpowiednio zamykać połączenia.

## Weryfikacja i dalsze prace

`adc.inf` sprawdza stałe wektory Tiger empty/abc, kolejność bajtów, 1000 rund base32 i bity końca; wymagane/duplikowane/prywatne/nieznane pola; limity liczb/tekstu/funkcji/bajtów; placeholder/normalizację/podszywanie IP; stałą tożsamość, anulowanie, własność eksportu, atomowe aktualizacje i stany gościa/hasła. Testy strumienia obejmują każdy podział na dwa fragmenty i bajtowy UTF-8 z odrzuceniem semantycznym i odzyskiwaniem parsera.

`adc.inf.ncdc` używa prawdziwego ncdc w izolowanym fixture z --inf. ClientSession C++ negocjuje SUP, posiada przydzielony SID i sprawdza rzeczywisty PID/CID/BINF, eksportuje bez PD i pozostaje w niezatwierdzonym IDENTIFY. TCP i TLS Pythona przekazują osobne ukończenia wysyłek i sprzątają sesję. Nie dowodzi to podłączenia Network C++ ani pełnego logowania gościa. Dotychczasowy rzeczywisty TCP/OpenSSL i fixture stanów/SUP/SID są osobne.

Nadal wymagane są ograniczone uporządkowane wyjście/preflight, escape, transakcje rejestru/unikalności/dopuszczenia/zmiany nicku, cykl listy osób/QUIT, uwierzytelnianie kont i termin całego logowania. Hash Tiger dla tożsamości jest zaimplementowany; drzewa TTH plików i usługa haseł nie. Zobacz [TESTING.md](TESTING.md), [SECURITY.md](SECURITY.md) i [ROADMAP.md](ROADMAP.md).
