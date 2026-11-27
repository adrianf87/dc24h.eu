# Etap IV — weryfikacja adresu e-mail i rejestracja kont

**Stan na 27 listopada 2026: IMPLEMENTACJA GOTOWA / OWNER ZGŁOSIŁ UDANE LOGOWANIE / ODBIÓR KOŃCOWY OTWARTY.**

Według [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10)
OWNER pomyślnie zalogował się na publiczny WebAdmin
`https://server-747585786.dc24h.eu:7278` jako `Administrator` hasłem
i ośmiocyfrowym kodem doręczonym na `admin@dc24h.eu`.
Potwierdził także dashboard, stan usługi kont i zmianę hasła.
To dowód operatora, **nie** niezależna kwalifikacja bezpieczeństwa,
akceptacja `OWNER STAGE IV ACCEPTANCE` ani zgoda na scalenie.
Implementacja pozostaje w [Draft PR #27](https://github.com/adrianf87/dc24h.eu-twilight/pull/27).

## Aktualny model kont i kodów

- Chroniony `dc24h-stage4-init` czyta plik
  `/etc/dc24h/stage4-webadmin-bootstrap.env` należący do root (0600),
  przygotowuje konto `Administrator`, natywny weryfikator hasła C++
  oraz tabele Stage IV MariaDB. Produkcyjna i testowa baza są odrębne.
- Pierwsze logowanie Administratora wymaga hasła i kodu na jego skonfigurowany
  adres e-mail; poprawny kod weryfikuje skrzynkę i aktywuje konto.
  Każde następne logowanie wymaga ponownego podania hasła i nowego kodu.
- Publiczne **Załóż konto** pozwala utworzyć tylko użytkownika `registered`:
  użytkownik wprowadza nazwę, hasło i e-mail, potwierdza skrzynkę kodem,
  a następnie loguje się osobno. Nie może sam nadać sobie praw admin/OWNER.
- Formularz logowania **nie wybiera odbiorcy**. Zweryfikowany e-mail pochodzi
  z prywatnego rekordu konta; pole `EM` publicznego profilu ADC nie stanowi
  dowodu tożsamości. Uprawnienia, status i wersja poświadczeń są ponownie
  sprawdzane w backendzie C++.
- Kod ma osiem cyfr, nie jest przechowywany jawnie (HMAC), wygasa po pięciu
  minutach, jest jednorazowy i dopuszcza maksymalnie pięć błędnych prób.
  Odmowy, błędna dostawa, utrata roli, unieważnienie i wygaśnięcie sesji
  muszą działać fail-closed. Samo hasło albo sam kod nie wystarczają.
- Publiczne logowanie Stage IV **nie korzysta z passkey**. Zmiany e-mail,
  odzyskiwanie kont oraz nadawanie ról uprzywilejowanych wymagają osobnej,
  audytowanej procedury OWNER poza formularzem rejestracyjnym.

## SMTP i podział usług

- Apache udostępnia HTTPS na 7278; Node działa wyłącznie pod
  `127.0.0.1:4278` jako `dc24h-webadmin` i łączy się z prywatnym C++
  przez ograniczony Unix bridge z kontrolą UID. ADCS pozostaje na 1512.
- Nadawcą jest `admin@dc24h.eu`, transport zapewnia `/usr/bin/msmtp`
  z poprawnym TLS. Sekrety należą do konta usługi w prywatnym pliku
  `/var/lib/dc24h-stage4/.msmtprc` (0600), nigdy w repo, logach lub CI.
  OWNER potwierdził połączenie home.pl na porcie 465 z TLS implicit
  (`tls_starttls off`); dla portu 587 typowo używa się STARTTLS.
- Ciasteczka `Secure`, `HttpOnly`, `SameSite=Strict` nie są izolowane
  numerem portu. Konieczny pozostaje niezależny test ich przekazywania
  między usługami tego samego hosta, CSRF, nagłówków, sesji i tras.

## Obecna procedura i ograniczenia

Do instalacji, inicjalizacji, diagnostyki, kopii i rollbacku stosuj
[instrukcję wydzielonej usługi](STAGE_IV_DEDICATED_SERVICE.md).
**Nie używaj starej procedury**
`public-email-enroll-main.mjs --catalog verified-emails.json`:
aktualny Stage IV przechowuje weryfikację konta w MariaDB, a pierwsze
potwierdzenie Administratora następuje przez formularz WebAdmin.

Przed kolejną zmianą VPS sprawdź SHA, zainstalowane pliki i oddzielną
zgodę OWNER. Udane logowanie nie oznacza zgody na restart, firewall,
migrację prywatnych danych, wydanie wersji ani scalenie PR.

## Nadal wymagane do odbioru

1. Niezależne zewnętrzne testy HTTPS/TLS/SAN/Host/Origin i odrzuceń
   niedozwolonych metod, nagłówków i tras; brak publicznego dostępu
   do MariaDB, wewnętrznego API, portu Node 4278 i panelu developerskiego.
2. Testy negatywne kodów, ról, sesji, CSRF, limitów oraz cookies na
   innych portach HTTPS tej samej domeny.
3. Weryfikacja nowego użytkownika przez rzeczywistą skrzynkę, kontrola
   cofnięcia dostępu, utraty poczty i odzyskania konta oraz audytu.
4. Dowody bezpiecznego odtworzenia danych, procedury restartu i rollbacku
   bez celowego restartowania działającego huba Stage III/VPS na potrzeby testu.
5. Formalny zapis decyzji o publicznej aktywacji,
   `OWNER STAGE IV ACCEPTANCE` i osobne `OWNER MERGE GO`.

Ostatni zatwierdzony kod przechodził CI na GitHub-hosted Ubuntu
22.04, 24.04 i 26.04. Zmiany dokumentacji tworzą nowszy SHA,
dlatego wynik trzeba odnosić do konkretnego commita.
