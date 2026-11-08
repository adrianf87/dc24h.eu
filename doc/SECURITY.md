# Bezpieczeństwo

Aktualny Core nie przyjmuje ruchu sieciowego, nie przetwarza kont i nie
przechowuje haseł. Uruchamiaj go jako nieuprzywilejowany użytkownik.
Sygnały są odbierane synchronicznie; asynchroniczny handler nie wywołuje
kodu C++, loggera ani operacji blokujących. Nie próbujemy przechwytywać
SIGSEGV, SIGKILL ani SIGSTOP.

Nie umieszczaj sekretów, prywatnych kluczy TLS ani plików `.env` w repozytorium.
Logowanie danych klientów będzie wymagało ochrony przed wstrzykiwaniem linii
logów i usuwania sekretów; obecny logger obsługuje komunikaty wewnętrzne.

Przed włączeniem przyszłych modułów wymagane będą: limity rozmiarów wiadomości
i sesji, timeouty, ochrona przed floodem, walidacja ADC, TLS, bezpieczny hash
haseł (plan Argon2id z audytowanej biblioteki), kontrola dostępu i parametryzowane
zapytania SQL. Te zabezpieczenia sieciowe i uwierzytelnianie nie są jeszcze
zaimplementowane; alpha-0.1.1 nie jest wydaniem produkcyjnym huba.
