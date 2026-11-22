# beta-0.3.10 — opcjonalny pilotaż prywatnego odczytu zdalnego

**Wyłącznie za decyzją operatora i do testów, bez publicznego WebAdmin.**
Etap beta-0.3.8–beta-0.3.10 uszczelnia obecny **lokalny HTTPS**, izolację
procesu Node w systemd i udostępnia procedurę **lokalnego tunelu SSH**.
Nie powstają nowe publiczne porty, role, trasy, uprawnienia do zapisu ani
automatyczna instalacja lub konfiguracja SSH.

## Granica zaufania

Przeglądarka operatora (`https://localhost:8443`) → lokalny port tunelu SSH
(`127.0.0.1:8443` na komputerze operatora) → serwer SSH → Apache HTTPS
(`127.0.0.1:8443` na serwerze) → Node (`127.0.0.1:4182`) → prywatne API
C++ przez gniazdo Unix. **Oprócz weryfikacji klucza hosta SSH przeglądarka
musi potwierdzić certyfikat Apache** wystawiony dla nazwy `localhost`.

To krótki, nadzorowany pilotaż tylko dla aktywnego administratora/Ownera
z hasłem i zarejestrowanym TOTP. TOTP nie jest odporne na phishing.
Kradzież klucza SSH, przejęcie komputera, przeglądarki lub serwera nadal
może doprowadzić do naruszenia bezpieczeństwa.

## Uruchomienie na wyraźną decyzję operatora

1. Sprawdź `config/apache2-webadmin-remote.conf.example`: TLS 1.2/1.3,
   mocne szyfry AEAD z forward secrecy dla TLS 1.2, wyłączone kompresja
   TLS i bilety sesji oraz nagłówki ochrony prywatności. Apache musi nadal
   słuchać **wyłącznie** na `127.0.0.1:8443`. Przygotuj ważny certyfikat z
   nazwą DNS `localhost`; na stację przenoś wyłącznie publiczny certyfikat
   lub CA, nigdy klucz prywatny. Zweryfikuj zaufanie i termin ważności.
2. Sprawdź `config/dc24h-remote-gateway.service.example`: prywatny katalog
   MFA, system tylko do odczytu, dostęp sieciowy tylko do loopback, izolację
   urządzeń, kernela, procesów i przestrzeni nazw. Nie włączaj
   `PrivateNetwork=true` (uniemożliwiłoby dostęp Apache do Node), ani
   `MemoryDenyWriteExecute=true` bez testu V8 JIT. Testuj usługę i dostęp
   do istniejącego gniazda/tokenu API na rzeczywistym serwerze.
3. Osobno zabezpiecz SSH: logowanie kluczem, znany klucz hosta, dedykowane
   konto/klucz wyłącznie dla pilotażu, ograniczenie `PermitOpen
   127.0.0.1:8443`, firewall i limity prób. Opcje klucza w
   `authorized_keys` mogą mieć formę
   `restrict,port-forwarding,permitopen="127.0.0.1:8443"`.
   Zweryfikuj także shell, inne uprawnienia konta i ustawienia sshd.
4. Na komputerze operatora wykonaj (zastąp nazwę hosta i użytkownika
   prawidłowymi wartościami, po sprawdzeniu tożsamości serwera):

   ```sh
   ssh -N -T -o ExitOnForwardFailure=yes -o StrictHostKeyChecking=yes \
     -o IdentitiesOnly=yes \
     -L 127.0.0.1:8443:127.0.0.1:8443 pilot-user@trusted-ssh-host
   ```

   Tunel nasłuchuje tylko na loopback **komputera operatora**. Nie stosuj
   `0.0.0.0`, wildcard ani publicznego przekierowania portu 8443.
   SSH nie omija kontroli HTTPS, Origin/Host, CSRF, MFA ani uprawnień Node.
5. Po bezpiecznym skopiowaniu publicznego certyfikatu/CA na stację
   roboczą uruchom z katalogu repozytorium:

   ```sh
   python3 scripts/remote_pilot_check.py --ca-file /secure/path/remote-ca.pem
   ```

   Skrypt zawsze łączy się z `localhost:8443` przez HTTPS z kontrolą
   certyfikatu. Wysyła pięć **nieuwierzytelnionych GET** i sprawdza, że
   portal działa, dane bez sesji oraz panel administracji pozostają
   zablokowane, a odpowiedzi mają no-store i nosniff. Nie wysyła hasła,
   TOTP, cookie ani tokenu; odmowa następuje przy przekierowaniu, błędzie
   certyfikatu lub niespodziewanym sukcesie. Nie zastępuje audytu serwera.
6. Otwórz na tej samej stacji `https://localhost:8443/remote.html`.
   Zaufaj certyfikatowi poprawną metodą systemową — **nie** ignoruj
   ostrzeżeń TLS. Zaloguj się hasłem i TOTP, potwierdź wyłącznie zbiorcze
   statystyki i wyloguj się. Sprawdź rejestr zdarzeń systemd-journald.

## Wycofanie pilotażu i ograniczenia

Zatrzymaj proces SSH (`Ctrl+C`), aby zakończyć dostęp. Wyloguj użytkownika;
opcjonalnie restart Node usunie sesje w pamięci. Nie cofaj ani nie
resetuj dziennika TOTP przy niezmienionym sekrecie. Nie ma publikacji
Internetowej, proxy publicznego, zapisu administracyjnego, trwałego
wdrożenia ani odzyskiwania MFA. Przed przyszłym dostępem internetowym
potrzebne będą WebAuthn/passkeys odporne na phishing, rejestracja i
odzyskiwanie, ochrona krawędzi, kontrola nadużyć i trwały audyt.
