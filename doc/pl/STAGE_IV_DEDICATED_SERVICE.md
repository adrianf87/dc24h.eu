# Obsługa WebAdmin, etap IV

Na hoście z jednostkami Twilight lub instalacją o nowych nazwach najpierw wykonaj [procedurę migracji i wycofania](REBRAND_MIGRATION.md).

Zatwierdzony adres Ubuntu to `https://server-747585786.dc24h.eu:7178`.
Historyczny adres Debiana to `https://server-747585786.dc24h.eu:7278`.
W całej procedurze używaj portu zatwierdzonego dla danej instalacji. Apache
obsługuje TLS i przekazuje wybrane żądania do Node na `127.0.0.1:4278`.
Hub gościnny etapu III działa na ADCS TCP 1512.

Node działa jako `dc24h-webadmin` i używa dedykowanego konta MariaDB. Proces
C++ obsługuje istniejące tabele kont i weryfikatorów. Uruchamia wyłącznie
prywatne API uniksowe, dzięki czemu Stage III może nadal obsługiwać ADCS.
Most korzysta z UID huba i sprawdza dokładny UID WebAdmin przez
`SO_PEERCRED`. Gniazdo i token C++ są prywatne dla tego UID. Panel obejmuje
rejestrację, logowanie, zmianę hasła i zarządzanie sesjami. Zespół administracji
widzi również stan usługi kont.

## Instalacja lub aktualizacja zatwierdzonej wersji

Użyj zweryfikowanej dystrybucji Node 24 LTS w `/opt/node24-release`. Przed
aktualizacją wykonaj opisaną niżej zgodną kopię, zapisz stan włączenia witryn
i jednostek, a w zatwierdzonym oknie serwisowym zatrzymaj bramę, most i usługę
uwierzytelniania Stage IV. Zachowaj istniejącą witrynę Apache. Wykonaj
instalację w konsoli dostawcy, jako root.

```sh
apt-get update
apt-get install cmake g++ libssl-dev libgcrypt20-dev libutf8proc-dev libmariadb-dev \
  mariadb-server mariadb-client msmtp apache2 ca-certificates
cmake -S . -B build-stage4 -DCMAKE_BUILD_TYPE=Release
cmake --build build-stage4 --parallel "$(nproc)" --target dc24h dc24h-owner dc24h-account-admin
public_port=7178 # Dla zatwierdzonego historycznego Debiana wybierz 7278.
./scripts/stage4_install.sh /opt/node24-release build-stage4 "$public_port"
```

Przed rozpakowaniem Node.js 24 LTS do `/opt/node24-release` sprawdź sumę
archiwum z podpisanym `SHASUMS256.txt` dystrybucji Node.js. Instalator sprawdza
`/etc/apache2/sites-available/dc24h-stage4-webadmin.conf`. Przy ponownej
instalacji zgodna witryna zachowuje treść, właściciela, uprawnienia i stan
włączenia. Rozbieżność w nasłuchu, VirtualHost, HTTPS, Host lub Origin
przerywa operację. Zmiana zatwierdzonej witryny wymaga jawnej decyzji operatora,
kopii i sprawdzenia `apache2ctl configtest`. Nowy plik powstaje po pomyślnym
zakończeniu pozostałych kroków instalacji. Włączenie Apache, zapora oraz
istniejący certyfikat lub zestaw TLS podlegają odrębnej obsłudze. Konto
`dc24h-stage3` i działający hub powinny już istnieć na VPS.

## Konfiguracja i inicjalizacja

Jeśli plik jeszcze nie istnieje, utwórz go z szablonu jako root:

```sh
install -d -o root -g root -m 0700 /etc/dc24h
test -e /etc/dc24h/stage4-webadmin-bootstrap.env ||
  install -o root -g root -m 0600 config/stage4-webadmin-bootstrap.env.example \
    /etc/dc24h/stage4-webadmin-bootstrap.env
editor /etc/dc24h/stage4-webadmin-bootstrap.env
```

Ustaw `WEBADMIN_PUBLIC_PORT` na jawnie wybrane `7178` lub `7278`, tak samo
jak podczas instalacji. Zachowaj `WEBADMIN_HOST=server-747585786.dc24h.eu`
i `WEBADMIN_HTTPS=true`. Ustaw `STAGE4_ENVIRONMENT=production`. Wpisz `dc24h_stage4_production`
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

Inicjalizacja sprawdza zgodność witryny Apache z wybranym Origin przed
zapisem stanu lub użyciem MariaDB. Zatwierdza nazwę bazy, stosuje migracje C++ i wersjonowane
migracje etapu IV, sprawdza schemat oraz uprawnienia i zapisuje weryfikator
hasła obliczony przez C++. Istniejące konto Administrator bez powiązania
Stage IV lub inny OWNER wymaga ręcznej audytowanej migracji. Ponowna
inicjalizacja tej samej bazy zachowuje konto i weryfikator. Po udanej
inicjalizacji wyczyść pola `ADMIN_INITIAL_PASSWORD` i
`MARIADB_ROOT_PASSWORD` w pliku. Usunięcie pliku albo edycja nie usuwa
wcześniejszej zawartości z dysku, migawek, kopii i historii edytora.
Inicjalizacja produkcyjna zapisuje zgodny `DC24H_STAGE4_ORIGIN` w
`/etc/dc24h/stage4-activation.env`. Starsze nadpisania jednostek muszą
pozostać zgodne z tą wartością przy aktualizacji i wycofaniu.
Usługi korzystają z osobnych plików runtime. Pliki testowe zapisują się
pod `/etc/dc24h/stage4-test`. Usługa produkcyjna odrzuca bazę testową.

