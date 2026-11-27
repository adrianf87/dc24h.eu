# Etap IV — zatwierdzony projekt publicznego WebAdmin (P0)

**Status: KANDYDAT IMPLEMENTACJI / BRAK PEŁNEGO ODBIORU / BRAK PUBLICZNEJ AKTYWACJI WEBADMIN.**

OWNER wybrał publiczny panel HTTPS na istniejącej domenie VPS oraz osobnym porcie.
Zgoda OWNER obejmuje przygotowanie kodu na roboczej gałęzi, ale nie uruchomienie
panelu na VPS. Dokument nie upoważnia do otwarcia portu, instalacji na serwerze,
zmiany DNS, certyfikatu ani konfiguracji publicznego serwera.
Wykaz blokad odbioru: [raport Stage IV](../us/PUBLIC_STAGE_IV_WEBADMIN_IMPLEMENTATION.md).
Pełne wymagania techniczne i macierz testów są w [wersji US](../us/PUBLIC_STAGE_IV_WEBADMIN_DESIGN.md).

| Cel | Wartość |
| --- | --- |
| Publiczny hub ADCS dla gości | `adcs://server-747585786.dc24h.eu:1512` |
| Przyszły panel WebAdmin | `https://server-747585786.dc24h.eu:7278` |
| IPv4 VPS Debian 13 | `62.171.156.32` |
| WebAuthn RP ID | `server-747585786.dc24h.eu` |
| Dokładny Origin przeglądarki | `https://server-747585786.dc24h.eu:7278` |
| Uwierzytelnianie | odrębne konta OWNER/admin, passkeys i MFA |
| Aktywacja | osobna decyzja `OWNER WEBADMIN PUBLIC ACTIVATION GO` |

WebAuthn RP ID to sama nazwa hosta, **bez portu**. Origin zawiera schemat HTTPS,
nazwę hosta **i port 7278**. Klucze z lokalnego pilota `localhost` wymagają
odrębnej, jawnie zaakceptowanej rejestracji lub migracji.

Istniejący `/remote.html` oraz gateway na `127.0.0.1:4182` pozostają
wyłącznie lokalne. Nie wolno ich publicznie wystawiać przez prostą zmianę
Apache. Nowy gateway musi mieć osobny punkt wejścia, ścisłą listę dozwolonych
tras i metod, weryfikację uprawnień w C++, CSRF, bezpieczne sesje,
ograniczenia prób logowania, unieważnianie urządzeń i audyt.

**Ważne ryzyko:** ciasteczka przeglądarki nie są izolowane według numeru portu.
Ten sam host może otrzymywać cookies zarówno na 7278, jak i na innych portach
HTTPS. Przed aktywacją trzeba sprawdzić wszystkie usługi HTTP tej domeny,
oddzielić nazwy sesji, blokować przekazywanie cookies i wykazać w testach,
że nie dochodzi do wycieku lub kolizji. Sam osobny port nie daje izolacji sesji.

Publiczne konto OWNER i konta administratorów muszą być rozdzielone.
**Decyzja OWNER: hasło ORAZ passkey są obowiązkowe przy każdym publicznym
logowaniu uprzywilejowanym.** Najpierw backend potwierdza hasło konta, a potem
WebAuthn weryfikuje passkey tego samego użytkownika, dokładny Origin/RP ID,
obecność i weryfikację użytkownika, podpis, replay i odwołanie uprawnień.
Dopiero po obu czynnikach i ponownym sprawdzeniu roli można wydać sesję.
Wymagane są bezpieczne procedury odzyskiwania, unieważniania sesji oraz testy
negatywne. Logowanie oparte wyłącznie na haśle, passkey albo haśle/TOTP nie
jest zatwierdzone tą decyzją.

Certyfikat Let's Encrypt musi zawierać prawidłowy SAN dla nazwy hosta.
Klucze prywatne, tokeny usługowe i sekrety nie mogą trafiać do repozytorium,
artefaktów CI ani przeglądarki. Port 1512 pozostaje dla ADCS; publiczny ADC
1511, MariaDB, wewnętrzne API i porty developerskie pozostają niedostępne.

Testy CI używają wyłącznie GitHub-hosted Ubuntu 22.04, 24.04 i 26.04.
Nie przywracamy self-hosted runnera i nie wykonujemy zdalnego `sudo` przez CI.
Uruchomienie publicznego panelu wymaga oddzielnego odbioru Etapu IV oraz zgody
`OWNER WEBADMIN PUBLIC ACTIVATION GO`.
