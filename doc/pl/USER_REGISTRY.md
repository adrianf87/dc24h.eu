# User Registry — alpha-0.3.7

**Bieżący etap — alpha-0.3.9:** [User List](USER_LIST.md) tworzy samodzielne, deterministyczne powitanie ze stronicowanej listy identified/ready i zwalidowanych metadanych publicznych ADC. Sprawdza cały rozmiar po kodowaniu przed kolejkowaniem/rejestracją. Własny INF jest ostatni; dalsze join/update/quit zachowują synchronizację FIFO. Starsze opisy modułów zachowują zakres historyczny.

Etap powstał bezpośrednio z main `ca75d6c53d9b228b978dc3b3b8fdd2fcb0f33852`, po scaleniu alpha-0.3.6 Nick Manager. Wcześniejsze etapy alpha-0.3.1–alpha-0.3.6 są już obecne. Gałąź alpha-0.3.7 zachowuje main i gałęzie historyczne; nie jest scalana automatycznie.

## Własność i znaczenie kluczy

`users::UserRegistry` jest ograniczonym, niekopiowalnym i nieprzenoszalnym indeksem przyjętych użytkowników po SID i CID, obsługiwanym przez jeden reaktor. Należy prywatnie do SessionManager i zastępuje jego surowe mapy SID/CID. Przechowuje ciągi kluczy i lokalny SessionId właściciela, bez drugiej kopii User/profilu i bez wskaźników do danych wywołującego. SessionManager jest właścicielem Session, a każda Session swojego opcjonalnego User. NickManager niezależnie pilnuje nicków w tej samej transakcji przyjęcia/usunięcia.

