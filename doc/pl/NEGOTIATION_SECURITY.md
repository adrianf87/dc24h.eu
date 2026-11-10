# Bezpieczeństwo negocjacji — alpha-0.2.10

**Bieżący etap — alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

Etap dodaje łączne budżety wejścia początkowego logowania, limity oczekujących przyjęć i wspólne bramki przed weryfikacją tożsamości. Podstawa: main `a3ca70d65a0fcbb563147e8a44a1ce2669a0ec53`, po scaleniu STA/QUI alpha-0.2.9. Historia zostaje zachowana, a gałąź nie scala się automatycznie. Protokół ADC, model gości, API v1 i schemat bazy pozostają dotychczasowe.

## Limity i konfiguracja

Nowe opcje są dodatnimi liczbami całkowitymi w INI `[security]`; brak wpisu oznacza wartość domyślną. Nie dodano przełączników CLI.

| Opcja | Domyślnie | Zakres | Zakres działania |
| --- | --- | --- | --- |
| max_pending_logins | 256 | 1–65535 | Wszystkie przyjęte, ale niegotowe gniazda TCP/ADCS reaktora |
| max_pending_per_ip | 64 | 1–65535 | Oczekujące gniazda jednego rzeczywistego kanonicznego IP |
| max_login_bytes | 65536 | 1–16777216 | Łączne jawne bajty wejścia oczekującego połączenia |
| max_login_frames | 64 | 1–65535 | Łączne wyniki dekodera oczekującego połączenia |
| max_identity_checks_per_minute | 64 | 1–100000 | Próby początkowego INF na IP w stałej minucie |
| max_identity_checks_per_second | 128 | 1–100000 | Próby początkowego INF w całym hubie w stałej sekundzie |

Nadal działają dotychczasowe limity połączeń/przyjęć, częstości i błędnych ramek, bezwzględne terminy TLS/logowania/ramki, limity wyjścia i końcowego zamykania. Obowiązuje minimum nakładających się limitów; zwiększenie pending ponad max_connections nie zwiększa całkowitej pojemności. To lokalna polityka zasobów, nie limity ADC ani gwarancja wydajności produkcyjnej.

## Przyjęcia i własność gotowości

PeerLimiter sprawdza globalną i indywidualną pojemność oczekujących sesji przed alokacją TLS i rejestracją gniazda. Odrzucenie zamyka gniazdo bez odpowiedzi protokołu i bez kontekstu TLS, zwiększa istniejące rejected/security_rejected i nie zajmuje nowej rezerwacji. Dotychczasowy limit przyjęć IP nadal liczy udane przyjęcia aplikacyjne.

Każde przyjęte TCP/TLS trzyma miejsce oczekujące, obejmując nieukończony TLS, PROTOCOL, IDENTIFY i końcowe opróżnianie przed gotowością. Poprawny CID/PID lub wewnętrzne NORMAL nie zwalnia miejsca. Zwolnienie następuje dokładnie raz po pełnym wysłaniu własnego końcowego początkowego INF, wraz z login_ready. Rozłączenie gotowej sesji zwalnia aktywne miejsce bez zużycia oczekującego miejsca innego klienta. Nieudana inicjalizacja/rejestracja, EOF, fatalny raport, odrzucenie, timeout, nieudane powitanie i shutdown sprzątają rezerwację przy faktycznym zamknięciu.

Mapa PeerLimiter nadal ma limit 65536 kluczy źródłowych. Klucz pochodzi z numerycznego adresu jądra; normalizacja IPv4-mapped IPv6 jest wspólna dla ADC/ADCS. Deklarowane I4/I6, nick, CID i PID nie wybierają innego licznika. Klucze pozostają tylko w pamięci. Gotowi użytkownicy zachowują aktywne miejsce, ale nie zajmują oczekującego.

Biblioteka ma jednego właściciela/wątek. complete_login wywołuje się raz dla ukończonego oczekującego przyjęcia, a release(peer, negotiating) dokładnie raz zgodnie z historią gotowości sesji. Transport przechowuje tę historię we fladze połączenia. Klient sieciowy nie ma dostępu do manipulowania tym API.

## Łączne wejście do końca logowania

NegotiationBudget ma osobne liczniki bajtów/ramek i trwałą flagę błędu, bez zegara, okna i resetowania, z kontrolą przepełnienia. Bajty obejmują końcowy LF, częściowe ramki, błędną składnię i dane odrzucane po przekroczeniu długości. Bieżący segment jest naliczany przed alokacją/parsowaniem dekodera. Segment ponad pozostały limit nie jest parsowany; już odczytane bajty nadal liczą się jako bajty transportu.

Każdy wynik dekodera — poprawna komenda, błąd lub keepalive — zużywa jednostkę przed obsługą przyjęcia. Fragmentacja, łączenie ramek, keepalive, oczekiwanie na nową sekundę i przejście PROTOCOL→IDENTIFY nie odnawiają budżetu całego logowania. Ogranicznik częstości pozostaje osobną pierwszą bramką. Naliczanie kończy dopiero pełna gotowość, nie zatwierdzenie SUP/SID/tożsamości. Zamykanie nie pozwala dalej negocjować; obowiązuje dotychczasowe ograniczone kończenie.

