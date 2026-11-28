# Automatyczne odnawianie certyfikatów TLS

Panel **Ustawienia → Bezpieczeństwo → Certyfikaty TLS** pokazuje certyfikat, harmonogram i wynik weryfikacji usług. Właściciel i administrator mogą czytać status i sprawdzić gotowość. Zmiana harmonogramu wymaga aktualnego hasła właściciela i sesji założonej w ciągu ostatnich pięciu minut po potwierdzeniu ośmiocyfrowego kodu e-mail.

## Zakres wdrożenia Ubuntu

WebAdmin na Ubuntu używa HTTPS na TCP 7178, a publiczny hub ADCS korzysta z TCP 1512. Apache przekazuje dozwolone ścieżki API do Node na 127.0.0.1:4278. MariaDB i oba API C++ korzystają z prywatnych gniazd Unix. Historyczny WebAdmin na Debianie 13 używa TCP 7278.

Kontrola Ubuntu 24.04 wykazała Certbota 2.9.0, jedną linię certyfikatu z metodą standalone HTTP-01, jawny adres IPv4 i wyłączony standardowy timer. Certyfikat zawiera dokładny SAN `server-747585786.dc24h.eu`. Apache korzystał z linii Certbota, hub z osobnej kopii i zgłaszał `CanReload=no`. Podczas inspekcji port 80 nie miał lokalnego nasłuchu. Przed zgodą sprawdź własność DNS i dostępność TCP 80 z Internetu.

