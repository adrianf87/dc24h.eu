# Bezpieczeństwo

Alpha-0.1.3 przyjmuje surowe połączenia TCP. Domyślnie nasłuchuje wyłącznie
na `127.0.0.1:1511`; publiczny adres wymaga jawnej konfiguracji. Nie obsługuje
kont, uwierzytelniania ani szyfrowania. Nie jest wydaniem produkcyjnym huba.
Uruchamiaj jako nieuprzywilejowany użytkownik.

Network ogranicza liczbę aktywnych klientów, zamyka bezczynne gniazda i używa
stałego bufora odczytu i ograniczonego bufora ramki każdego klienta (16 KiB).
Parser ogranicza liczbę parametrów do 128, waliduje UTF-8/NFC, nagłówki i escape;
odrzuca błędną ramkę i wraca do odczytu przy następnym LF. Surowe ASCII control
oraz DEL są odrzucane. Treść i parametry nie trafiają do logów. Poprawna składnia
nie uprawnia do wykonania komendy; nie ma jeszcze walidacji sesji, SID/IP peer
ani semantyki pól. Network obecnie nie wykonuje i nie przekazuje wiadomości.
Limit dotyczy aktywnych połączeń aplikacji, nie ruchu ani kolejki TCP.
Nie ma limitów per-IP, ochrony przed DDoS ani limitu przepustowości.

Sygnały są odbierane synchronicznie; asynchroniczny handler nie wywołuje
kodu C++, loggera ani operacji blokujących. Nie próbujemy przechwytywać
SIGSEGV, SIGKILL ani SIGSTOP. Gniazda i eventfd mają close-on-exec.

Nie umieszczaj sekretów, prywatnych kluczy TLS ani plików `.env` w repozytorium.
Obecny logger otrzymuje komunikaty wewnętrzne i zwalidowane adresy numeryczne.
Przyszłe logowanie danych klientów wymaga ochrony przed wstrzykiwaniem linii
logów oraz usuwania sekretów.

Przed włączeniem kolejnych modułów potrzebne będą limity wiadomości i buforów
wyjściowych, ochrona przed floodem, walidacja semantyczna ADC, TLS,
bezpieczny hash haseł
(plan Argon2id z audytowanej biblioteki), kontrola dostępu i parametryzowane SQL.
