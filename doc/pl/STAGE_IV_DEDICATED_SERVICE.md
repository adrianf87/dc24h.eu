# Obsługa WebAdmin, etap IV

Publiczny adres to `https://server-747585786.dc24h.eu:7278`. Apache
obsługuje TLS i przekazuje wybrane żądania do Node na `127.0.0.1:4278`.
Hub gościnny etapu III działa na ADCS TCP 1512. ADC TCP 1511 pozostaje wyłączony.

Node działa jako `dc24h-webadmin` i używa dedykowanego konta MariaDB. Proces
C++ obsługuje istniejące tabele kont i weryfikatorów. Uruchamia wyłącznie
prywatne API uniksowe, dzięki czemu Stage III może nadal obsługiwać ADCS.
Most korzysta z UID huba i sprawdza dokładny UID WebAdmin przez
`SO_PEERCRED`. Gniazdo i token C++ są prywatne dla tego UID. Panel obejmuje
rejestrację, logowanie, zmianę hasła i zarządzanie sesjami. Zespół administracji
widzi również stan usługi kont.

## Instalacja na Debianie 13

W konsoli dostawcy VPS, jako root, w zweryfikowanej kopii gałęzi Stage IV:

```sh
apt-get update
apt-get install cmake g++ libssl-dev libgcrypt20-dev libutf8proc-dev libmariadb-dev \
  mariadb-server mariadb-client msmtp apache2 ca-certificates
cmake -S . -B build-stage4 -DCMAKE_BUILD_TYPE=Release
cmake --build build-stage4 --parallel "$(nproc)" --target dc24h dc24h-owner dc24h-account-admin
./scripts/stage4_install.sh /opt/node24-release build-stage4
```

Przed rozpakowaniem Node.js 24 LTS do `/opt/node24-release` sprawdź sumę
archiwum z podpisanym `SHASUMS256.txt` dystrybucji Node.js. Instalator
umieszcza pliki usług i witryny na dysku. Ich włączenie opisano niżej.
Konto `dc24h-stage3` i działający hub powinny już istnieć na VPS.

## Konfiguracja i inicjalizacja

Jeśli plik jeszcze nie istnieje, utwórz go z szablonu jako root:

```sh
install -d -o root -g root -m 0700 /etc/dc24h
test -e /etc/dc24h/stage4-webadmin-bootstrap.env ||
  install -o root -g root -m 0600 config/stage4-webadmin-bootstrap.env.example \
    /etc/dc24h/stage4-webadmin-bootstrap.env
editor /etc/dc24h/stage4-webadmin-bootstrap.env
```

Ustaw `STAGE4_ENVIRONMENT=production`. Wpisz `dc24h_stage4_production`
zarówno w `MARIADB_DATABASE`, jak i `MARIADB_APPROVED_DATABASE`.
Nazwa dedykowanego użytkownika SQL musi kończyć się na `_production`.
MariaDB ma pozostać lokalna, z połączeniem przez gniazdo uniksowe.
Przy typowym ustawieniu Debiana pozostaw `MARIADB_ROOT_PASSWORD` puste,
aby uruchomić inicjalizację przez unix_socket jako root. Wpisz hasło
początkowe `Administrator` (minimum 15 znaków), osobne hasło użytkownika
SQL oraz dane serwera SMTP. Ustaw STARTTLS na 587 lub ustaw
`SMTP_STARTTLS=false` dla bezpośredniego TLS na porcie dostawcy.
Szyfrowanie i weryfikacja certyfikatu są obowiązkowe. Nadawca to
`admin@dc24h.eu`. Wartości są literalne, po pierwszym znaku równości.

```sh
/usr/local/sbin/dc24h-stage4-init
```

Inicjalizacja zatwierdza nazwę bazy, stosuje migracje C++ i wersjonowane
migracje etapu IV, sprawdza schemat oraz uprawnienia i zapisuje weryfikator
hasła obliczony przez C++. Istniejące konto Administrator bez powiązania
Stage IV lub inny OWNER wymaga ręcznej audytowanej migracji. Ponowna
inicjalizacja tej samej bazy zachowuje konto i weryfikator. Po udanej
inicjalizacji wyczyść pola `ADMIN_INITIAL_PASSWORD` i
`MARIADB_ROOT_PASSWORD` w pliku. Usunięcie pliku albo edycja nie usuwa
wcześniejszej zawartości z dysku, migawek, kopii i historii edytora.
Usługi korzystają z osobnych plików runtime. Pliki testowe zapisują się
pod `/etc/dc24h/stage4-test`. Usługa produkcyjna odrzuca bazę testową.

## Uruchomienie, SMTP i HTTPS

```sh
systemctl start dc24h-twilight-stage4-webadmin.service
systemctl status dc24h-twilight-stage4-{auth,bridge,webadmin}.service --no-pager
journalctl -u dc24h-twilight-stage4-webadmin.service --no-pager -n 30
ss -ltn '( sport = :4278 or sport = :7278 or sport = :1512 or sport = :1511 or sport = :3306 )'
```