[Specyfikacja ADC, identyfikacja klienta](https://adc.sourceforge.io/ADC.html#_client_identification) rozróżnia SID nadany przez hub, publiczny CID i prywatny PID. Przydziały SID nadal obsługuje ADC SidPool. Przyjęte/negocjujące połączenie może posiadać protokołowy SID bez publicznego wpisu w UserRegistry. Rejestracja CID nie uwierzytelnia konta. Indeksy nie przechowują PID, haseł/PAS, adresów IP, ról ani tekstu profilu.

Neutralny model Users używa ograniczonych, nieinterpretowanych kluczy SID/CID: niepustych, bez NUL, najwyżej 64/128 bajtów. `add` wymaga poprawnego User od walidującego adaptera; te sprawdzenia nie dowodzą składni ADC, własności SID, kanonicznego base32 ani hash(PID)==CID. Adapter ADC nadal sprawdza czteroznakowy SID, Tiger CID/PID, źródło, INF i Unicode przed przyjęciem. Klucze są porównywane dokładnie z wielkością liter; brak składania liter i cichej normalizacji. SID i CID to niezależne przestrzenie nazw.

## Kontrakt rejestru

| Operacja | Zachowanie |
| --- | --- |
| `add(session_id, user)` | Łączy jeden SID i CID z jednym właścicielem sesji. Nie zastępuje wpisu. Dane wejściowe są pożyczane wyłącznie na czas wywołania; ich późniejsza zmiana/usunięcie nie zmienia kluczy. |
| `by_sid(sid)` / `by_cid(cid)` | Zwracają opcjonalny SessionId bez tymczasowej alokacji. Puste/z NUL/za długie zapytanie nie ma właściciela. |
| `contains(session_id)` | Sprawdza kompletną publiczną rejestrację tego czasu życia. |
| `remove(session_id)` | Usuwa oba klucze tego właściciela, zwraca false przy braku. Powtórne sprzątanie nie usuwa klucza przejętego przez późniejszy czas życia. |
| `size()` / `capacity()` | Liczba publicznych rezerwacji i jawny limit 1–65535. |

`UserRegistryResult` to opcjonalny UserRegistryError: invalid_user, owner_exists, duplicate_sid, duplicate_cid lub capacity_reached. Ponowne add właściciela daje owner_exists także dla takich samych kluczy. Nie można zmienić SID/CID przez ponowne add. Ponowne powiązanie wymaga usunięcia/przyjęcia przez właściciela cyklu życia; SessionManager dodatkowo odrzuca zmianę SID/CID w aktualizacji działającego User. Błędny limit konstruktora rzuca invalid_argument. Błąd alokacji przekazuje bad_alloc.

Rejestr utrzymuje uporządkowane mapy SID i CID oraz odwrotną mapę właściciela z ich iteratorami. Każdy klucz występuje raz we własnej mapie; odwrotny indeks nie kopiuje ciągów. Wszystkie trzy alokujące wstawienia poprzedzają publikację kompletnego wpisu. Błąd alokacji usuwa tylko wpisy dodane w danym wywołaniu. Typowane odrzucenia i wyjątki zachowują innych użytkowników. Usuwanie korzysta z iteratorów i nie alokuje. Kopiowanie/przenoszenie jest wyłączone ze względu na zależność odwrotnego indeksu od węzłów map.

Limit liczy publiczne rezerwacje, nie wszystkie przyjęte połączenia. Pamięć jest ograniczona liczbą wpisów, dwoma ciągami kluczy i trzema węzłami map na użytkownika, z ograniczonym stanem tymczasowym podczas dodawania. To nie dokładny budżet bajtów ani gwarancja przepustowości. Wyszukiwanie/dodawanie/usuwanie mają O(log n). Na zewnątrz nie wychodzą iteratory, modyfikowalny rejestr, uchwyty bazy, wewnętrzny zegar ani dostęp między wątkami. Samodzielny wywołujący musi dostarczać różne identyfikatory czasów życia; po usunięciu nie ma trwałej historii wpisów.

## SessionManager i wyszukiwanie modeli

SessionManager jest prywatnym właścicielem UserRegistry z limitem sesji. Przyjęcie najpierw sprawdza fazę, monotoniczny czas, User i kolizje SID/CID/nicka. Przygotowuje wpisy UserRegistry i NickManager przed nierzuconym zatwierdzeniem Session/User. Wyjątek cofa oba rejestry i zachowuje negocjujący model. Dotychczasowe RegistryError i SessionError pozostają kontraktem zarządzanych operacji. Wewnętrzny błąd rejestru po tych sprawdzeniach oznacza naruszenie niezmiennika, nie dodatkową ścieżkę zewnętrznej modyfikacji.

`SessionManager::by_sid` i `by_cid` rozwiązują SessionId na pożyczoną Session tylko do odczytu. Nowe `user_by_sid` i `user_by_cid` zwracają pożyczony `const User*` do aktualnego User tej sesji albo null. Nie alokują ani nie kopiują profilu. Oba klucze wskazują ten sam model; świeże wyszukiwanie pokazuje zmianę nicka/profilu bez przebudowy indeksów niezmiennych SID/CID. Referencji do podstruktur User nie należy przechowywać przez aktualizację/usunięcie. Zmiany innych sesji zachowują adres Session właściciela. Kopie danych nadal zapewnia istniejące API `snapshot`/`page`.

Wpis obejmuje identified, ready, closing i closed do `remove`; samo znalezienie nie uprawnia do routingu. Przed dostarczeniem aplikacyjnym sprawdź ready() sesji. `identified_users()` raportuje teraz liczbę rezerwacji UserRegistry. `begin_close`, `force_close` i `finish_close` zachowują SID/CID i nick. Końcowe `remove` wymaga zamkniętego modelu, zwalnia klucze po lokalnym SessionId i usuwa model. Reconnect może użyć tego samego CID/nicka z nowym czasem życia; usunięta lokalna sesja pozostaje nieobecna.

## Działające ADC/ADCS

Istniejąca kontrola kolizji CID w LoginHub pyta teraz UserRegistry przez SessionManager; zajęty CID nadal daje ISTA 224 i ograniczone QUI/zamknięcie. Kolizja nicka pozostaje ISTA 222. Routing direct/echo statusu korzysta z zarejestrowanego SID i wymaga gotowości. Odrzucony gość nie trafia na listy publiczne i nie przejmuje kluczy. Zmiana nicka/profilu zachowuje SID/CID. Rozłączenie, błąd TLS i zatrzymanie sprzątają przez ten sam SessionManager.

Nie dodano zależności, konfiguracji, migracji SQL, pól JSON API, funkcji WebAdmin, komunikatów operatora ani kluczy tłumaczeń. Język tworzenia i domyślny runtime pozostaje EN/US; istniejące languages/us i languages/pl nadal obowiązują. Konta/uwierzytelnianie, trwałe tożsamości, role, rejestr rozproszony, czat/wyszukiwanie/transfery i administracyjne modyfikacje pozostają planami.

## Weryfikacja

`users.user_registry` sprawdza limity, puste zapytania, API const, niezależność/dokładność przestrzeni nazw, kolizje właściciela/SID/CID, własność wejścia, granice kluczy i lokalnego ID, brak alokacji przy wyszukiwaniu/usuwaniu, izolowane sprzątanie, ochronę przed starym właścicielem, ponowne użycie i 1000 czasów życia. Testy zarządzane potwierdzają ten sam rzeczywisty User z obu kluczy, aktualny nick/profil, odrzucenie zmiany tożsamości, rezerwacje closing/closed i brak starych wpisów. Ograniczone błędy alokacji C++ obejmują każdy punkt samodzielnego dodawania oraz wspólnego przyjęcia UserRegistry/NickManager/Session, z zachowaniem innego aktywnego użytkownika i ponowieniem po błędzie. To nie pełna kampania błędów alokatora całego systemu.

`users.user_registry.process` sprawdza rzeczywisty mieszany reaktor TCP/TLS w obu kierunkach: listy SID/CID, powtarzane kolizje CID, routing do pierwotnego właściciela, zmianę nicka bez zmiany kluczy, reconnect z tym samym CID i nowym SID, brak routingu do starych/odrzuconych SID, odrzucenie zmiany CID i kolejne użycie tożsamości. Prywatne dane tożsamości/profilu/statusów nie trafiają do logów operatora. Nadal obowiązują testy sesji, nicków, źródeł, danych końcowych i dostawcy kryptografii.

Rzeczywisty ncdc loguje się i łączy ponownie przez ADC/ADCS, zmieniając polski nick. Przechwycenie ADC dodatkowo sprawdza inny SID i ten sam publiczny CID przy reconnect. ADCS korzysta z rzeczywistego listenera OpenSSL i sprawdza NORMAL po stronie klienta; nie deklaruje odczytu zaszyfrowanego ruchu jako tekstu jawnego. Lokalne wyniki i ograniczenia opisuje [TESTING.md](TESTING.md).
