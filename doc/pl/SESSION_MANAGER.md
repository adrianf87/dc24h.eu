# Session Manager — alpha-0.3.4

**alpha-0.7.9 Auth Sessions:** [sesje kont związane z połączeniem](AUTH_SESSIONS.md) dodają atomowe powiązanie przy gotowości, zaufane odczyty według konta i unieważnianie połączenia. Zamykanie natychmiast usuwa uwierzytelnienie. Bez nowej migracji SQL, automatycznych ról i sesji przeglądarkowych. CI korzysta z [runnerów GitHub](GITHUB_CI.md) bez czekania na wynik.

**Etap dostarczony w alpha-0.3.13:** [Statistics](STATISTICS.md) dodaje liczniki cyklu życia w czasie stałym, użytkowników online/szczyt i sumę logowań, atomowe statystyki transportu, pola chronionego API oraz metryki WebAdmin US/PL. Starsze etapy poniżej zachowują swój zakres historyczny.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.3.5:** [User Core](USER_CORE.md) dodaje typ Nickname sprawdzający UTF-8/NFC, wspólną walidację tekstu ADC/Users, samodzielnych kandydatów zmiany nicku i jawną projekcję ClientInfo/VerifiedIdentity. Users korzysta z neutralnego dc24h_text i istniejącego utf8proc. Własność/unikalność managera pozostają bez zmian. Starsze opisy dotyczą zakresu historycznego.

Etap bazuje na main `6162f8a648b4e7b720d0417d0e829c521ea0e55d`, po uporządkowanym scaleniu Session Manager/User Core. Centralny rejestr czasu życia z alpha-0.3.2 przejmuje indeksy SID, CID i nicku z LoginHub. Łączy publiczną tożsamość/profil User ze zmianami cyklu życia, zamiast tworzyć kolejny magazyn sesji. Main i historyczne gałęzie są zachowane; ta gałąź nie scala się automatycznie.

## Własność i rejestracja

Jeden niekopiowalny i nieprzenoszalny SessionManager posiada modele Session w dc24h_users, niezależnie od gniazd, parsera ADC, TLS, logowania i SQL. Session posiada opcjonalny User. Obiekty protokołu LoginHub pożyczają modele const, zachowując stan protokołu, dzierżawy SID i kolejki wyjścia. Każdy rodzaj publicznego klucza ma jeden autorytatywny indeks w managerze. add/find nie udostępnia modyfikowalnych wskaźników: zmiany zarejestrowanych modeli przechodzą przez manager. Samodzielny Session nadal działa bez managera.

Jawna pojemność wynosi 1–65535 wpisów; LoginHub korzysta z dotychczasowego limitu połączeń. Liczą się wszystkie zarejestrowane transporty: ADCS przed TLS, negocjowanie, przyjęta tożsamość oczekująca na powitanie, gotowe i zamykane sesje. Zamykany wpis zajmuje miejsce do usunięcia. To inna liczba niż liczba przyjętych publicznych użytkowników i gotowość aplikacyjna.

`add(SessionId, Transport, accepted)` zwraca pożyczony wskaźnik const Session lub typowany stale_id/capacity_reached. Niezerowe ID muszą rosnąć przez cały czas życia managera, również po usunięciu. Brak pojemności nie zatwierdza ID ani wpisu. Wyjątek alokacji nie pozostawia częściowego modelu ani nie przesuwa najwyższego ID. Network nadal dostarcza sekwencję bez ponownego użycia; rejestracja nie rozpoczyna negocjacji. Błąd konstrukcji obiektu protokołu finalizuje/usuwa model, ale zużywa już zarejestrowane ID.

Uporządkowana mapa posiada modele przez unique_ptr. Dodawanie/usuwanie innego ID zachowuje adres modelu. Usunięcie/destrukcja unieważnia jego referencje. Referencje do części tożsamości/profilu wygasają także przy zastąpieniu. Cały dostęp, również const i kopiowanie snapshotów, jest ograniczony do wątku reaktora; const nie zapewnia synchronizacji. Nie ma uchwytów między wątkami, callbacków ani trwałych UUID.

