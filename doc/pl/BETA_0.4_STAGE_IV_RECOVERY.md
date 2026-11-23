# Beta 0.4.15 — odtwarzanie po awarii w Etapie IV

Instrukcja **próby na odizolowanej instalacji**, nie potwierdzenie poprawnego odzyskania danych produkcyjnych. Nie wykonywano awarii ani przywrócenia bazy na docelowym Debianie 13. RPO, RTO i spójność bazy: **NOT MEASURED**.

## Przygotowanie spójnej kopii

1. Uzyskaj zgodę na próbę i obecność drugiego, niezależnego operatora. Przygotuj osobne konta, testową instancję MariaDB, Apache2, lokalny hub i bramę. Zapisz SHA, schemat **v11**, wersje procesów i aktualny czas.
2. Zatrzymaj lokalną bramę, przerwij zapisy huba do bazy i upewnij się, że procesy rzeczywiście nie zapisują plików. Opcja `--ack-stopped` to potwierdzenie operatora, nie automatyczny detektor uruchomionych usług. Nigdy nie kopiuj działającego dziennika anty-replay, kluczy lub limitera z niezakończonym zapisem.
3. Zrób spójny `mariadb-dump` przez prywatne gniazdo Unix, z konfiguracją uwierzytelniania `0600`; sprawdź kod wyjścia, sumy i liczbę wierszy. Nie przywracaj dumpa mogącego usuwać bazy do działającego serwera produkcyjnego. Użyj **nowej, osobnej instancji**.
4. Zachowaj jako jeden zestaw pięć prywatnych plików bramy: TOTP, replay, audit, passkeys i rate limits. Kopia poza serwerem musi być zaszyfrowana i kontrolowana. Narzędzie nie wysyła danych do sieci.

Przykłady na **zatrzymanej instalacji testowej**; katalogi nadrzędne prywatne `0700`, pliki `0600`, bez dowiązań symbolicznych:

```sh
python3 scripts/stage4_private_recovery_drill.py snapshot --source /var/lib/dc24h-remote --destination /var/lib/dc24h-rehearsals/snapshot-001 --ack-stopped --ack-isolated
python3 scripts/stage4_private_recovery_drill.py verify --source /var/lib/dc24h-rehearsals/snapshot-001 --ack-isolated
python3 scripts/stage4_private_recovery_drill.py restore --source /var/lib/dc24h-rehearsals/snapshot-001 --destination /var/lib/dc24h-rehearsals/restored-001 --ack-stopped --ack-isolated
```

`restore` zapisuje wyłącznie do **nowego, oddzielnego** katalogu; katalog docelowy nie może leżeć wewnątrz źródła ani kopii zapasowej. Narzędzie nie zastępuje danych aktywnych usług i niczego nie uruchamia. Ponownie sprawdza SHA-256 przy kopiowaniu, porównuje pięć odtworzonych plików z oryginalnym manifestem, kontroluje rozmiary, formaty MFA/passkey/replay/limiter, kanoniczny czas UTC w manifeście i łańcuch haszy audytu. Poprawny format czasu nie dowodzi jego autentyczności. Narzędzie odrzuca kopie zmodyfikowane, ucięte, niespójne i dowiązania. **Po błędzie może pozostać niekompletny katalog docelowy; nie wolno go instalować.** Odtworzony katalog zawiera tylko pięć plików usług, bez manifestu. Manifest ani pliki prywatne nie mogą trafić do artefaktów GitHub CI.

## Scenariusze obowiązkowe

1. **Niedostępność MariaDB:** obserwuj odmowy operacji, czas awarii i poprawność uprawnień po przywróceniu usługi. Nie dopuszczaj użycia starych grantów ani wyników z pamięci podręcznej.
2. **Restart huba, Apache2 i bramy:** sprawdź nasłuch wyłącznie lokalny, poprawną nazwę i łańcuch TLS, sesje/CSRF unieważnione po restarcie bramy oraz wymaganie świeżego hasła i TOTP/passkey.
3. **Uszkodzony audit/replay/passkeys:** system musi odmówić dostępu (fail closed). Zabezpiecz oryginał do analizy, nie kasuj historii, nie nadpisuj jej pustym plikiem. Odtwarzaj wyłącznie zgodny, wcześniej zweryfikowany zestaw.
4. **Próba odtworzenia:** importuj MariaDB do nowej instancji i sprawdź schemat v11, liczbę rekordów, uprawnienia i integralność prywatnego zestawu. Wykonaj negatywne testy odtwarzania sesji i replay, bez ujawniania danych kont.
5. **Rollback:** zatrzymaj instalację testową, przywróć poprzednio przypiętą binarkę/konfigurację oraz odpowiadającą jej bazę i prywatne pliki. Etap IV **nie** wprowadza migracji SQL. Po testach ponowny dostęp jest odrębną, ręczną decyzją.

| Dowód | Stan |
| --- | --- |
| Jedna spójna kopia MariaDB + prywatnych plików | **NOT MEASURED** |
| Awaria/odtworzenie MariaDB i restart usług | **NOT MEASURED** |
| Negatywne testy offline uszkodzonych plików | Lokalnie zaliczone; serwer docelowy **PENDING** |
| Przywrócenie do osobnej instancji i kontrola grantów | **NOT MEASURED** |
| RPO (wiek ostatniej poprawnej kopii), RTO (czas do bezpiecznego powrotu) | **NOT MEASURED** |

Wyniki testów Python dotyczą jedynie bezpiecznego działania **narzędzia offline**. Nie dowodzą działania MariaDB ani prawdziwego RPO/RTO. Wymagane niezależne sprawdzenie i ręczne zatwierdzenie. Dokument źródłowy: [EN/US](../us/BETA_0.4_STAGE_IV_RECOVERY.md). Lista zatwierdzeń: [Etap IV](BETA_0.4_STAGE_IV.md).
