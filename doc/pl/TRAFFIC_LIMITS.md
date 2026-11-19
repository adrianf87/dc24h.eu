# Limity ruchu — alpha-0.9.4

Spam i nadmierny ruch obecności. Ten etap ogranicza aktualizacje INF gotowej sesji, zanim zostaną zapisane i rozesłane. Gałąź powstaje z main `2daeb20`. Nazwa gałęzi to `alpha-0.9.4`.

Wiadomości czatu mają już kubełek żetonów. Zapytania i wyniki wyszukiwania mają własny. Żądania połączenia wychodzące i przychodzące mają własny. Nadal działa ogólny limit ramek. Ten etap nie zastępuje tych budżetów i nie miesza aktualizacji obecności z kubełkiem czatu.

## INF gotowej sesji

Każde połączenie ma jeden stały `PresenceGuard`. To nie jest klucz INI. Kubełek startuje pełny: 8 aktualizacji, potem 4 aktualizacje na sekundę. INF logowania pozostaje stanem identify i nie jest naliczany. Kolizja nicku jest odrzucana przed tym naliczeniem.

Każdy inny INF, który dochodzi do stanu normal, jest naliczany, także aktualizacja złożona tylko z nieznanych pól i aktualizacja, która później nie przejdzie sprawdzenia pola. Odmowa nie zapisuje aktualizacji, nie rozsyła jej i nie rozłącza. Nadawca dostaje co najwyżej jeden odzyskiwalny `ISTA 100` na sekundę, `Presence update rate limit exceeded`, wraz z fourcc tego polecenia. Kolejna odmowa w tej samej sekundzie jest cicha. Strażnik nie zachowuje tekstu pól.

STA zostaje poza tym kubełkiem, więc peer nadal może zgłosić wynik połączenia w istniejącym limicie ramek. Keepalive zostają na tym limicie ramek. Test porządkujący, który potrzebuje wielu znaczników, używa STA, a nie INF.

## Bez zmian

Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, strony WebAdmin, ciągu językowego, klucza INI ani ustawienia Apache. Check status zostaje na GitHub-hosted Ubuntu 24.04. To nie jest limit pasma, nie jest ochroną przed DDoS i nie zamyka zagrożenia szczątkowego T13. Klient, który przejdzie przyjęcie, nadal może zużywać CPU na TLS i hashowanie hasła. HTTP/2, OCSP, automatyczne odnawianie i publiczna uwierzytelniona brama pozostają niezaimplementowane.

`adc.presence` sprawdza zapas, ułamkowe uzupełnienie, jedno powiadomienie na sekundę, zegar cofnięty oraz to, że jedna wiadomość czatu nadal przechodzi po odmowie obecności. `adc.presence.process` sprawdza ten sam limit na ADC i ADCS, w tym cichą drugą odmowę i niezależną wiadomość czatu.