## Zarządzane zmiany i rezerwacje tożsamości

Manager udostępnia start_negotiation, identify, update_user, rename, mark_ready, begin_close, force_close i finish_close z przekazanym czasem monotonicznym. MutationResult rozróżnia RegistryError i SessionError; brak ID jest jawnym błędem. Bramki fazy/czasu, pierwsza/wymuszona/końcowa przyczyna i stały termin zamykania zachowują zachowanie Session Core. Nie ma ukrytych odczytów zegara ani planowania.

`identify(id, User, time)` wymaga negotiating, poprawnych ograniczonych danych i wolnych SID, CID i nicku. Kolizje zwracają duplicate_sid, duplicate_cid lub duplicate_nickname. Alokacja trzech węzłów indeksów poprzedza zatwierdzenie modelu bez wyjątków; błąd alokacji wycofuje wcześniejsze wpisy, pozostawiając model bez zmian. Po zatwierdzeniu nie ma alokacji. Odrzucenie fazy/czasu/modelu nie tworzy rezerwacji. Testy nie stanowią kampanii wymuszania dowolnych awarii alokacji.

`update_user` wymaga identified/ready, niezmiennych SID/CID i niemalejącego czasu. Nowy nick jest sprawdzany i alokowany przed zastąpieniem całego User; stary indeks znika dopiero po zatwierdzeniu modelu. Kolizja/błędne dane/cofnięty czas zachowują tożsamość i profil. Ten sam nick pozwala odświeżyć profil bez dodawania wpisu. `rename` korzysta z tej samej ścieżki, zachowując profil. Klucze są porównywane dokładnie, z rozróżnianiem wielkości liter, zgodnie z wcześniejszą polityką ADC; nie wprowadzono nowej normalizacji Unicode ani ignorowania wielkości liter.

Fabryki magazynowania i unikalność rejestru nie dowodzą własności CID ani konta. Istniejące sprawdzenia ADC SID/CID/Tiger/NFC/INF poprzedzają projekcję i pozostają autorytatywne. Ogólny manager obsługuje ograniczone publiczne klucze; nie indeksuje PID, PAS/hasła, adresu IP, uprawnień ani kont/ról. Deklaracje profilu klienta nie są zweryfikowanymi faktami.

Rezerwacje utrzymują się w closing i closed do remove, wykluczając ponowne użycie podczas opróżniania wyjścia. Wyszukiwanie może zwrócić sesję niegotową. `finish_close` zapisuje końcowy stan bez zwalniania kluczy. `remove` wymaga closed i usuwa wyłącznie SID/CID/nick danego właściciela oraz model. missing i not_closed pozostają jawne. Po usunięciu publiczna tożsamość może należeć do nowego, większego SessionId, a stare lokalne ID pozostaje nieobecne. Nie ma historii zamkniętych sesji ani wpisów zastępczych po usunięciu.

## Wyszukiwanie, kopie i koszt

`find(id)`, `by_sid(key)`, `by_cid(key)` i `by_nickname(key)` zwracają pożyczony wskaźnik const Session albo null. Zapytania publiczne odrzucają pusty/NUL/zbyt długi klucz przed wyszukiwaniem; limity modelu wynoszą 64/128/256 bajtów. Trzy przestrzenie kluczy są niezależne. Przed przekazaniem aplikacyjnym trzeba sprawdzić ready(); znalezienie rezerwacji nie oznacza zgody na dostarczenie ani uwierzytelnienia. Uporządkowane mapy zapewniają O(log n) przy ograniczonej długości kluczy, bez alokowania tymczasowych tekstów zapytania.

`identified_users()` liczy publiczne rezerwacje, w tym identified/ready/closing i sfinalizowane wpisy czekające na usunięcie. `counts()` zwraca sumę i sześć bieżących liczników w O(1), aktualizowanych po poprawnych przejściach modeli. Testy porównują je z posiadanymi modelami. Patrz [Statistics](STATISTICS.md). Hub usuwa sfinalizowane wpisy w tym samym wywołaniu sprzątania.

