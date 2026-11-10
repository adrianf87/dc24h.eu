# Bezpieczeństwo — alpha-0.1.7

**Bieżący etap — alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) dodaje osobny adapter polityki i rozsyłania BMSG oraz jedną wspólną niemutowalną ramkę na publikację. Offsety i logiczne budżety odbiorców pozostają niezależne; zachowano bramki źródła/stanu/prywatności i FIFO.

**Etap dostarczony w alpha-0.4.2:** [Chat Core](CHAT_CORE.md) dodaje osobny silnik wiadomości niezależny od protokołu oraz publiczne BMSG, z kopią dla nadawcy i /me. Czat prywatny/grupowy/feature/hub, moderacja i historia pozostają osobnymi etapami.

**Etap dostarczony w alpha-0.4.1:** [ADC MSG](ADC_MSG.md) dodaje kodek MSG z typowanymi polami PM/ME, sprawdzanym budowaniem komunikatów, ograniczoną serializacją i walidacją w maszynie stanów. Dostarczanie czatu i polityka rozmów pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.3.13:** [Statistics](STATISTICS.md) dodaje liczniki cyklu życia w czasie stałym, użytkowników online/szczyt i sumę logowań, atomowe statystyki transportu, pola chronionego API oraz metryki WebAdmin US/PL. Starsze etapy poniżej zachowują swój zakres historyczny.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

**Etap dostarczony w alpha-0.3.4:** [Session Manager](SESSION_MANAGER.md) posiada indeksy SID/CID/nick wraz z żywymi modelami. Odczyt const i zarządzane zmiany cyklu życia/User zachowują unikalność przy przyjęciu, zmianie nicku i sprzątaniu. LoginHub zachowuje protokół i kolejki. Starsze opisy dotyczą zakresu historycznych etapów.

**Etap dostarczony w alpha-0.3.3:** [User Core](USER_CORE.md) dodaje do Session publiczny model gościa i profilu, projekcję całego zwalidowanego INF oraz aktualizacje przez bramkę cyklu życia. Powstał bezpośrednio z main z Session Core; main łączy go teraz z wcześniej scalonym alpha-0.3.2 Session Manager. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.


**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

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

## Podstawa WebAdmin — alpha-0.1.10

Nie umieszczaj tokena w pamięci przeglądarki, JavaScript, URL, zmiennych VITE ani HTML. Adapter deweloperski sprawdza prywatny plik właściciela poza serwowanym katalogiem, blokuje obcy origin/host/site/metody/ścieżki/body, dodaje własny sekret i normalizuje ograniczone odpowiedzi. To lokalne narzędzie, nie produkcyjne logowanie WWW. Przykład Apache2 serwuje statyczne zasoby lokalnie i blokuje API. Etap nie dodaje operacji zmieniających stan, zdalnego API, sesji WWW ani ról. Zobacz [WEBADMIN.md](WEBADMIN.md).

## Przykład HTTPS Apache2 — alpha-0.1.11

TLS i logowanie WWW są osobnymi mechanizmami. Szablon wybiera TLS 1.2/1.3 z AEAD, ustala host przekierowania, zachowuje Require local, blokuje prywatne/ukryte/sekretne ścieżki i wyłącza indeksy/symlinki. HSTS jest krótki, tylko dla hosta i HTTPS; CSP/no-sniff/referrer/frame obejmują błędy. Przed użyciem zweryfikuj zaufanie/nazwę/zgodność klucza; sama składnia nie dowodzi zgodności. Szablon nie ujawnia tokena i nie aktywuje bramy produkcyjnej. Zobacz [APACHE2_HTTPS.md](APACHE2_HTTPS.md).

## Granica ADC State — alpha-0.2.3

Niezależny moduł stanów odrzuca przedwczesne komendy, powtórzone kroki logowania, przychodzące typy zarezerwowane dla huba i podszyty źródłowy SID. Jawne zatwierdzenie wymaga oczekującego żądania; samo parsowanie INF/PAS nie przyznaje NORMAL. Moduł nie zachowuje PID, PAS, nicku ani parametrów i nie loguje. To kontrola kolejności, nie uwierzytelnianie ani wdrożona granica bezpieczeństwa: odbiorczy Network go nie wywołuje. Przyszły dispatcher musi sprawdzić zgodność funkcji/hash, PID/CID, wymagane pola, IP, unikalność nicku/CID i hasło przed zatwierdzeniem, usunąć PD przed przekazaniem, zastosować limity/terminy i zamknąć po odrzuceniu. Zobacz [ADC_STATE.md](ADC_STATE.md).

