# Etap IV — weryfikacja e-mail Administratora i odbiór publicznego WebAdmin

**Kod przygotowywany do PR. Nie uruchomiono panelu ani nie wysłano prawdziwego e-maila.**
OWNER wskazał `admin@dc24h.eu` jako adres konta **Administrator**. Samo
wskazanie nie oznacza jeszcze zweryfikowania skrzynki.

## Wymagane warunki

1. Wskazać dokładną nazwę konta C++ `Administrator`. Nowy mechanizm sam
   sprawdza nazwę z hasłem w prywatnym API C++ i ustala jego **numeryczny ID**.
   Nie trzeba podawać ID w czacie ani zgadywać go z nicku/pola `EM` w ADC.
   Konto musi być aktywne i mieć rolę administratora lub OWNER.
2. Prawdziwy odbiorca musi odebrać wiadomość na `admin@dc24h.eu` i przepisać
   kod podczas wykonywania narzędzia w konsoli usługodawcy. Narzędzie sprawdza
   hasło konta C++, rolę, kod 8-cyfrowy, termin 5 minut i rolę ponownie.
3. Dopiero po poprawnym teście zostaje utworzony prywatny plik 0600
   `verified-emails.json`. Narzędzie nie nadpisuje istniejących powiązań,
   nie tworzy kont C++, nie nadaje uprawnień i nie otwiera portów.
4. `msmtp` musi działać dla **oddzielnego użytkownika `dc24h-webadmin`**, nie tylko root. Serwer
   SMTP musi używać uwierzytelnienia i poprawnego TLS. Hasła pozostają poza
   repozytorium, logami i CI; konfiguracja `/root/.msmtprc` nie jest
   automatycznie dostępna dla nieuprzywilejowanego systemd. Konfigurację
   `/var/lib/dc24h-stage4/.msmtprc` (0600) odczytuje wyłącznie WebAdmin.
5. Logowanie przez C++ w działającym hubie jest **domyślnie wyłączone**.
   Wymaga osobnej zgody OWNER, zmiennej
   `DC24H_STAGE4_PRIVATE_AUTH_ACK=OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO`
   i kontrolowanego restartu huba. Do czasu odbioru nie dodawać tej zmiennej.
   Gniazdo i token C++ nadal należą do UID huba; WebAdmin łączy się wyłącznie
   przez ograniczony most Unix sprawdzający UID procesu (`SO_PEERCRED`).

## Przygotowanie i uruchomienie narzędzia

Po przeprowadzeniu testów na izolowanej maszynie zbudować z dokładnego commita:

Odczyt stanu VPS bez modyfikowania usług można wykonać przez własną konsolę
VNC usługodawcy, po pobraniu i sprawdzeniu przetestowanego commita:

```sh
python3 scripts/stage4_vps_preflight.py
```

Raport JSON nie ujawnia haseł ani kluczy; pola `NOT_TESTED` nadal wymagają
ręcznego potwierdzenia. Nie jest potrzebny self-hosted runner ani przekazanie
mi hasła SSH.

```sh
cd webadmin
npm ci --no-audit --no-fund
npm run typecheck
npm run build:public:ci
npm run build:enrollment:ci
```

Następnie w konsoli dostawcy, przy zatrzymanym publicznym gateway, jako
użytkownik usługi i z prywatnym dostępem do C++ API:

```text
DC24H_STAGE4_BRIDGE_SOCKET=/run/dc24h-stage4-bridge/bridge.sock \
/opt/dc24h/node24/bin/node /opt/dc24h/webadmin/dist/public-email-enroll-main.mjs \
  --account-name Administrator \
  --email admin@dc24h.eu \
  --catalog /var/lib/dc24h-stage4/verified-emails.json \
  --owner-ack STAGE4_EMAIL_ENROLLMENT_GO
```

Hasło C++ i kod e-mail wpisuje się w konsoli bez wyświetlania. Po sukcesie
narzędzie pokaże prawdziwy numeryczny ID konta. Błędny kod,
brak dostawy, wygaszenie, cofnięta rola, niewłaściwe uprawnienia pliku lub
istniejące przypisanie kończą operację odmową, bez utworzenia nowego wpisu.

Zakładanie nowych kont administratorów odbywa się wyłącznie poprzez istniejące
zaufane narzędzia C++ OWNER, a nie przez publiczny formularz rejestracyjny.
Zmiana adresu i odzyskiwanie dostępu wymagają osobnej procedury offline,
kopii zapasowej, kontroli tożsamości i unieważnienia sesji.

## Co nadal jest do zrobienia?

- Test faktycznego wysłania i odebrania wiadomości przez `msmtp` na Debianie 13.
- Test prawdziwego konta Administrator i kompletnego backendu C++.
- Kontrola Apache HTTPS `:7278`, portów i certyfikatu Let's Encrypt.
- Testy dwóch usług, sesji, ról, restartu systemd, odzyskania i rollbacku.
- Niezależna kontrola publicznego dostępu, bezpieczeństwa ciasteczek i zgoda
  `OWNER WEBADMIN PUBLIC ACTIVATION GO` **po** zakończeniu kwalifikacji.

Publiczny hub `adcs://server-747585786.dc24h.eu:1512` pozostaje bez zmian.
Panel `https://server-747585786.dc24h.eu:7278` nadal jest wyłączony.

Szczegółowy plan w [wersji EN-US](../us/STAGE_IV_EMAIL_ENROLLMENT.md).
Nowa procedura wydzielonej usługi, SMTP, systemd i rollbacku:
[Etap IV — osobny WebAdmin](STAGE_IV_DEDICATED_SERVICE.md).
