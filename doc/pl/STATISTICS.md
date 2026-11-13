# Statistics — alpha-0.3.13

**Bieżący etap — alpha-0.6.5:** [Protocol Negotiation](PROTOCOL_NEGOTIATION.md) dodaje do Transfer Core własne wartości protokołu i jawne rozróżnienie zwykłego ADC/1.0 oraz wymagającego TLS ADCS/1.0. Czysty wybór preferencji/możliwości nie obniża wymaganego TLS. CTM/RCM zachowują wybraną nazwę i zgodność rozszerzeń, a odpowiedzi błędów klientów zachowują tokeny. Rzeczywisty ncdc sprawdza TLS klientów z weryfikacją certyfikatu przez oba transporty huba; zachowano regresję pełnego terminala.

**Etap dostarczony w alpha-0.6.4:** [Connection Routing](CONNECTION_ROUTING.md) łączy przyjmowanie i dostarczanie D/E CTM oraz RCM we wspólnym routerze SID, jednym typie niezmiennego pakietu i jednej ścieżce ograniczonych kolejek. Odbiorcami są ustalone okresy życia gotowych sesji: D tylko cel, E cel i niezależne echo źródła. Token nie wybiera odbiorców; zachowano role, walidację, błędy i prywatność. Testy równoległych par używają tego samego tokenu bez mieszania odbiorców.

**Etap dostarczony w alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Etap dostarczony w alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) dodaje niezależny silnik C++20 koordynacji połączeń: ograniczone własne żądania, role aktywne/pasywne IPv4/IPv6 i niezmienne plany jednej próby związane z sesjami, anulowaniem i ponownymi kontrolami. Wspólna klasyfikacja adresów Users zachowuje Search. Routing CTM/RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.5.13:** [Monitoring wyszukiwań WebAdmin](WEBADMIN_SEARCH.md) dodaje lokalne tempo zapytań/wyników/odmów/kolejek, ograniczony wykres i dostępną tabelę przedziałów. Pierwszy odczyt, przerwy, wykryte zerowanie i błędy rozpoczynają nowy pomiar; pauza/ręczne odświeżanie i lokalne czyszczenie zachowują liczniki łączne. API/ustawienia i izolacja sekretu pozostają.

**Etap dostarczony w alpha-0.5.12:** [Konfiguracja i statystyki wyszukiwania](SEARCH_CONFIGURATION.md) udostępnia 19 liczników SCH/RES huba, wskaźnik przechowywanych okien tokenów z leniwym sprzątaniem i 16 efektywnych ustawień startowych przez uwierzytelnione API tylko do odczytu oraz dwujęzyczną stronę Search WebAdmin. Zakolejkowanie jest odrębne od dostarczenia; UDP między klientami jest wyłączone. Bez treści/historii i API zapisu konfiguracji.

Gałąź utworzona bezpośrednio z main `59b7b3a048b6d00504038a6a0d0b6580f3c04907`, zawierającego alpha-0.3.1–alpha-0.3.12. Poprzednie gałęzie i historia są zachowane.

## Własność i liczniki sesji

`users::Statistics` jest kopią wartości należących do SessionManager. `statistics()` i `counts()` działają w O(1), bez alokacji, noexcept, wyłącznie w wątku właściciela. Każda poprawna zmiana fazy aktualizuje liczniki po zatwierdzeniu modelu. Błędy walidacji, stare ID, odmowy limitów i wycofane alokacje nie zmieniają statystyk. Powtórzone close/finish nie nalicza zdarzenia ponownie; zmiana nicka/profilu nie jest nowym logowaniem.

| Pole | Znaczenie |
| --- | --- |
| `sessions.total` | Wszystkie posiadane modele, także closed oczekujące na usunięcie |
| `sessions.accepted/negotiating/identified/ready/closing/closed` | Rozłączne bieżące fazy; ich suma jest równa total |
| `online()` | Dokładnie `sessions.ready` |
| `peak_online` | Najwyższa liczba jednoczesnych ready w życiu managera |
| `created_sessions` | Poprawne dodania modelu, po alokacji i wstawieniu |
| `successful_logins` | Pierwsze przejścia do ready dla kolejnych sesji |
| `completed_sessions` | Pierwsze przejścia do closed, także błędy anonimowe i shutdown |

Zweryfikowany użytkownik czekający na powitanie jest identified, zajmuje limit użytkowników, ale nie jest online. Ready następuje po przyjęciu przez transport ostatniego LF własnego INF powitania. Rozpoczęcie closing natychmiast zmniejsza online; rezerwacje tożsamości pozostają do remove. Bezpośrednie EOF/wymuszone sprzątanie z dowolnej fazy również aktualizuje liczniki. Remove zmniejsza tylko total/closed. Sumy historyczne Users zatrzymują się na UINT64_MAX zamiast zawijać. Nowy manager/proces zaczyna od zera; MariaDB nie przechowuje statystyk.