## Polityka SUP — alpha-0.2.4

Połączony moduł SUP/stanów sprawdza początkowy BASE/wspólny TIGR, rozdziela funkcje lokalne i klienta, ogranicza tokeny/pamięć i wycofuje błędne aktualizacje. Wybranego hash nie można usunąć ani zmienić. Nieznane funkcje klienta są biernymi metadanymi i nie są odbijane do odpowiedzi huba. Dodatkowe funkcje lokalne wymagają implementacji wywołującego; domyślnie reklamowane są tylko BASE/TIGR. Wybór nie oznacza obliczania Tiger, walidacji PID/CID, hasła ani konfiguracji TLS. Transport produkcyjny jeszcze nie wywołuje modułu. Błędy/logi nie zawierają tekstu klienta ani sekretów. Zobacz [ADC_SUP.md](ADC_SUP.md).

## Własność SID — alpha-0.2.5

Wspólna pula gwarantuje unikalność aktywnych rezerwacji jednego huba, także podczas oczekiwania na wysłanie ISID. Prywatny przenoszalny uchwyt zapobiega zwalnianiu przez skopiowany SID, a reset przeniesionego/starego uchwytu nie zwalnia nowego właściciela. Koder obejmuje pełne 20 bitów; lokalna polityka wyklucza zero/AAAA z przydziału klientom. SID jest deterministycznym publicznym adresem routingu, nie sekretem, uprawnieniem ani cookie. Nadal wymagane są kontrole wykonawcy/źródła. Wyczerpanie daje stały błąd bez częściowej rezerwacji. Zmiany puli/sesji i zniszczenie ostatniego współwłaściciela należą do jednego wątku. Anulować stare wyjście i referencje rejestru przed zwolnieniem/ponownym użyciem. Moduł nie jest jeszcze granicą bezpieczeństwa działającego transportu. Zobacz [ADC_SID.md](ADC_SID.md).

## Walidacja INF — alpha-0.2.6

Początkowe NI/ID/PD, kanoniczne kodowanie hash, Tiger(PID)==CID, limity liczb/tekstu/funkcji i zgodność IP są sprawdzane przed zatwierdzeniem. PD jest pożyczony z wejścia, nigdy nie trafia do ClientInfo i jest usunięty z publicznych snapshotów; odkodowane tymczasowe bajty PID są czyszczone. Oryginalnych ramek właściciela API nie usuwa — nie wolno ich logować/przekazywać. Roszczenia uprawnień poza CT 0/1 są odrzucane; CT1 jest informacyjne, nie autoryzuje. Nieznane pola są ignorowane; duplikaty odrzucane także dla nieznanych nazw. Brak algorytmu Libgcrypt odrzuca walidację bez zastępczego hash i zmian globalnej polityki kryptografii. Globalna unikalność nicku/CID, konta, role i podłączenie transportu pozostają poza modułem. Zobacz [ADC_INF.md](ADC_INF.md).

## Dedykowany weryfikator CID/PID — alpha-0.2.7

Wspólny weryfikator pożycza tekst, dekoduje PID bezpośrednio do niekopiowalnego bufora, oblicza hash w drugim czyszczonym buforze i porównuje dokładnie 24 bajty przez CRYPTO_memcmp. Każdy zwykły powrót czyści oba własne bufory; nie obiecujemy wymazania wejścia, rejestrów i dowolnych kopii kompilatora/bibliotek. Stałoczasowość dotyczy końcowego porównania, nie składni i całego handshake. Sukces zawiera tylko CID, nie poświadczenie konta, PID, rolę ani dowód globalnej unikalności. Awarie algorytmu/kontekstu/odczytu są testowane lokalnymi wrapperami linkera; produkcja nie ma wstrzykiwanego dostawcy ani zmian FIPS/globalnych trybów. Zobacz [ADC_IDENTITY.md](ADC_IDENTITY.md).
