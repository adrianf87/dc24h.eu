# Bezpieczeństwo — alpha-0.1.7

Etap dodaje lokalne limity zasobów do TCP/ADCS i parsera składni ADC. Nadal nie ma uwierzytelniania, tożsamości sesji, ról ani pełnego logowania ADC. Uruchamiaj jako zwykły użytkownik; domyślny nasłuch pozostaje lokalny, adres publiczny/wildcard wymaga jawnej konfiguracji.

## Zaimplementowane limity

| Ustawienie | Domyślnie | Zakres |
| --- | --- | --- |
| Aktywne połączenia na kanoniczny źródłowy IP | 128 | 1..65535 |
| Przyjęcia IP w stałym oknie minuty | 256 | 1..100000 |
| Wyniki dekodera na połączenie w stałym oknie sekundy | 1024 | 1..100000 |
| Błędne ramki w czasie życia połączenia | 512 | 1..65535 |
| Bezwzględny termin negocjacji TLS | 10 sekund | 1..3600 |
| Bezwzględny termin niedokończonej ramki ADC | 10 sekund | 1..3600 |

Nadal obowiązują globalny limit klientów (1024), bezczynność (120 sekund), 16384 bajty treści ramki i 128 parametrów. Są to zasady dc24h, nie limity [ADC 1.0.4](https://dc-protocols.github.io/ADC.html). Pojedyncze błędne ramki są ignorowane z możliwością resynchronizacji; kolejne mogą wyczerpać limit błędów i zamknąć klienta.

Sześć opcji jest dostępnych w `[security]` i CLI; zobacz [CONFIGURATION.md](CONFIGURATION.md). Zero nie wyłącza ochrony. `--check-config` sprawdza zakresy bez portów/logu. Konstruktor Network sprawdza też ustawienia przekazane przez kod C++.

## Przyjmowanie i pamięć

Po accept reaktor sprawdza limit globalny, a potem wspólny limiter IP **przed utworzeniem sesji TLS i dodaniem watch klienta**. Obowiązuje na wszystkich portach TCP/TLS. Nieukończone TLS zajmuje slot. Zamknięcie/błąd/timeout/stop go zwalnia; nieudane tworzenie sesji lub watch także zwalnia licznik aktywnych. Przyznane przyjęcie nadal liczy się w minucie, nawet jeśli późniejsza inicjalizacja zawiedzie. Odrzucenia nie odnawiają okna ani nie liczą się jako przyjęcia. Kernel może zakończyć handshake TCP przed odrzuceniem przez aplikację.

IP pochodzi z sockaddr systemu, nie z pól ADC klienta. IPv4-mapped IPv6 normalizuje się do IPv4; nasłuch IPv6 nadal ma IPV6_V6ONLY. Różne tekstowe zapisy tego samego adresu nie tworzą nowych kluczy. Pamięć przechowuje najwyżej 65536 wpisów, klucz do 48 bajtów; nowy adres przy pełnej mapie jest odrzucany. Przy przyjmowaniu, najwyżej raz na pięć sekund, usuwane są nieaktywne wpisy z wygasłym oknem minuty. Aktywne pozostają. Nie ma trwałych banów ani tożsamości kont.

Okna zaczynają się od pierwszego przyjęcia/wyniku i odnawiają na żądanie. Są stałe, nie kroczące ani token bucket; na granicy możliwy jest większy chwilowy ruch. Klienci za NAT/proxy współdzielą budżet IP; limity trzeba dostosować. Nagłówki proxy i zaufana tożsamość reverse proxy nie są obsługiwane.

## Ramki i terminy

Każdy wynik dekodera zużywa budżet: poprawne komendy, nieznane rozszerzenia, keepalive i błędy. Zbyt długa ramka generuje jeden błąd, a dane są odrzucane do LF bez wzrostu bufora 16 KiB. Przy max_invalid_frames=N N-ty błąd zamyka połączenie; poprawne ramki nie zerują licznika. Wynik ponad tempo nie trafia do zwykłych liczników ADC. Reaktor podaje dekoderowi najwyżej jeden segment ramki na wywołanie i przestaje przetwarzać resztę odczytu po przekroczeniu limitu. Wynik przekraczający limit mógł już być sparsowany, ale dalsze ramki nie są parsowane. Brak kolejki ramek.

TLS ma termin od przyjęcia; dosyłane bajty go nie odnawiają. Efektywny czas ograniczają termin TLS i bezczynność. Niedokończona ramka ma termin od pierwszych zaobserwowanych bajtów, także w stanie odrzucania za długość. Dalsze bajty nie przedłużają go. Ukończenie zeruje timer; następna częściowa ramka z tego samego odczytu dostaje własny start. Kontrola następuje przed/po I/O i przed segmentami, więc spóźnione dane nie odnawiają wygasłego terminu. Osobny timer bezczynności mierzy aktywność aplikacji. Przegląd wygasania działa też przy pętli oczekującej do 250 ms; CPU/dysk/planista mogą dodać opóźnienie.

Fragmentacja między odczytami i rekordami TLS jest dozwolona. Pozostają walidacja UTF-8/NFC, nagłówków/escape, odrzucanie NUL/control i limit parametrów. Przy końcowym EOF można doliczyć ucięcie raz, gdy klient już jest usuwany, bez kolejnego żywego slotu tempa ramek.

## Obserwowalność i prywatność

Atomowy `security_rejected` zlicza odrzucenia przez IP/pojemność mapy, zawarte też w Network rejected. `security_closed` obejmuje limit ramek/błędów i terminy TLS/częściowej ramki, także bezczynność podczas nieukończonego TLS. Zwykła bezczynność, I/O klienta, EOF i stop nie zwiększają go. Końcowy INFO ma postać `Security stopped; rejected=N closed=N`. Pola atomowe są odczytywane niezależnie.

Ostrzeżenia limitów przyjęć, ramek i częściowej ramki są ograniczone częstotliwością, bez źródłowych IP i danych. Opcjonalne zdarzenia INFO dodają `security_quota`, `partial_frame_timeout` i używają istniejącego `handshake_timeout`. Liczniki i WARN mogą być widoczne przy wyłączonych szczegółowych zdarzeniach. Nie loguje się tekstu ADC, nicków, CID/PID, haseł/hashów ani IP klientów. Gotowość portu nadal pokazuje lokalny adres bind. Zobacz [LOGGING.md](LOGGING.md).

## Pozostałe kontrole i ograniczenia

TLS >=1.2 chroni tylko ADCS; wczytywalna zgodna para PEM jest sprawdzana przed gotowością. Zaufanie/nazwę/ważność sprawdza klient. Brak certyfikatów klientów i uwierzytelniania kont. Klucze pozostają poza Git z ograniczonymi prawami. INI/logi zachowują kontrole typu/rozmiaru deskryptora i kolizji ścieżek oraz ograniczone UTF-8; wymagane są zaufane katalogi. Stop jest synchroniczny, main ignoruje SIGPIPE, deskryptory mają close-on-exec. Szczegóły: [TLS](ADCS_TLS.md), [konfiguracja](CONFIGURATION.md).

Nie jest to ochrona DDoS, limit bajtów/przepustowości, rejestr audytowy ani audyt bezpieczeństwa. Synchroniczny dysk/stderr może blokować reaktor. Krytyczny błąd pętli/alokacji może zakończyć proces. Brak limitu podsieci/globalnych prób, współdzielenia ochrony między procesami, wyjścia/backpressure, semantyki sesji, kont/ról/banów, haseł użytkowników, zapisu danych aplikacji do SQL i autoryzacji ról dla zmian administracyjnych HTTP. Kolejne moduły muszą wdrożyć je jawnie. Zakres testów i platform podaje [TESTING.md](TESTING.md).

## Hasło bazy — alpha-0.1.8

Używaj dedykowanego lokalnego konta SQL z minimalnymi uprawnieniami. Hasło pochodzi tylko z prywatnego zwykłego pliku właściciela, bez wartości w CLI/INI. Parametry SQL są wiązane; LOCAL INFILE i reconnect są wyłączone. Log nie może wskazywać pliku hasła. Błędy nie zawierają tekstu serwera ani sekretów. Migracje są jawne, chronione współpracującą blokadą i zapisem SHA-256; nie zastępują kopii bazy ani audytu struktury tabel. Zobacz [DATABASE.md](DATABASE.md).

## Lokalne API administracyjne — alpha-0.1.9

Każde żądanie tylko do odczytu wymaga tego samego efektywnego UID, prywatnego katalogu 0700/gniazda 0600 i prywatnego losowego tokena Bearer 64-hex. Porównanie tej samej długości jest stałoczasowe. Brak TCP, zmian stanu, CORS i anonimowego health. Ścisły podzbiór HTTP bez body odrzuca duplikaty/niejednoznaczne ramkowanie; klienci, nagłówki, bufory i bezwzględny czas żądania/odpowiedzi są ograniczone. Dane żądań/sekrety nie trafiają do logów ani odpowiedzi. Sprzątanie sprawdza inode i nie usuwa istniejącego endpointu przy starcie. Katalogi i procesy UID usługi są zaufane; przejęcie root/tego samego UID, limity tempa API, sesje WWW, role, brama i audyt żądań pozostają poza etapem. Zobacz [API.md](API.md).