Próba rzeczywistej wysyłki, z konta usługi, przed otwarciem portu:

```sh
printf 'From: admin@dc24h.eu\nTo: admin@dc24h.eu\nSubject: Stage IV SMTP probe\n\nDelivery check.\n' |
  runuser -u dc24h-webadmin -- /usr/bin/msmtp --file=/var/lib/dc24h-stage4/.msmtprc -t
```

Kod wyjścia zero potwierdza przyjęcie przez dostawcę SMTP. Sprawdź dostarczenie
wiadomości osobno w skrzynce `admin@dc24h.eu`.

Sprawdź DNS, certyfikat Let's Encrypt i inne aplikacje HTTPS na danej nazwie
hosta. Przeglądarka wysyła ciasteczka dla tego hosta także na inne porty HTTPS.
Sprawdź, czy te aplikacje pomijają ciasteczka w logach i nie przekazują ich
do innych usług.

```sh
a2enmod ssl headers rewrite proxy proxy_http
a2ensite dc24h-stage4-webadmin
apache2ctl configtest && systemctl reload apache2
systemctl enable dc24h-twilight-stage4-webadmin.service
```

Otwórz w zaporze dostawcy tylko potrzebny port TCP 7278. Z zewnątrz sprawdź
`curl --fail --silent --show-error https://server-747585786.dc24h.eu:7278/`
oraz połączenie ADCS z weryfikacją certyfikatu. Porty 4278 i 3306 pozostają
lokalne. Port 1511 pozostaje zamknięty.

Zaloguj się jako `Administrator` hasłem początkowym. Po sprawdzeniu hasła
pierwszy kod ośmiocyfrowy trafi na `admin@dc24h.eu`. Wpisz kod w formularzu,
aby aktywować konto. Każde następne logowanie wymaga nowego kodu.
Nowy użytkownik wybiera **Załóż konto**, podaje nazwę, hasło i e-mail oraz
potwierdza kod. Otrzymuje rolę `registered` i loguje się później osobno,
z hasłem i nowym kodem. Kod wygasa po pięciu minutach lub pięciu próbach.
Rejestrację można ponowić po wygaśnięciu oczekującego zgłoszenia,
z uwzględnieniem limitu prób. Zmiana hasła unieważnia otwarte sesje.

```sh
systemctl restart dc24h-twilight-stage4-webadmin.service
systemctl status dc24h-twilight-stage4-{auth,bridge,webadmin}.service --no-pager
```

## Kopia, odzyskiwanie i wycofanie

W zaszyfrowanej kopii przechowuj bazę wraz z procedurami, prywatne pliki
runtime etapu IV i odpowiadające im pliki wersji, Apache i systemd.

```sh
umask 077
backup="/root/stage4-production-$(date -u +%Y%m%dT%H%M%SZ).sql"
mariadb-dump --protocol=socket --single-transaction --routines \
  --databases dc24h_stage4_production >"$backup"
```

Przećwicz odtworzenie na osobnym serwerze MariaDB. Operator przywracania
potwierdza tożsamość i zgłoszenie niezależnym kanałem, a decyzję zapisuje
w chronionym dzienniku. Po utracie skrzynki konto pozostaje zawieszone
do odzyskania skrzynki lub przeprowadzenia audytowanego przekazania własności
przez narzędzie C++. Nie oznaczaj e-maila jako zweryfikowanego ręczną zmianą
bazy. Po odtworzeniu zgodnej kopii i sprawdzeniu migracji, przed otwarciem
dostępu, usuń wpisy z `stage4_sessions` i `stage4_challenges`. To wyczyści
również oczekujące rejestracje.

Wycofanie etapu IV:

```sh
a2dissite dc24h-stage4-webadmin
apache2ctl configtest && systemctl reload apache2
systemctl disable --now dc24h-twilight-stage4-webadmin.service
systemctl stop dc24h-twilight-stage4-bridge.service dc24h-twilight-stage4-auth.service
systemctl is-active dc24h-stage3-adcs.service
```

Odtwarzaj tylko właściwe pliki etapu IV i odpowiadającą im bazę. Pozostaw
działający hub ADCS i certyfikat Let's Encrypt.

## Odbiór

`scripts/stage4_test_database.sh` sprawdza inicjalizację konta, migracje,
C++ i MariaDB, transakcje, rejestrację, kody, symulowane awarie SMTP, restart
bazy oraz kopię i odtwarzanie w środowisku jednorazowym. CI korzysta z
hostowanych przez GitHub obrazów Ubuntu 22.04. Osobno odnotuj
potwierdzony restart systemd na VPS, dostarczenie prawdziwej wiadomości,
aktywację Administratora i weryfikację HTTPS z zewnątrz wraz z SHA wdrożenia.