Mechanizm obsługuje jeden publiczny DNS SAN, istniejącą linię Certbota z pakietów Ubuntu oraz standalone HTTP-01 na zatwierdzonym IPv4, z produkcyjnym katalogiem Let's Encrypt. Zmiany DNS, firewalla, portów, metody, zakresu nazw lub użycie danych dostawcy DNS wymagają osobnej decyzji właściciela. Potwierdź HTTP-01 w uzgodnionym środowisku testowym z Let's Encrypt staging. Przycisk sprawdzenia gotowości bada warunki lokalne i certyfikat obu usług bez składania zamówienia ACME. [Dokumentacja Certbota](https://eff-certbot.readthedocs.io/en/stable/using.html#renewing-certificates)

## Przygotowanie po zgodzie właściciela

1. Zapisz zaakceptowany SHA. Wykonaj chronioną kopię konfiguracji, plików wykonywalnych i zasobów WebAdmin, certyfikatu z kluczem, vhosta Apache, jednostek systemd i danych Certbota. Potwierdź dostęp awaryjny i ważność certyfikatu do odtworzenia.
2. Zweryfikuj linię certyfikatu i globalne `cli.ini`. Zachowaj standalone, właściwy adres IPv4, zatwierdzony urząd certyfikacji i standardowe ścieżki Certbota. Dopuszczalne są ustawienia logów i odnowienia z pakietu Ubuntu oraz stały hook tego projektu. Sprawdź inne timery i zadania Certbota. Przed zmianą wspólnego harmonogramu zaplanuj obsługę innych certyfikatów.
3. Przygotuj Certbot, ConfigObj i cryptography z pakietów Ubuntu. Zainstaluj [jednostki TLS](../../config/tls-renewal/) z właścicielem root i pozostaw timer odnawiania wyłączony. Umieść `scripts/tls_renewal.py` pod `/usr/local/libexec/dc24h-tls/tls_renewal.py`, a wykonywalny `scripts/tls_certbot_deploy_hook.sh` pod `/usr/local/libexec/dc24h-tls/certbot-deploy-hook`. Tylko root może zapisywać te katalogi i pliki.
4. Utwórz należące do roota katalogi `/etc/dc24h-tls` (0755) i `/var/lib/dc24h-tls` (0700). Utwórz `/etc/dc24h-tls/renewal.json` z pliku przykładowego z trybem 0600. Ustaw zatwierdzoną domenę, nazwę linii i IPv4. Po decyzji właściciela i kwalifikacji ACME wpisz w `approval` wartość `OWNER_TLS_HTTP01_ACTIVATION_GO`. Przygotuj aktualny zestaw certyfikatu do pierwszego wdrożenia.

   ```sh
   sudo /usr/bin/python3 -I /usr/local/libexec/dc24h-tls/tls_renewal.py bootstrap
   ```

5. W zatwierdzonym oknie wdrożenia ustaw `SSLCertificateFile` i `SSLCertificateKeyFile` vhosta `dc24h-stage4-webadmin.conf` na `fullchain.pem` i `privkey.pem` w `/etc/dc24h-tls/served/current`. Zachowaj TCP 7178 w Listen, VirtualHost, regułach Host/Origin i konfiguracji Node. Wprowadź ścisłe reguły proxy z poprawionego przykładu Stage IV. Przed łagodnym przeładowaniem wykonaj `apache2ctl configtest`.
6. W `[tls]` huba wskaż te same dwa pliki. Włącz prywatne `[api]` z `socket = /run/dc24h-stage3-api/hub.sock` i `token_file = /var/lib/dc24h-stage3/api.token`. Utwórz 64-znakowy token szesnastkowy, należący do `dc24h-stage3`, o trybie 0600 albo 0400. Do pliku roota `/etc/dc24h-tls/hub.env` wpisz `DC24H_TLS_RELOAD_HOSTNAME=<zatwierdzona domena>`. Zainstaluj plik drop-in `config/tls-renewal/adcs-reload.conf.example`. Wdróż odpowiadające sobie wersje huba, usługi kont C++, mostu, gatewaya i zasobów WebAdmin. Uzgodnij z właścicielem pierwszą wymianę binariów. Rutynowe odnowienie korzysta z `ExecReload`.
7. Przeładuj definicje systemd i aktywuj wyłącznie gniazdo zarządzania poleceniem `systemctl enable --now dc24h-tls-control.socket`. Sprawdź prywatne usługi C++, oba punkty TLS i polecenie łagodnego przeładowania Apache. Obie usługi muszą używać przygotowanego certyfikatu. Timer włączy właściciel z panelu.

## Obsługa i awaria

Wybierz **Sprawdź gotowość**. Po pozytywnym wyniku wybierz **Włączone**, wpisz aktualne hasło i zastosuj ustawienie. Systemd planuje kontrole na 03:00 i 15:00 UTC z losowym przesunięciem do 30 minut oraz odtwarza pominiętą kontrolę po restarcie. Certbot decyduje, kiedy certyfikat trzeba odnowić. Panel pokazuje faktyczny stan timera i ostatni odczyt obu usług.

Zadanie sprawdza łańcuch CA, dokładny SAN, ważność i zgodność klucza. Zastępuje zestaw jednym atomowym przełączeniem dowiązania, sprawdza konfigurację Apache i weryfikuje certyfikat obu usług po przeładowaniu. Istniejące sesje huba zachowują swoje konteksty TLS. Daty w panelu pochodzą z pól ważności certyfikatu. Zmiana na **Wyłączone** usuwa harmonogram i zachowuje działający zestaw. Jeśli trwa odnowienie, ponów zmianę po zwolnieniu blokady.

Nieudane zastosowanie uruchamia powrót do poprzedniego zestawu i weryfikację obu usług. Po przerwaniu zadania chroniony zapis transakcji pozwala dokończyć odtworzenie. W razie nieskutecznego rollbacku sprawdź ważność poprzedniego certyfikatu, stan usług i dziennik `/var/lib/dc24h-tls/audit.jsonl`, journal systemd i chronione logi Certbota. Zachowaj stan transakcji, archiwum i konto Certbota do odzyskania.

Przy planowanym rollbacku wyłącz harmonogram, zaczekaj na zakończenie zadania i odtwórz razem zweryfikowany certyfikat oraz dopasowaną konfigurację. Sprawdź Apache przez `apache2ctl configtest` i wykonaj łagodne przeładowanie. Przy wycofaniu binariów huba lub zmianie ścieżek zastosuj zatwierdzone okno serwisowe. Potwierdź nowe połączenia TLS do obu punktów z poprawnym SAN, zaufanym CA i oczekiwanym odciskiem. Zaktualizuj ręczne odciski KEYP zaufanym kanałem uzgodnionym z klientami.
