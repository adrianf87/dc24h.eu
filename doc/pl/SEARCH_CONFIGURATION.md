# Konfiguracja i statystyki wyszukiwania — alpha-0.5.12

**Bieżący etap — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) dodaje do Transfer Core własne wartości protokołu i jawne rozróżnienie zwykłego ADC/1.0 oraz wymagającego TLS ADCS/1.0. Czysty wybór preferencji/możliwości nie obniża wymaganego TLS. CTM/RCM zachowują wybraną nazwę i zgodność rozszerzeń, a odpowiedzi błędów klientów zachowują tokeny. Rzeczywisty ncdc sprawdza TLS klientów z weryfikacją certyfikatu przez oba transporty huba; zachowano regresję pełnego terminala.

**Etap dostarczony w alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) łączy przyjmowanie i dostarczanie D/E CTM oraz RCM we wspólnym routerze SID, jednym typie niezmiennego pakietu i jednej ścieżce ograniczonych kolejek. Odbiorcami są ustalone okresy życia gotowych sesji: D tylko cel, E cel i niezależne echo źródła. Token nie wybiera odbiorców; zachowano role, walidację, błędy i prywatność. Testy równoległych par używają tego samego tokenu bez mieszania odbiorców.

**Etap dostarczony w alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Etap dostarczony w alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) dodaje niezależny silnik C++20 koordynacji połączeń: ograniczone własne żądania, role aktywne/pasywne IPv4/IPv6 i niezmienne plany jednej próby związane z sesjami, anulowaniem i ponownymi kontrolami. Wspólna klasyfikacja adresów Users zachowuje Search. Routing CTM/RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

Weryfikacja lokalna 2026-11-13: **120/120 zestawów CTest przeszło osobno w Debug, Release i ASan/UBSan**, z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym rzeczywistym ncdc ADC/ADCS/UDP, MariaDB i Apache HTTPS. Bez ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, 6 testów Node, 8 testów Chromium i integracja z API C++. Poprawne są metadane, UTF-8, zgodność plików językowych i linków dokumentacji, składnia zmienionych plików Python, przykładowy INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne nie zostały zweryfikowane.

Gałąź alpha-0.5.12 utworzono z main `c20b6d4bdbcf1b2fcdb7f2ded1f814988a37446a`, a przed tym etapem włączono ukończoną gałąź Search Security `a17009447b9375feca932be58ba39e555937931d`. Main pozostaje bez zmian; zachowano istniejący kod i historię.

## Efektywna konfiguracja startowa

16 sprawdzanych ustawień `[search]` opisanych w [Search Security](SEARCH_SECURITY.md) jest teraz widocznych w uwierzytelnionym `GET /api/v1/status` i na stronie Search w WebAdmin. Wartości domyślne i zakresy nie zmieniły się. Operator edytuje [przykładowy INI](../../config/dc24h.ini.example), sprawdza go przez `dc24h --config ŚCIEŻKA --check-config` i uruchamia serwer ponownie. Nie ma przeładowania podczas pracy ani API zapisu. Panel pokazuje rzeczywiste wartości użyte przy starcie, w tym wartości domyślne pominiętych kluczy.

Pięć przełączników to `queries_enabled`, `results_enabled`, `guest_queries`, `guest_results`, `require_result_token`. Jedenaście limitów to `max_active_searches`, `max_active_per_user`, `search_lifetime_seconds`, `query_rate`, `query_burst`, `query_byte_rate`, `query_byte_burst`, `result_rate`, `result_burst`, `result_byte_rate`, `result_byte_burst`. Wyniki wysyłane i odbierane mają niezależne budżety o tych samych ustawieniach. Widoczna jest polityka globalna/gości; zaufane indywidualne uprawnienia sesji pozostają prywatne.

## Kontrakt liczników zbiorczych

`search/statistics.hpp` definiuje 19 kumulacyjnych liczników uint64 z nasyceniem oraz jeden wskaźnik bieżący. Aktualizuje je LoginHub w wątku reaktora. Każde zdekodowane SCH/RES dla istniejącego, niezamykającego się połączenia zwiększa dokładnie jeden licznik odebrania i jeden wynik końcowy. Obejmuje to odrzucenie źródła/stanu/polityki i cichą odmowę antyflood. Błędy surowej składni kodeka, bezpośrednie wywołania zaufanego Core/routera, ruch po zamknięciu i UDP między klientami są wyłączone. Nowy proces zeruje liczniki; nie ma trwałego zapisu.

| Pole zapytania | Znaczenie |
| --- | --- |
| queries_received | Wszystkie zdekodowane próby SCH objęte pomiarem. |
| queries_routed | Dispatch zwrócił raport, również przy pustym rozsyłaniu/braku zakolejkowania. |
| queries_rejected | Odrzucenie stanu/źródła/polityki, semantyki, tokenu/limitu okien lub niedostępnego celu. |
| queries_rate_limited | Odmowa limitu liczby/bajtów nadawcy. |
| queries_ignored | Dopuszczone zapytanie bez kryteriów. |
| query_errors | Błąd przygotowania źródła/wewnętrznego dispatch, w tym alokacji. |
| query_queued | Kopie przyjęte do kolejek odbiorców. |
| query_unavailable | Niedostępny cel kierowany lub odbiorca podczas dispatch. |
| query_refused | Kopie odrzucone przez kolejkę/sink odbiorcy. |
| query_canceled | Kopie anulowane przez ponowną kontrolę dispatch, również po zamknięciu źródła. |

