# Monitorowanie — alpha-0.9.11

CPU, pamięć, sesje i opóźnienia. Gałąź powstaje z main `402fa10`. Nazwa gałęzi to `alpha-0.9.11`.

Hub publikuje jedną próbkę na istniejącym `GET /api/v1/status`, które wymaga bearer. Health pozostaje bez zmian. Nie ma nowej trasy, klucza INI, migracji schematu, ustawienia Apache ani strony WebAdmin. Istniejący panel pokazuje próbkę, gdy obiekt jest obecny.

## Proces

Próbka czyta tylko `/proc/self/stat` w wątku API. Nie czyta wiersza polecenia, środowiska, innego procesu ani cgroup.

`cpu_user_ms` i `cpu_system_ms` to skumulowany czas użytkownika i systemu w milisekundach. Procesy potomne nie wchodzą do licznika. `rss_bytes` to liczba stron rezydentnych razy rozmiar strony. To nie jest limit i to nie jest wykrywanie wycieku.

`cpu_window_ms` to czas ścienny od poprzedniej udanej próbki. `cpu_delta_ms` to czas użytkownika plus systemu zużyty w tym oknie. Pierwsza próbka, nieudany odczyt i dwa odczyty w tej samej milisekundzie podają okno zerowe. Kilka wątków może sprawić, że delta jest większa niż okno. To nie jest CPU hosta.

Nieudany odczyt ustawia `available` na false i liczby procesu na zero. Hub nie zapisuje rekordu do logu.

## Sesje

Obiekt kopiuje bieżące liczniki faz `sessions_accepted`, `sessions_negotiating`, `sessions_identified`, `sessions_ready`, `sessions_closing` i `sessions_closed` oraz `sessions_created` i `sessions_completed`. `sessions_ready` jest tym samym rodzajem licznika co `users.online`, ale jest próbkowany osobno, więc te liczby mogą się różnić. Nie ma nicka, SID, CID, adresu ani konta.

## Opóźnienia

`login_last_ms` i `login_max_ms` mierzą czas steady clock od przyjęcia sesji do przejścia w gotowość. `login_samples` liczy te przejścia. Nieudane albo powtórzone wywołanie ready nie jest próbką. To nie jest czas obiegu do klienta.

`reactor_last_ms` i `reactor_max_ms` mierzą jeden przebieg pracy reaktora między oczekiwaniem, w pełnych milisekundach. `reactor_samples` liczy te przebiegi. Samo oczekiwanie nie wchodzi do pomiaru. Wolny dysk albo duża porcja accept może podnieść maksimum. To nie jest opóźnienie jednego użytkownika i nie jest zapisywane.

## Granice

Próbki są niezależne. Nic nie jest zapisywane do MariaDB, do pliku ani do zdalnego kolektora. Nie ma punktu Prometheus, historii ani listy opóźnień poszczególnych sesji. Schemat v9 pozostaje bez zmian. `Require local` zostaje. Status i health nadal wymagają tylko bearer. To nie zamyka zagrożenia szczątkowego T13.

`monitoring.unit` sprawdza rekord stat, przeliczenie taktów i stron, przepełnienie oraz żywą próbkę procesu. `users.statistics` sprawdza próbki od accept do ready. `api.process` sprawdza obiekt statusu.