`snapshot(id)` zwraca samodzielną kopię lub nullopt. `page(after, limit)` zwraca kopie według rosnącego SessionId ściśle po wyłącznym kursorze; domyślnie 64, limit 1–256. Niepoprawny limit powoduje wyjątek przed kopiowaniem; luki/usunięte kursory działają, UINT64_MAX nie zawija. Pusta strona oznacza bieżący koniec. Ostatnie ID jest kolejnym kursorem. Strony są niezależnymi odczytami właściciela, nie zamrożoną transakcją. Kopie zawierają ograniczoną tożsamość/profil i cykl życia; przetrwają aktualizację/usunięcie źródła. Nie dodano nieograniczonego kopiowania całego rejestru.

Pamięć ograniczają pojemność, limity modelu/profilu oraz maksymalnie trzy ograniczone klucze na przyjęty model; nie jest to konfigurowalny budżet dokładnej liczby bajtów. Kopie kluczy służą wyszukiwaniu i nie tworzą drugiego modelu User. Rejestracja, wyszukiwanie i usuwanie mają O(log n), strona O(log n + limit), poza ograniczonym kopiowaniem. Nie przeprowadzono benchmarku wydajności/obciążenia.

## Integracja ADC/ADCS

LoginHub nie posiada już map nick/CID/SID. Wstępna kontrola nicku i kontrola zwalidowanego CID odczytują SessionManager, zachowując statusy 222/224. Zbudowanie ograniczonego powitania poprzedza identify w managerze. Pełne wysłanie własnego INF oznacza gotowość przez manager dokładnie raz. Aktualizacje INF zastępują cały User i indeks nicku; błędna walidacja i zajęty nick zachowują wcześniejszy model.

Bezpośredni/echo STA wyszukuje SID w managerze i sprawdza gotowość przed użyciem istniejącego obiektu protokołu/wyjścia. Przekazywanie, kolejność powitania, limity bezpieczeństwa i serializacja ADC pozostają bez zmian. Sprzątanie rozgłasza IQUI, gdy trzeba, anuluje wyjście/rozłącza protokół, finalizuje model, niszczy obiekt protokołu i usuwa model/indeksy. Kolejne disconnect w hubie jest nieszkodliwe. Zamykana rezerwacja nie jest celem normalnego przekazywania i nie może być przejęta przez inne przyjęcie.

`LoginHub::sessions()` pozostaje wewnętrznym odczytem const, bez dostępu HTTP/WebAdmin. Nie potrzeba nowej konfiguracji, komunikatów/tłumaczeń runtime, zależności, migracji SQL, danych logowania ani pól JSON API. Istniejące katalogi US/PL są aktualne. Konta, role, bany, trwałe/rozproszone rejestry, synchronizowana publikacja i zdalne mutacje są osobnymi etapami.

## Weryfikacja

`users.registry` sprawdza typowane błędy braku/fazy/czasu, trzy kolizje bez częściowej rezerwacji, niezmienne klucze, brak częściowych zmian nicku/profilu, dokładne porównanie i granice zapytań, rezerwacje closing/closed, niezależne usuwanie, ponowne użycie tożsamości przy nowym lokalnym ID i 1000 kolejnych przyjęć/usunięć. Sprawdzenia kompilacyjne wymagają pożyczonych modeli const. Zaktualizowany users.manager korzysta z API zarządzanych zmian, zachowując testy pojemności, stabilności adresów, stronicowania, maksymalnych ID i wycofania błędu inicjalizacji protokołu.

users.core sprawdza też, czy publiczne zapytania rzeczywistego LoginHub wskazują ten sam model, stary nick znika po zmianie, zamykany klient zachowuje rezerwację, a disconnect ją usuwa. Dotychczasowe testy procesów obejmują mieszane TCP/TLS, duplikaty, aktualizacje/reconnect, gotowość, końcowe wyjście i rzeczywiste ncdc ADC/ADCS. Zobacz [testowanie](TESTING.md), [Session Core](SESSION_CORE.md), [User Core](USER_CORE.md) i [logowanie gości](ADC_LOGIN.md). Wyniki lokalne nie dowodzą innych systemów, zdalnego CI, obciążenia produkcyjnego ani audytu bezpieczeństwa.
