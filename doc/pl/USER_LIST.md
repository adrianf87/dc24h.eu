# User List — alpha-0.3.9

Etap powstał z main `b191141ad1cfa5a6a915945b5a934fe82640340e` po scaleniu alpha-0.3.8 User Login. Poprzednie etapy są już obecne. Gałąź alpha-0.3.9 zachowuje main/historię i nie jest scalana automatycznie.

## Synchronizacja początkowa

`adc::UserList` przygotowuje samodzielny `InitialUserList`: IINF huba, BINF każdego aktualnie publicznego użytkownika w rosnącej kolejności lokalnego SessionId i własny BINF dołączającego na końcu. Pożycza SessionManager i zaufany callback zwracający zwalidowane ClientInfo danego połączenia. Nie posiada trwałego rejestru, gniazd, kolejki, zegara ani poświadczeń. Wywołujący pozostaje w wątku reaktora i nie modyfikuje stanu przez callback. Manager/źródła muszą istnieć przez czas przygotowania; plan posiada własne komunikaty i przeżywa zmianę źródeł.

[Protokół ADC](https://adc.sourceforge.io/ADC.html) używa publicznych INF i QUI do informacji o użytkownikach i odejściach. Własny INF jako znacznik i kolejność FIFO zachowują dotychczasowe logowanie gości; nie dodano polecenia ani rozszerzenia resynchronizacji. UserLogin nadal odpowiada za rejestrację/gotowość, LoginHub za rzeczywiste wysyłanie.

`SessionManager::public_users(after, limit)` zwraca tylko identyfikatory identified/ready w kolejności rosnącej, ściśle za kursorem. Nie kopiuje User/profilu. Limit wynosi 1–256 (domyślnie 64); błędny rzuca invalid_argument. Pomija accepted/negotiating/closing/closed. UserRegistry nadal rezerwuje klucze closing/closed do usunięcia; publiczne członkostwo i liczba rezerwacji są różne. Strony są niezależnymi odczytami jednego właściciela, nie transakcją między wątkami.

UserList odczytuje strony po 256 ID do wyczerpania, bez ucinania listy na granicy. Uwzględnia identified czekające na powitanie, aby równoczesne przyjęcia się nie pomijały. Pomija closing/closed mimo rezerwacji kluczy. Autorytatywne pozostają istniejące modele/indeksy; nie powstaje drugi rejestr członkostwa.

## Walidacja i limity

`prepare(joining, client, lookup, available_wire_bytes)` wymaga negocjującego modelu, NORMAL po zatwierdzeniu gościa, poprawnego niezerowego SID i zweryfikowanego publicznego CID/nicka. Własny SID/CID/nick nie może być już zarezerwowany. Istniejący wpis musi mieć zwalidowane metadane z CID/nickiem zgodnym z modelem i poprawny protokołowy SID. Brakujące/nieaktualne/obce metadane odrzucają cały plan zamiast cicho usuwać użytkownika z listy.

Publiczne ramki pochodzą z ClientInfo::public_message. Zachowują zwalidowane pola ADC, np. adresy i deklaracje połączeń, a pomijają PID i nieznane pola. Sam profil User utraciłby te dane. Lista nie zawiera surowych buforów, haseł/PAS ani uprawnień kont. Nadal obowiązują limity tekstu/pól i serializacja ADC.

Builder serializuje każdy komunikat, mierząc faktyczne bajty UTF-8 po escapowaniu wraz z LF, i sprawdza odejmowanie przed dodaniem. Budżet obejmuje wszystkie ramki, w tym INF huba i własny INF. Dokładny rozmiar jest akceptowany, o bajt mniejszy odrzucany. Właściciel wyjścia podaje mniejszy pozostały limit klienta/huba, bez nowej opcji konfiguracji. Przygotowanie niczego nie kolejkuje, nie rezerwuje, nie rejestruje i nie zmienia gotowości/modelu. Błędy to invalid_state, invalid_identity, inconsistent_source i output_limit; błąd alokacji przekazuje bad_alloc. Częściowy plan tymczasowy jest niszczony przy błędzie.

Komunikaty/ID są ograniczone limitem sesji (najwyżej 65535) i budżetem kolejki. Poza planem potrzebna jest jedna strona ID i jedna tymczasowa serializowana ramka. Narzut obiektów/ciągów/wektorów oznacza, że bajty wyjścia nie są dokładnym limitem pamięci. Przegląd członkostwa ma O(liczba sesji), dodatkowo występują ograniczone wyszukiwania i serializacja. Nie deklaruje się gwarancji obciążenia/przepustowości.

## Synchronizacja i publikacja

LoginHub sprawdza/zatwierdza gościa, przygotowuje całą listę według aktualnie wolnych limitów, kolejkuje ją z oznaczeniem welcome na ostatniej ramce i rejestruje przez UserLogin. Dopiero potem rozsyła publiczny INF nowego użytkownika. Przepełnienie lub niespójne dane wymuszają natychmiastowe output_limit/internal_error przed publikacją; błąd alokacji przygotowania/kolejkowania/rejestracji daje natychmiastowe internal_error bez alokowania statusu. Przypięte wyjście pozostaje do sprzątania. Rozsyłanie obecności po zatwierdzeniu jest poza tą ochroną; nie deklaruje się pełnego odtwarzania po braku pamięci systemu.

Niezmienny przód FIFO i kolejka klienta zapewniają, że późniejsze dołączenia, aktualizacje/anulowania INF i odejścia IQUI następują po przygotowanym własnym INF. Równoczesny gość jest albo w początkowym planie, albo w dalszym join, nigdy w obu. UserLogin sprawdza tożsamość i ustawia gotowość dopiero po rzeczywistym wysłaniu końcowego LF. Częściowe/anulowane/końcowe wyjście nie kończy synchronizacji. Istniejące limity/terminy mogą rozłączyć wolnego/przepełnionego odbiorcę; nie dodano nieograniczonej listy ani trwałego strumienia ponowień.

Nie dodano zależności, migracji SQL, pól JSON API, funkcji WebAdmin ani tłumaczeń runtime. Domyślny EN/US UTF-8 i istniejące languages/us/languages/pl pozostają. Konta, zdalne API list, polecenia resynchronizacji, rozproszone snapshoty i czat/wyszukiwanie/transfery pozostają planami.

## Weryfikacja

`users.list` sprawdza wybór identified/ready, pomijanie pending/closing/closed, kursory/limity ID, deterministyczną kolejność, własny INF na końcu, brak pól prywatnych, zachowanie pól ADC, dokładny budżet, brakujące/nieaktualne źródła, własność komunikatów i więcej niż jedną stronę (260 użytkowników). Ograniczona kampania alokacji C++ potwierdza brak zmian modeli/rejestrów i możliwość ponowienia; nie wstrzykuje błędów alokatora C ani systemu. Testy UserLogin nadal sprawdzają ochronę przed publikacją i sprzątanie przy błędzie/przepełnieniu.

`users.list.process` porównuje rzeczywiste mapy klientów przy nakładających się dołączeniach ADC/ADCS, uporządkowanych znacznikach aktualizacji, polskiej zmianie nicka, anulowaniu pola, liście późniejszego gościa, odejściu i reconnect z tym samym CID/nowym SID. Oba kierunki transportów osiągają zgodność bez starego SID i powtórzonego CID. Rzeczywisty ncdc ADC/ADCS loguje się przy działającym obserwatorze, pokazuje dwóch użytkowników i dostaje go na początkowej liście; przy reconnect obserwator odbiera właściwe quit/join z niezmienionym CID. Lokalne wyniki i ograniczenia opisuje [TESTING.md](TESTING.md).