Przekroczenie budżetu inicjuje jedno stałe fatalne ISTA 240 (Negotiation resource limit exceeded), a przy przypisanym SID własne IQUI. Zachowane są przypięty początek, rachunek kolejki, brak danych po własnym QUI i close_timeout. Miejsce oczekujące jest trzymane do zakończenia zamykania. Niesprawny/przepełniony transport może zamknąć się bez doręczenia. Nie dodano ostrzeżenia na każdy bajt lub ramkę.

## Bramka przed początkową tożsamością

LoginHub udostępnia niezależne od transportu needs_identity_check: żywe, niezamykane IDENTIFY bez zachowanej tożsamości i przychodzące BINF. Reaktor musi uzyskać kredyt IP/minutę i hub/sekundę przed wywołaniem ClientSession/INF/Tiger. Sam predykat nie waliduje kandydata i nie zatwierdza logowania.

Kredyty liczą próby początkowego INF, także błędną tożsamość, źródło/pola, niezgodną parę Tiger i późniejszy konflikt nick/CID; nie liczą wyłącznie udanych hash ani haseł kont. Bramka celowo poprzedza semantykę. Aktualizacja INF w NORMAL nie wymaga nowego kredytu: późniejsze PID jest zabronione, CID niezmienny. Samodzielne niższe moduły ADC nadal są walidatorami bez polityki limitów sieciowych; osadzający musi zapewnić własną kontrolę przyjęć.

Okno minuty należy do IP, nie gniazda. Przeżywa nieudane logowanie, gotowość, rozłączenie i reconnect. Nieaktywny wpis jest usuwany dopiero po wygaśnięciu okna przyjęć i tożsamości; starsze okno przyjęć nie może skasować nowszego okna weryfikacji. Aktywne wpisy są zachowane. Globalna sekunda obejmuje wszystkie IP i transporty. Odmowa IP nie zużywa kredytu globalnego, odmowa globalna nie zużywa próby IP ani nie zaczyna/resetuje jego minuty. Minuta zaczyna się przy pierwszej naliczonej próbie. Odmowy nie przesuwają aktywnego okna.

To stałe okna: ruch przy granicy może wykorzystać dwa sąsiednie okna. Użytkownicy za wspólnym IP/NAT dzielą minutę; wartości trzeba dobrać do wdrożenia. Etap nie dodaje trwałych banów, blokady haseł/kont, dodatkowego dowodu własności tożsamości, limitera rozproszonego ani usługi reputacji.

Odmowa nie dociera do pracy INF/Tiger. Powoduje stałe fatalne ISTA 240 (Identity verification rate limit exceeded), własne IQUI przy przypisanym SID i ograniczone sprzątanie. PID, deklarowana tożsamość, IP i hash odpowiedzi nie trafiają do opisu lub logów. Brak automatycznej porady retry/bana/przekierowania.

## Obserwacja i testy

Nowe stałe zdarzenia INFO: negotiation_limit i identity_limit, z istniejącym budżetem opcjonalnych zdarzeń. Osobne pięciosekundowe ostrzeżenia mają katalogi US/PL. Opisy sieciowe pozostają angielskie. Podsumowanie startu pokazuje sześć opcji bez sekretów. security_closed liczy dane połączenie raz również przy zbiegu limitów bezpieczeństwa i wyjścia. API/WebAdmin i schemat JSON pozostają bez zmian; nie ujawniają liczników IP ani nie udają licznika gotowych użytkowników.

security.negotiation sprawdza dokładne granice bajtów/ramek, trwały błąd, fragmentację, przepełnienie size_t, własność miejsc, limity globalne/IP, brak alokacji przy odmowie, granice sekund/minut i reconnect/pruning ze sztucznym zegarem. security.negotiation.process używa rzeczywistych TCP/OpenSSL: oczekujące przyjęcia z niedokończonym TLS, zwolnienie po gotowości, łączne limity mimo czasu, dokładne poprawne logowanie, normalny ruch po gotowości, błędne INI, wspólne limity tożsamości, izolacja różnych IP i odmowa przed Tiger (240 zamiast 227 niezgodnej pary).

adc.security.ncdc i adcs.security.ncdc używają tymczasowego rzeczywistego ncdc: jedna nieudana para zużywa kredyt, ncdc wykorzystuje pozostały do NORMAL, normalna komenda nadal działa, a reconnect otrzymuje komunikat limitu bez kolejnego login_ready. Dotychczasowe testy klientów, blokowanego zapisu/terminu zamykania, MariaDB, Apache i WebAdmin pozostają wymagane. Historyczne testy transportu utrzymują wejście przed logowaniem w nowym budżecie; duży strumień TLS najpierw kończy logowanie. Asercja prywatności bada granicę pola, nie przypadkowe XX wewnątrz poprawnego CID base32.

Pełna konfiguracja rejestruje 44 testy CTest. To lokalne deterministyczne testy, nie benchmark, gwarancja DDoS, pełne wymuszanie alokacji/TLS, audyt side channel lub potwierdzenie pozostałych OS. Konta/uprawnienia/bany, czat/wyszukiwanie/transfery, TLS close_notify/reload i produkcyjna administracja pozostają osobnymi etapami.

Podstawa: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), szczególnie negocjacja, identyfikacja i stany; [STA/QUI](ADC_STATUS.md) i [logowanie gości](ADC_LOGIN.md) opisują dotychczasową kolejność wysyłki i sprzątania.
