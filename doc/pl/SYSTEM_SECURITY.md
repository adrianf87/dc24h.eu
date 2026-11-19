# Bezpieczeństwo systemu — alpha-0.9.9

Utwardzenie usługi na Debianie i Ubuntu. Gałąź powstaje z main `21865ac`. Nazwa gałęzi to `alpha-0.9.9`.

Hub nadal działa na pierwszym planie i kończy po SIGTERM. Nie demonizuje się, nie instaluje jednostki i nie uruchamia usługi. Ten etap dodaje jeden przykład jednostki i jedno zablokowane konto systemowe. Skopiowanie plików ich nie włącza.

## Jednostka

`config/dc24h.service.example` ma `Type=simple`. `ExecStart` to `/usr/local/bin/dc24h --config /etc/dc24h/dc24h.ini`. Zmień tę ścieżkę, gdy prefiks instalacji nie jest `/usr/local`. Użytkownik i grupa to `dc24h`. Zbiór uprawnień jest pusty, więc port nasłuchu poniżej 1024 jest odrzucany. Włączone są `NoNewPrivileges`, `ProtectSystem=strict`, `ProtectHome`, `PrivateTmp` i `MemoryDenyWriteExecute`. Rodziny adresów to tylko `AF_UNIX`, `AF_INET` i `AF_INET6`. Filtr wywołań systemowych to `@system-service`. Awaria uruchamia ponownie co najwyżej 5 razy w ciągu 60 sekund. Czysty SIGTERM nie uruchamia ponownie.

`ProtectSystem=strict` zostawia `/usr`, `/etc` i resztę hierarchii tylko do odczytu. Wyjątki zapisywalne to katalogi, które systemd tworzy dla tej usługi:

- `/var/lib/dc24h`, tryb `0700`, na stan i pliki sekretów tylko dla właściciela
- `/run/dc24h`, tryb `0700`, na gniazdo API
- `/var/log/dc24h`, tryb `0750`, na plik dziennika. Hub dopisuje ten plik tylko wtedy, gdy sam plik ma tryb `0600` i należy do użytkownika usługi. Zobacz [Bezpieczeństwo logów](LOGGING_SECURITY.md).

`/etc/dc24h` ma tryb `0755` i nie jest magazynem sekretów. Nie umieszczaj tam hasła bazy, tokena API ani klucza ADC. Trzymaj te pliki w trybie `0600` w `/var/lib/dc24h`. Skieruj gniazdo API na `/run/dc24h`, co spełnia dotychczasową regułę prywatnego katalogu. Ścieżka kopii też musi być w zapisywalnym katalogu stanu. Członkostwo w grupie gniazda MariaDB zostaje przy operatorze.

## Konto

`config/dc24h.sysusers.example` opisuje użytkownika systemowego `dc24h` z powłoką `/usr/sbin/nologin` i katalogiem domowym `/var/lib/dc24h`. Budowanie nie uruchamia `systemd-sysusers` na hoście. Utwórz konto dopiero przy włączaniu jednostki.

## Bez zmian

Przykład nie jest kopiowany do `/etc/systemd/system` i `systemctl enable` nie jest wykonywane. Nie ma polityki AppArmor ani SELinux, zapory ani nienadzorowanej aktualizacji. To nie zamyka zagrożenia szczątkowego T07: root i każdy inny proces użytkownika `dc24h` nadal może otworzyć gniazdo. Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, strony WebAdmin, klucza INI ani ustawienia Apache. `Require local` zostaje.

`systemd.sandbox` sprawdza wysłane dyrektywy, weryfikuje jednostkę, tworzy konto wyłącznie w prywatnym korzeniu, uruchamia hub pod tymi ograniczeniami jako `nobody`, przyjmuje połączenie TCP, wiąże gniazdo API w trybie `0600`, zdejmuje efektywne uprawnienia, odmawia zapisu poza katalogami usługi i kończy po SIGTERM. Gdy test nie jest rootem, a sudo bez hasła działa, uruchamia się ponownie z tą samą binarką huba i tymi samymi dwoma przykładami. Bez roota i bez sudo kontrole żywej usługi są pomijane, a kontrole dyrektyw nadal działają.
