# Bezpieczeństwo logów — alpha-0.9.10

Bezpieczne logi i rejestrowanie zdarzeń. Gałąź powstaje z main `3237062`. Nazwa gałęzi to `alpha-0.9.10`.

Hub nadal zapisuje synchroniczne wiersze UTC na stderr albo do jednego pliku dopisywanego. Ten etap odrzuca log, który może czytać inny użytkownik, i zapisuje stały zestaw skutków bezpieczeństwa nawet przy wyłączonym logowaniu połączeń.

## Prywatny plik

Nowy plik logu powstaje z `O_EXCL` i `O_NOFOLLOW`, a potem dostaje tryb `0600`. Musi być zwykłym plikiem, mieć jedno dowiązanie i należeć do efektywnego użytkownika. Istniejący plik jest dopisywany tylko wtedy, gdy ma już tryb `0600` i tego samego właściciela. Hub nie zmienia trybu istniejącego pliku. Tryb taki jak `0640` albo `0644`, inny właściciel, końcowe dowiązanie symboliczne, FIFO, urządzenie albo drugie dowiązanie twarde są odrzucane. Katalog nie jest tworzony. `--log-file -` nadal wybiera stderr. `--check-config` nadal nie otwiera pliku.

Restrykcyjna maska umask nie zostawia nowego pliku bez zapisu właściciela. Plik nie jest rotowany, ponownie otwierany ani wysyłany do zdalnego kolektora. Wolny dysk nadal może zablokować wywołującego. To nie zamyka zagrożenia szczątkowego T13.

## Zapisy bezpieczeństwa

Przy `info` albo `warning` te skutki są zapisywane nawet przy wyłączonym logowaniu połączeń:

`tls_handshake_failed`, `handshake_timeout`, `security_quota`, `partial_frame_timeout`, `login_rejected`, `login_timeout`, `negotiation_limit`, `identity_limit`, `authentication_revoked`, `authentication_limit`, `kicked`, `banned`.

Wiersz to `Security record=` oraz ta stała nazwa. Nie ma w nim identyfikatora połączenia, adresu, portu, nicka, CID, PID, hasła ani tekstu ADC. W każdym jednosekundowym oknie zapisywanych jest co najwyżej 32 wierszy. Dalsze są liczone. Jedno ostrzeżenie, `Suppressed security records=`, powstaje w następnym oknie albo przy zatrzymaniu loggera. `error` i `off` nie zapisują tych wierszy. Opcjonalne wiersze połączeń mają własny budżet i nadal wymagają `connections=on` oraz `level=info`.

Te zapisy nie są śladem audytu. Nie mają aktora ani celu. Odmowy utrzymania, nieudany CSRF i użycie grantu nie są wierszami i nie są tymi liniami. To nie zamyka zagrożenia szczątkowego T11. Produkcyjny audyt nie jest zaimplementowany. Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, strony WebAdmin, klucza INI ani ustawienia Apache. `Require local` zostaje.

`logging.unit` sprawdza tryb tylko dla właściciela, odmowę istniejącego pliku, stałe zapisy i budżet 32 na sekundę. `logging.process` sprawdza odmowę startu przy pliku o trybie `0644` i jeden zapis `partial_frame_timeout` bez tekstu klienta.