## Uruchomienie, SMTP i HTTPS

```sh
origin="https://server-747585786.dc24h.eu:$public_port"
grep -Fx "DC24H_STAGE4_ORIGIN=$origin" /etc/dc24h/stage4-activation.env
systemctl start dc24h-stage4-webadmin.service
systemctl status dc24h-stage4-{auth,bridge,webadmin}.service --no-pager
journalctl -u dc24h-stage4-webadmin.service --no-pager -n 30
ss -ltn "( sport = :4278 or sport = :$public_port or sport = :1512 or sport = :1511 or sport = :3306 )"
```

Próba rzeczywistej wysyłki, z konta usługi, przed otwarciem portu:

```sh
printf 'From: admin@dc24h.eu\nTo: admin@dc24h.eu\nSubject: Stage IV SMTP probe\n\nDelivery check.\n' |
  runuser -u dc24h-webadmin -- /usr/bin/msmtp --file=/var/lib/dc24h-stage4/.msmtprc -t
```

Kod wyjścia zero potwierdza przyjęcie przez dostawcę SMTP. Sprawdź dostarczenie
wiadomości osobno w skrzynce `admin@dc24h.eu`.

Sprawdź DNS, certyfikat Let's Encrypt i inne aplikacje HTTPS na danej nazwie
hosta. Zweryfikuj ścieżki certyfikatu i klucza w zatwierdzonym vhoście.
Jeśli działa procedura odnawiania TLS, zachowaj ścieżki wspólnego zestawu.
Starsza witryna wymaga przeglądu i aktualizacji listy dozwolonych tras Apache
przed udostępnieniem funkcji odnawiania w WebAdminie. [Konfiguracja odnawiania TLS](TLS_AUTO_RENEWAL.md)
Przeglądarka wysyła ciasteczka dla tego hosta także na inne porty HTTPS.
Sprawdź, czy te aplikacje pomijają ciasteczka w logach i nie przekazują ich
do innych usług.

```sh
a2enmod ssl headers rewrite proxy proxy_http
a2ensite dc24h-stage4-webadmin
apache2ctl configtest && systemctl reload apache2
systemctl enable dc24h-stage4-webadmin.service
```

W zaporze dostawcy używaj wyłącznie zatwierdzonego portu. Z zewnątrz sprawdź
`curl --fail --silent --show-error "$origin/"`
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
systemctl restart dc24h-stage4-webadmin.service
systemctl status dc24h-stage4-{auth,bridge,webadmin}.service --no-pager
```

## Kopia, odzyskiwanie i wycofanie

W zaszyfrowanej kopii przechowuj bazę wraz z procedurami, prywatne pliki
runtime etapu IV i odpowiadające im pliki wersji, Apache i systemd. Zapisz
port, zgodny plik aktywacji, stan włączenia witryny oraz dodatki do jednostek.

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
systemctl disable --now dc24h-stage4-webadmin.service
systemctl stop dc24h-stage4-bridge.service dc24h-stage4-auth.service
systemctl is-active dc24h-stage3-adcs.service
```

Przy wycofaniu z kopii odtwórz poprzedni zgodny zestaw binariów Stage IV,
witryny Apache, pliku aktywacji i jednostek. Sprawdź porty `Listen` i
`VirtualHost`, reguły Host i Origin oraz zgodny Origin w Node. Uruchom
`apache2ctl configtest` przed przywróceniem zapisanego stanu włączenia.
Sprawdź HTTPS, logowanie hasłem i OTP, role, wylogowanie i ADCS. Odtwarzaj
bazę tylko w razie potrzeby na podstawie odrębnej decyzji uwzględniającej
nowsze rejestracje. Zachowaj działający hub ADCS i certyfikat Let's Encrypt.

## Odbiór

Workflow testowy uruchamia `scripts/stage4_test_install.sh` na jednorazowym
runnerze, żeby sprawdzić zachowanie witryny, ponowną instalację, awarię i
wycofanie. Test `scripts/stage4_test_database.sh 7178` z osobną bazą sprawdza inicjalizację konta, migracje,
C++ i MariaDB, transakcje, rejestrację, kody, symulowane awarie SMTP, restart
bazy oraz kopię i odtwarzanie w środowisku jednorazowym. CI obejmuje
GitHub-hosted Ubuntu 22.04, 24.04 i 26.04 dla dokładnego SHA badanego kodu.
Aktywacja produkcyjna wymaga odrębnej decyzji właściciela.
OWNER zgłosił 27 listopada 2026 udane logowanie Administratora kodem e-mail
i zmianę hasła. Oddzielnie udokumentuj SHA rzeczywiście wdrożonego kodu,
stan usług, doręczenie poczty oraz niezależne testy TLS, odmów dostępu,
uprawnień, sesji, odtworzenia i rollbacku. Po zmianach dokumentacji powstaje
nowy SHA wymagający osobnej weryfikacji CI. Zielone CI i potwierdzenie
operatora nie są `OWNER STAGE IV ACCEPTANCE` ani `OWNER MERGE GO`.
