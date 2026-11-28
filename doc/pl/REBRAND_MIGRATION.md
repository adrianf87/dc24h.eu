# Migracja usług dc24h.eu i powrót do poprzedniej wersji

Zmianę nazwy śledzi [zgłoszenie #36](https://github.com/adrianf87/dc24h.eu/issues/36). Wdrażaj i wycofuj razem binaria huba i narzędzi oraz lokalny, zdalny i publiczny WebAdmin. Walidatory statusu w obu wersjach oczekują innych identyfikatorów serwera mimo tego samego numeru wersji programu.

## Punkt odniesienia migracji

Punktem odniesienia jest chroniona gałąź `main` na commicie `cfd979d4a9f14b158dfc2f35f270ab2d6e2a055e`. Repozytorium GitHub jest dostępne jako `adrianf87/dc24h.eu`. W istniejącej kopii użyj `git remote set-url origin https://github.com/adrianf87/dc24h.eu.git`. Historyczne tagi i raporty zachowują przypisanie do swoich rewizji i przebiegów CI.

- Nazwa w CLI, powitaniu ADC i WebAdminie to `dc24h.eu`. Instalowane programy to `dc24h`, `dc24h-owner`, `dc24h-account-admin` i `dc24h-service-controller`. Pakiet npm to `dc24h-webadmin`.
- Hub C++ udostępnia Node.js API statusu v1 przez uwierzytelnione gniazdo Unix. Lokalny i zdalny WebAdmin wymagają `server.product_id = "dc24h"` oraz `server.name = "dc24h.eu"`. Poprzedni WebAdmin wymaga nazwy Twilight. Przy mieszanych wersjach widoki statusu odrzucają odpowiedź do czasu uruchomienia zgodnych wersji.
- Apache2 obsługuje publiczne HTTPS i przekazuje żądania do lokalnej bramy Node.js. Widoki React korzystają z sesji istniejącego mechanizmu logowania. Brama Etapu IV używa MariaDB do obsługi tożsamości i sesji oraz prywatnego mostu do C++. Zachowaj zatwierdzone dla instancji DNS, TLS, WebAuthn RP ID, Origin, konta usługowe, ścieżki gniazd i dane trwałe.
- Bieżące workflow Ubuntu 22.04, 24.04 i 26.04 używają kanonicznej nazwy repozytorium. Etap V obejmuje zmiany źródłowe w PR-ach i uruchamia się po pushu do `main`. Starsze weryfikatory Beta sprawdzają pierwotne repozytoria i formaty artefaktów. [GitHub Actions CI](GITHUB_CI.md)

## Kontrola wstępna i kopia zapasowa

1. Na izolowanym Ubuntu zapisz SHA źródła, sprawdź `--version` czterech binariów i uruchom `systemd-analyze verify` dla jednostek przeznaczonych do instalacji.
2. Sprawdź definicje usług, dowiązania, drop-iny i stan aktywności oraz włączenia. Nazwę `dc24h.service` może nadal zajmować jednostka High Starter. Uwzględnij jednostki danych instancji, np. `dc24h-stage3-adcs.service`, oraz procesy uruchamiane ręcznie, które mogą korzystać z tych samych danych lub portów.
3. Zachowaj binaria, artefakty WebAdmin, definicje systemd, drop-iny, regułę Polkit i listę włączonych jednostek. Przygotuj i sprawdź zgodną kopię prywatnej konfiguracji, danych i bazy MariaDB według procedury odzyskiwania instancji.
4. Potwierdź faktyczny host i port HTTPS, Origin, WebAuthn RP ID, konfigurację Apache i lokalny port Node. Przykład repozytorium na 7278 oraz konfiguracja instancji na 7178 wymagają odrębnej weryfikacji.

## Przełączenie i weryfikacja

Po akceptacji OWNER zatrzymaj bramy i kontrolery, a potem hub i prywatne usługi. Wyłącz jednostki Twilight i sprawdź zakończenie procesów. Dla zajętej nazwy docelowej zachowaj poprzednią jednostkę w kopii i jawnie usuń ją z aktywnej konfiguracji. Po `systemctl daemon-reload` potwierdź, że nazwa jest wolna. Zainstaluj nowe pliki usług pod wolnymi nazwami. Instalator Stage IV odrzuci kolizję z istniejącą jednostką.

Dla huba, kontrolera i bramy zdalnej przygotuj właściwą regułę Polkit i nowy, należący do roota plik `/etc/dc24h/dc24h-unit-migration-approved`. Przeładuj definicje systemd i uruchom wyłącznie komponenty przewidziane dla instancji. Konflikty w systemd mogą zatrzymać odpowiadającą starą jednostkę podczas startu nowej, dlatego zatrzymuj stare usługi jawnie przed instalacją. Sprawdź ADC/ADCS, prywatne API, logowanie z MFA i odczyty WebAdmin, stan jednostek oraz konfigurację Apache. Przy używaniu gniazda odnowienia TLS sprawdź usługę `dc24h-stage4-auth.service`.

## Wycofanie

Zatrzymaj i wyłącz nowe usługi przed przywróceniem starej wersji. Usuń nowe jednostki i znacznik migracji. Przywróć binaria, WebAdmin, jednostki, drop-iny, Polkit i poprzedni stan znacznika z kopii. Uruchom `sudo systemctl daemon-reload` i wznów poprzednie usługi według zapisanej listy. Dla Twilight uruchom hub przez `sudo systemctl start dc24h-twilight.service` i sprawdź go z odpowiadającą mu wersją WebAdmin.

[Angielska procedura z poleceniami](../us/REBRAND_MIGRATION.md)
