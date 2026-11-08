# Bezpieczeństwo

Alpha-0.1.2 przyjmuje surowe połączenia TCP. Domyślnie nasłuchuje wyłącznie
na `127.0.0.1:1511`; publiczny adres wymaga jawnej konfiguracji. Nie obsługuje
kont, uwierzytelniania ani szyfrowania. Nie jest wydaniem produkcyjnym huba.
Uruchamiaj jako nieuprzywilejowany użytkownik.

Network ogranicza liczbę aktywnych klientów, zamyka bezczynne gniazda i używa
stałego bufora bez kumulowania danych. Treść sieciowa jest odrzucana i nie trafia
do logów. Limit dotyczy aktywnych połączeń aplikacji, nie ruchu ani kolejki TCP.
Nie ma limitów per-IP, ochrony przed DDoS ani limitu przepustowości.

Sygnały są odbierane synchronicznie; asynchroniczny handler nie wywołuje
kodu C++, loggera ani operacji blokujących. Nie próbujemy przechwytywać
SIGSEGV, SIGKILL ani SIGSTOP. Gniazda i eventfd mają close-on-exec.

Nie umieszczaj sekretów, prywatnych kluczy TLS ani plików `.env` w repozytorium.
Obecny logger otrzymuje komunikaty wewnętrzne i zwalidowane adresy numeryczne.
Przyszłe logowanie danych klientów wymaga ochrony przed wstrzykiwaniem linii
logów oraz usuwania sekretów.

Przed włączeniem kolejnych modułów potrzebne będą limity wiadomości i buforów
wyjściowych, ochrona przed floodem, walidacja ADC, TLS, bezpieczny hash haseł
(plan Argon2id z audytowanej biblioteki), kontrola dostępu i parametryzowane SQL.
