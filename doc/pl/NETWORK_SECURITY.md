# Bezpieczeństwo sieci — alpha-0.9.3

Ochrona połączeń TCP, timeouty i limity buforów. Ten etap ogranicza kolejkę nasłuchu jądra oraz, na każdym przyjętym gnieździe, które hub zatrzymuje, bufory jądra wysyłania i odbioru oraz TCP user timeout. Gałąź powstaje z main `95abd7e`. Nazwa gałęzi to `alpha-0.9.3`.

Timeouty aplikacji — bezczynność, uzgadnianie, niepełna ramka, logowanie, wyjście i zamknięcie — już istnieją. Istnieją też przyjęcie według adresu, budżety ramek i logowania, limit ramki parsera 16384 bajtów, odczyt aplikacji 8192 bajtów i kolejki wyjścia. Ten etap ich nie zastępuje. To nie jest limit pasma, ochrona przed DDoS ani zamknięcie zagrożenia szczątkowego T13.

## Kolejka nasłuchu

`listen` używa `listen_backlog` (128) zamiast `SOMAXCONN`. Jądro może nadal zastosować mniejszy `somaxconn`. Istniejący test procesu otwiera naraz 48 klientów i ta seria mieści się w tej kolejce. Samo gniazdo nasłuchu nie dostaje limitu bufora połączenia. Mały bufor nasłuchu nie ograniczałby pewnie przyjętych gniazd i mógłby zmniejszyć kolejkę accept.

## Przyjęte gniazda

`protect_tcp_socket` działa dopiero po przydziale identyfikatora sesji i przed TLS, i tylko dla gniazda, które hub zatrzyma. Nadmiar połączeń, bany i odmowa przyjęcia nadal zamykają gniazdo przed tym wywołaniem.

Wywołanie ustawia:

- `SO_RCVBUF` i `SO_SNDBUF` na `socket_buffer_bytes` (65536). Linux zgłasza wartość od żądania do dwukrotności żądania, bo dolicza narzut księgowania. Test jednostkowy przyjmuje ten domknięty zakres.
- `TCP_USER_TIMEOUT` na skonfigurowany timeout bezczynności. Wartość opcji jest w milisekundach. Timeout bezczynności to `std::chrono::seconds` i jest już ograniczony do 1–86400 sekund, więc user timeout wynosi 1000–86400000 ms.

`TCP_USER_TIMEOUT` zamyka połączenie, którego wysłane dane pozostają niepotwierdzone przez ten czas. Nie zamyka cichego połączenia bez niepotwierdzonych danych. Cichych klientów nadal zamyka istniejący timeout bezczynności aplikacji. Lokalne testy z jednosekundowym timeoutem bezczynności pozostają ważne, bo potwierdzenia loopback kończą się natychmiast.

Zły deskryptor, timeout poza 1–86400000 ms albo nieudane `setsockopt` zwraca fałsz. Wcześniejsza opcja w tym samym wywołaniu może już być ustawiona. Wywołujący wtedy zwalnia przyjęcie klienta, liczy jedno zwykłe odrzucenie, zapisuje ograniczone ostrzeżenie `TCP socket protection failed; peer rejected` i zamyka gniazdo. Ostrzeżenie korzysta z istniejącego budżetu pięciu sekund. Reaktor pracuje dalej. Identyfikator sesji zwalnia uchwyt pętli accept. Nie dodano nowej przyczyny zamknięcia.

Bufor odczytu aplikacji to dotychczasowe 8192 bajty, teraz nazwane `read_buffer_bytes`. Każda partia accept nadal obsługuje co najwyżej 64 klientów. Każda tura odczytu klienta to nadal co najwyżej 8 × 8192 bajty. `TCP_NODELAY` nie jest ustawiane. Prywatne gniazdo Unix API nie jest gniazdem TCP i pozostaje bez zmian.

## Bez zmian

Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, strony WebAdmin, ciągu językowego, klucza INI ani ustawienia Apache. Check status zostaje na GitHub-hosted Ubuntu 24.04. HTTP/2, OCSP, automatyczne odnawianie i publiczna uwierzytelniona brama pozostają niezaimplementowane. Klient, który przejdzie przyjęcie, nadal może zużywać CPU na TLS i hashowanie hasła. Limity buforów jądra nie są limitem pasma w bajtach.

Istniejący test `network.unit` sprawdza trzy stałe, odrzuca zły deskryptor i zerowy timeout oraz sprawdza obie opcje bufora i `TCP_USER_TIMEOUT` na jednym gnieździe TCP loopback. Nie dodano nowego CTest.

Fixture terminu zamknięcia nadal wymusza 1024-bajtowy bufor wysyłania serwera po tym limicie i ustawia `TCP_USER_TIMEOUT` na 120 sekund. Ten fixture sprawdza termin opróżnienia aplikacji. Nie zmienia huba, w którym user timeout jest równy timeoutowi bezczynności.