## Publikacja i API

Tylko reaktor odczytuje SessionManager i publikuje liczby w atomikach Network przed oczekiwaniem oraz przy wyjściu. Wątek API nie odczytuje modeli ani kontenerów użytkowników. Bieżące wartości mogą czekać na granicę publikacji reaktora; niezależnie próbkowane pola nie stanowią transakcji. Nie należy wymagać równości między polami w czasie zmian połączeń. Active/online wracają do zera po zwykłym lub wyjątkowym zakończeniu reaktora. Sumy historyczne pozostają dostępne w obiekcie C++ TcpServer po run; pracownik HTTP kończy pracę podczas zamykania procesu.

`GET /api/v1/status` zachowuje dotychczasowe pola v1 i dodaje:

| Pole JSON | Znaczenie |
| --- | --- |
| `users.online` | Bieżący ready łącznie ADC/ADCS |
| `users.peak_online` | Szczyt ready od uruchomienia |
| `users.successful_logins` | Ukończone powitania, również po reconnect |
| `network.rejected_connections` | Wszystkie odmowy przyjęcia transportu; security_rejected jest podzbiorem |
| `network.closed_connections` | Zwolnione przyjęte gniazda, także anonimowe/błędne TLS i shutdown |
| `network.peak_connections` | Najwięcej jednocześnie przyjętych gniazd, także anonimowych i closing |
| `network.received_bytes/sent_bytes` | Faktycznie odebrane/wysłane bajty aplikacji z ramkami ADC, bez narzutu TLS |

`active_connections` liczy przyjęte gniazda, a `accepted_connections` sumuje ich przyjęcia, nie próby accept4 i nie logowania. Błędna identyfikacja po przyjęciu zwiększa accepted/closed, nie transport rejected. Odrzucone gniazdo nie zwiększa closed. Połączenia HTTP API nie wchodzą do liczników huba. Dotychczasowe statystyki ADCS/security zachowują znaczenie. Bajty opisują protokół huba, nie transfery plików P2P. W stanie bez zmian accepted = active + closed; jednoczesne niezależne odczyty nie gwarantują tej równości.

Pola są liczbowe, bez nicków, SID, CID, PID, IP, sekretów i ścieżek. Nadal wymagane są ten sam UID gniazda Unix i Bearer. Nie dodano publicznego portu ani trasy. Serializacja JSON zachowuje uint64; WebAdmin odrzuca liczby spoza bezpiecznego zakresu JavaScript zamiast wyświetlać niedokładne wartości.

## WebAdmin i zgodność

Przegląd pokazuje online, szczyt, poprawne logowania i zamknięte połączenia. Połączenia pokazują szczyt gniazd, wszystkie odmowy i sumy bajtów. Domyślny język to EN/US, dostępny jest PL. Dotychczasowy monitoring, nawigacja i wykres pozostają dostępne. Nieznane pola są usuwane przed wyświetleniem/eksportem. Brak lub błędna wartość licznika unieważnia odczyt, usuwa stare wartości i wyłącza eksport.

Starsze klienty v1 mogą ignorować nowe pola. Ten WebAdmin należy uruchamiać z alpha-0.3.13 lub późniejszym serwerem implementującym te pola; parser nie zastępuje brakujących danych zerami. Bez nowych opcji konfiguracji, zależności, migracji SQL i poleceń ADC. Konta, trwałe metryki, szybkości, histogramy, statystyki poszczególnych użytkowników i zdalna administracja pozostają planami.

## Weryfikacja

`users.statistics` porównuje pamiętane liczniki z modelami we wszystkich fazach, przy błędach, awariach alokacji, powtórzeniach i sprzątaniu. Sprawdza niepełny ostatni LF i anulowanie powitania w LoginHub. `users.statistics.process` używa prawdziwych mieszanych połączeń ADC/ADCS i chronionego API: anonimowy TLS, odmowy, błędny dowód, INF/nick, EOF, reconnect, fatal close, szczyty/sumy/bajty i brak danych prywatnych. Prawdziwy ncdc ADC/ADCS sprawdza logowanie, reconnect i online po wyjściu. Pozostają wymagane regresje cyklu życia, limitów, TLS, bazy i Apache. Testy parsera, przeglądarki i rzeczywistego API C++ weryfikują WebAdmin, US/PL i bezpieczny eksport.

Źródła: [oficjalna specyfikacja ADC](https://adc.sourceforge.io/ADC.html), [ncdc](https://dev.yorhel.nl/ncdc). Ten etap dodaje obserwację; dotychczasowe reguły logowania ADC nadal obowiązują.