| Pole wyniku | Znaczenie |
| --- | --- |
| results_received | Wszystkie zdekodowane próby RES objęte pomiarem. |
| results_routed | Raport dispatch bez odmowy limitu odbiorcy, także bez zakolejkowania. |
| results_rejected | Odrzucenie stanu/źródła/polityki/tokenu/semantyki/celu. |
| results_rate_limited | Odmowa limitu wysyłanych wyników nadawcy. |
| results_recipient_limited | Odmowa limitu wyników przychodzących celu, również bez powiadomienia. |
| result_errors | Błąd przygotowania źródła/wewnętrznego dispatch. |
| result_queued | Kopie przyjęte do kolejki celu. |
| result_unavailable | Niedostępny cel przy przygotowaniu lub dispatch. |
| result_refused | Kopie odrzucone przez kolejkę/sink celu, również przy limicie odbiorcy. |

Liczba odebranych jest sumą pięciu wyników końcowych przed nasyceniem dla pojedynczego odczytu wątku właściciela. Liczniki kolejek są osobnym wymiarem: broadcast może zakolejkować wiele kopii, a awaria celu nie jest błędem źródła. Zakolejkowanie nie potwierdza dostarczenia przez socket ani przyjęcia przez klienta. Odpowiedzi systemowe i powiadomienia nie są kopiami wyszukiwania. Błąd alokacji kolejki celu jest odmową i izoluje ten cel.

`tracked_searches` oznacza liczbę **przechowywanych okien tokenów** w Core. Wygasanie jest leniwe: przeterminowany wpis może pozostać do zwykłego sprzątania. Odczyt API nie wymusza skanowania terminów. Rozłączenie, odebranie prawa wyszukiwania, anulowanie i zwykłe sprzątanie usuwają wpisy. Zapytania bez tokenu nie dodają okien. Nie jest to liczba plików/wyników ani dokładna liczba niewygasłych wyszukiwań; etykieta panelu i podpowiedź wyjaśniają to ograniczenie.

## API i wątki

Dotychczasowe lokalne uwierzytelnione API dodaje obiekt `search` z `statistics` (19 pól powyżej i `tracked_searches`) oraz `configuration` (16 ustawień). Pełny przykładowy JSON znajduje się w [wersji US](../us/SEARCH_CONFIGURATION.md#api-and-threading). Uwierzytelnianie, proxy Apache i obsługa sekretu pozostają zgodne z [API](API.md).

Reaktor publikuje liczniki/wskaźnik w polach atomowych odczytywanych przez wątek API. Pola można bezpiecznie próbkować oddzielnie, ale odpowiedź nie jest transakcją wszystkich liczników; klient nie może odrzucać próbki za chwilową różnicę sum. Konfiguracja po starcie jest niezmienna. Odpytywanie nie zeruje liczników, nie czyta Core z obcego wątku i nie kopiuje rejestru tokenów.

Serializowane są wyłącznie dozwolone nazwy zbiorcze i ustawienia. Brak treści zapytań, nazw wyników, TTH/CID/SID, tokenów, uprawnień pojedynczych sesji, sekretów, historii i rekordów bazy. URES pomiędzy klientami omija hub i nie jest liczone; ADCS nie szyfruje oddzielnego UDP.

## WebAdmin i zgodność

Strona Search/Wyszukiwanie zawiera wskaźnik okien, dziesięć metryk zapytań, dziewięć metryk wyników i szesnaście ustawień tylko do odczytu. Obsługuje EN/US i PL, desktop/telefon, dotychczasowe odświeżanie/utratę połączenia oraz eksport oczyszczonego JSON. Nie dodano formularza konfiguracji ani sekretu API w przeglądarce.

Starsza odpowiedź bez `search` pozostaje poprawna: panel pokazuje komunikat niedostępności i kreski zamiast zer. Obecne, ale niepełne/niepoprawne rozszerzenie unieważnia migawkę i uruchamia dotychczasowy stan utraty połączenia. Nieznane pola są usuwane przed wyświetleniem/eksportem. Liczniki muszą być nieujemnymi bezpiecznymi liczbami całkowitymi JavaScript; przekroczenie `Number.MAX_SAFE_INTEGER` powoduje odrzucenie zamiast zaokrąglenia, choć serwer nadal liczy do nasycenia uint64. Limity mają te same zakresy i relację na użytkownika/do całości co przy starcie.

## Weryfikacja i granice

`search.statistics` sprawdza dokładne wyniki, próby ignorowane/błędne/duplikaty/odmowy/antyflood, uprawnienia, limity źródła i odbiorcy, odmowę/anulowanie kolejki, sprzątanie/zerowanie, awarię alokacji, nasycenie i zliczanie bez alokacji. `search.statistics.process` sprawdza dokładne liczniki ADC/ADCS, ustawienia, uwierzytelnianie/prywatność, sprzątanie okien i restart. Scenariusze rzeczywistego ncdc active/passive potwierdzają liczenie SCH/DRES w hubie i wyłączenie zewnętrznego URES. Testy panelu obejmują wszystkie wartości, języki, odświeżanie/restart, starsze/błędne API, filtrowanie eksportu i ruch od API C++ do React.

Wyniki walidacji zapisano w [CHANGELOG](../../CHANGELOG.md). Nie dodano zależności, migracji, logowania treści, API zapisu konfiguracji, trwałej historii, deduplikacji wyników ani certyfikacji obciążeniowej. Zdalne CI i pełna macierz systemów/klientów wymagają osobnego sprawdzenia.
