# Etap IV — osobna usługa WebAdmin (procedura wdrożenia)

**Stan: kod i testy GitHub-hosted, bez instalacji na Debianie 13.** Hub ADCS
na TCP `1512` pozostaje bez zmian; publiczny panel HTTPS `7278` nadal jest
wyłączony do osobnej decyzji `OWNER WEBADMIN PUBLIC ACTIVATION GO`.

| Element | Konto | Dostęp |
| --- | --- | --- |
| Hub ADCS | `dc24h-stage3` | TCP `1512`, prywatne API 0700/0600, ten sam UID |
| Most C++ | UID `dc24h-stage3`, GID `dc24h-webadmin` | Gniazdo Unix; sprawdzenie `SO_PEERCRED` dokładnie dla UID WebAdmin |
| Gateway Node 24 | `dc24h-webadmin` | Tylko `127.0.0.1:4278`; bez tokenu i gniazda huba |
| Apache | konto Apache | Publiczne HTTPS `:7278` po odbiorze |

Most działa z uprawnieniami UID huba, dlatego **jego kod wymaga osobnego
przeglądu bezpieczeństwa**. Może przekazywać wyłącznie trzy metody: logowanie
hasłem `POST /api/v1/admin/access`, odczyt ról
`GET /api/v1/management` i status `GET /api/v1/status`. Nie zmienia
istniejącej reguły tożsamości `SO_PEERCRED` serwera C++ i nie ujawnia tokenu
Node. Nie jest to dowolne proxy.

## Kolejność czynności na konsoli VNC usługodawcy

1. Potwierdź `dc24h-stage3-adcs.service` i konto `dc24h-stage3`. Zweryfikuj,
   że prywatne C++ API jest włączone osobną zgodą OWNER. Oczekiwane pliki
   `/run/dc24h/api.sock`, `/run/dc24h/api.token` muszą istnieć w katalogu
   0700 należącym do UID huba. **Nie zmieniaj ich chmod ani właściciela.**
2. Zatwierdź i zainstaluj przykład
   `config/dc24h-stage4-webadmin.sysusers.example` poprzez `systemd-sysusers`.
   Nowy `dc24h-webadmin` nie może być rootem ani UID huba.
3. Przygotuj zweryfikowaną, root-owned instalację Node **24 LTS** w
   `/opt/dc24h/node24/bin/node` (ze sprawdzeniem SHA-256 i pochodzenia).
   `/usr/bin/node` v20 i `/root/.nvm` nie są środowiskiem tej usługi.
4. Zainstaluj dokładnie przetestowany kod do `/opt/dc24h` (bez prawa zapisu
   WebAdmin do skryptu mostu, panelu ani plików `.mjs`).
5. Utwórz prywatną konfigurację SMTP
   `/var/lib/dc24h-stage4/.msmtprc`, właściciel `dc24h-webadmin`, tryb 0600;
   katalog nadrzędny 0700 tego samego UID. Ustaw konto `default`,
   `admin@dc24h.eu`, SMTP home.pl, auth i TLS z kontrolą certyfikatu.
   **Nie kopiuj `/root/.msmtprc` ani haseł do repozytorium.**
6. Dopiero po odbiorze przygotuj `/etc/dc24h/stage4-activation.env` (root,
   0600) z `DC24H_STAGE4_STARTUP_ACK=OWNER_WEBADMIN_PUBLIC_ACTIVATION_GO`
   oraz stabilnym kluczem `DC24H_STAGE4_LIMITER_KEY` (64 małe znaki hex,
   `openssl rand -hex 32`), innym niż token API huba.
7. Zweryfikuj, a następnie instaluj przykładowe usługi systemd: najpierw
   `dc24h-twilight-stage4-bridge.service`, potem
   `dc24h-twilight-stage4-webadmin.service`. Nie włączaj ich przed
   końcową zgodą. Obie działają bez `sudo`.
8. Przy wyłączonym gateway wykonaj narzędzie enrollment jako
   `dc24h-webadmin`, wybierając `Administrator` i `admin@dc24h.eu`.
   Hasło konta C++ i odebrany kod wpisz **tylko** w interaktywnej konsoli VNC.
   Katalogi audytu i limitów twórz jako użytkownik WebAdmin.
9. Sprawdź realne dostarczenie e-maila, błędne kody, role, unieważnianie sesji,
   CSRF, restart i rollback dwóch usług. Sprawdź z Internetu porty 1512,
   brak 1511, brak publicznego 4278 i publiczny HTTPS 7278.
10. Włącz istniejący przykład Apache
    `config/apache2-webadmin-public.conf.example` **dopiero po odbiorze**.
    Certyfikat Let's Encrypt dla `server-747585786.dc24h.eu` pozostaje w
    istniejącej lokalizacji. Jego ręczne odnawianie pozostaje decyzją OWNER.

### Odczyt stanu bez zmian na VPS

```sh
python3 scripts/stage4_vps_preflight.py
systemctl show dc24h-stage3-adcs.service -p User -p Group -p ActiveState
getent passwd dc24h-stage3 dc24h-webadmin
ss -lnt '( sport = :1512 or sport = :1511 or sport = :4278 or sport = :7278 )'
```

Rzeczywisty test SMTP musi być uruchomiony jako `dc24h-webadmin` z dokładną
konfiguracją `--file=/var/lib/dc24h-stage4/.msmtprc`. Wynik 0 z `msmtp`
oraz otrzymana wiadomość to dwa oddzielne wymagania. Test z konta root nie
kwalifikuje SMTP usługi.

### Wycofanie

Zanim włączysz panel, zrób kopie plików usług, kodu, konfiguracji Apache i
prywatnych danych WebAdmin. Gdy test nie przejdzie: wyłącz tylko vhost
Apache `:7278`, zatrzymaj WebAdmin i most, przywróć pliki/katalogi WebAdmin
i ponów preflight. Nie zatrzymuj huba, nie zmieniaj tokenu C++ i nie dotykaj
klucza Let's Encrypt. Testy izolowane Ubuntu nie zastępują prób Debian 13.

Do końca pozostały ręczne testy: realne SMTP, prawdziwe konto Administrator,
role, Internet HTTPS, systemd restart/restore i rollback. Same zielone CI
nie stanowią zgody na publiczną aktywację.
