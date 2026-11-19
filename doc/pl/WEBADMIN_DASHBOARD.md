# Panel WebAdmin — alpha-0.8.11

Ten etap dodaje panel główny WebAdmin i stałe podsumowanie statystyk. Punktem wyjścia jest main `9f72152`. Nie ma automatycznego scalenia. Nazwa gałęzi to `alpha-0.8.11`.

Schemat v9 pozostaje bez zmian. Podsumowanie nie jest zapisywane w MariaDB. Nie dopisuje wiersza audytu, nie tworzy bana, nie wykonuje kick ani mute i nie zmienia konta. `GET /api/v1/status` i `GET /api/v1/health` pozostają dostępne samym tokenem procesu. Udowodniony grant administracyjny nie jest wymagany i nie jest odczytywany przez ten obiekt. Zapisy konfiguracji i bieżąca administracja huba pozostają w planach.

## Obiekt statusu

`dashboard` jest dodawany po `database`, a przed opcjonalnym obiektem `configuration`. `schema` ma liczbę `1`. `core_running` i `database_enabled` powtarzają wartości logiczne już obecne w migawce. Każde pozostałe pole jest dziesiętnym zapisem jednego istniejącego licznika `uint64`, bez zer wiodących. Zapis zachowuje wartości powyżej bezpiecznego zakresu liczb całkowitych JavaScript. Obiekt nie sumuje liczników, nie liczy tempa i nie czyta zegara.

| Pole | Źródło |
| --- | --- |
| `uptime_seconds` | czas pracy ze zegara monotonicznego, już obecny w `server` |
| `users_online`, `users_peak`, `users_logins` | użytkownicy online, szczyt i udane logowania |
| `connections_active`, `connections_peak`, `connections_accepted`, `connections_rejected`, `connections_closed` | te same liczniki sieci |
| `security_rejected`, `security_closed` | liczniki limitów przyjęcia |
| `adc_frames`, `adcs_handshakes` | przyjęte ramki ADC i zakończone negocjacje ADCS |
| `bytes_received`, `bytes_sent` | liczniki bajtów transportu |
| `chat_received`, `chat_public`, `chat_private`, `chat_rejected`, `chat_flood_limited` | próby czatu i te cztery wyniki |
| `search_queries`, `search_routed`, `search_rejected`, `search_results` | odebrane, rozesłane i odrzucone zapytania oraz odebrane wyniki |
| `search_tracked` | przechowywane okna tokenów wyszukiwania. To wskaźnik bieżący, nie łączna liczba zdarzeń |
| `transfers_ctm`, `transfers_rcm` | odebrane żądania CTM i RCM |
| `transfers_peer_unsupported`, `transfers_peer_failed` | zgłoszenia protokołu i połączenia drugiej strony |
| `kicks_succeeded`, `kicks_refused` | wyniki kick |
| `mutes_applied`, `mutes_public_blocked`, `mutes_private_blocked` | zastosowane wyciszenia oraz zablokowane wiadomości publiczne i prywatne |

Kopie kolejek czatu, kopie doręczeń wyszukiwania, kopie kolejek transferów, próby kick i próby mute pozostają w swoich dotychczasowych obiektach. Nie są tu kopiowane ponownie. Nie ma nicku, CID, adresu IP, treści wiadomości, powodu, hasła ani ścieżki.

Liczniki są niezależnymi próbkami, tak jak reszta migawki statusu. Restart procesu je zeruje. Zastępczy proces huba ich nie dziedziczy.

## WebAdmin

`#dashboard` jest pierwszą stroną monitorowania. Domyślną trasą pozostaje `#overview`. Strona jest tylko do odczytu i używa tego samego ręcznego odświeżania oraz odpytywania co pięć sekund. Etykiety EN/US i polskie używają grupowania `en-US` i `pl-PL`. Pełny licznik kick lub mute typu `uint64` jest formatowany z zapisu dziesiętnego.

Każda grupa prowadzi do istniejącej strony szczegółów: użytkownicy do Przeglądu, sieć i transfery do Połączeń, czat do Czatu, wyszukiwanie do Wyszukiwania, a moderacja do Kick. Tamte strony zachowują własne liczniki.

Gdy brakuje `dashboard`, ta strona mówi, że podsumowanie jest niedostępne, i nie wymyśla zer. Pozostałe strony nadal pokazują pola, które już rozumieją. Gdy `dashboard` jest obecny, ale błędny, albo którekolwiek skopiowane pole nie zgadza się ze źródłem, cała migawka jest odrzucana. Licznik, który już jest za duży dla bezpiecznej liczby JavaScript w `network`, `users`, `chat`, `search` lub `transfers`, nadal uniemożliwia odczyt migawki; zapis dziesiętny tego nie obchodzi. Nieznane pola są usuwane i nie trafiają do eksportu.

Panel nie wyświetla kont, banów ani zdarzeń audytu i nie wysyła żądania konserwacji. Lokalne obserwacje wyszukiwania i transferów w przeglądarce zostają na swoich stronach i nie wchodzą do tego podsumowania.
