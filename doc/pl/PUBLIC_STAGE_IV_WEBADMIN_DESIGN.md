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
| Dokładny Origin przeglądarki | `https://server-747585786.dc24h.eu:7278` |
| Uwierzytelnianie | odrębne konta OWNER/admin: hasło + kod e-mail |
| Nadawca kodu | `admin@dc24h.eu` przez `msmtp -t` |
| Odbiorca | zweryfikowany adres przypisany do danego konta |
| Aktywacja | osobna decyzja `OWNER WEBADMIN PUBLIC ACTIVATION GO` |

Origin zawiera schemat HTTPS, nazwę hosta **i port 7278**. OWNER zastąpił
wcześniejszą decyzję o passkey: publiczne logowanie ma wymagać hasła konta
oraz 8-cyfrowego kodu e-mail. Dotychczasowy prywatny pilot WebAuthn
na `localhost` pozostaje bez zmian.

Istniejący `/remote.html` oraz gateway na `127.0.0.1:4182` pozostają
wyłącznie lokalne. Nie wolno ich publicznie wystawiać przez prostą zmianę
Apache. Nowy gateway musi mieć osobny punkt wejścia, ścisłą listę dozwolonych
tras i metod, weryfikację uprawnień w C++, CSRF, bezpieczne sesje,
ograniczenia prób logowania, unieważnianie sesji i audyt.

**Ważne ryzyko:** ciasteczka przeglądarki nie są izolowane według numeru portu.
Ten sam host może otrzymywać cookies zarówno na 7278, jak i na innych portach
HTTPS. Przed aktywacją trzeba sprawdzić wszystkie usługi HTTP tej domeny,
oddzielić nazwy sesji, blokować przekazywanie cookies i wykazać w testach,
że nie dochodzi do wycieku lub kolizji. Sam osobny port nie daje izolacji sesji.

Publiczne konto OWNER i konta administratorów muszą być rozdzielone.
**Aktualna decyzja OWNER: hasło ORAZ jednorazowy kod e-mail są obowiązkowe.**
Po poprawnym haśle potwierdzonym w C++ gateway odczytuje zweryfikowany adres
e-mail **z prywatnego rejestru kont**, a nie z pola podanego w formularzu lub
publicznego profilu ADC. Wysyła ośmiocyfrowy kod przez `msmtp -t` z adresu
`admin@dc24h.eu`. Kod jest ważny maksymalnie 5 minut, ma limit 5 błędnych
prób i nie jest przechowywany jawnym tekstem. Weryfikacja kodu oraz ponowna
kontrola roli w C++ są wymagane przed utworzeniem sesji.
Zmiana adresu lub odzyskiwanie konta wymaga zatwierdzonej procedury OWNER
poza publicznym formularzem. Hasło bez kodu nie wystarcza.

**Ryzyko:** kod e-mail jest słabszy od passkey i nie jest odporny na phishing.
Skrzynka pocztowa musi mieć silne, niezależne zabezpieczenia i MFA.

Certyfikat Let's Encrypt musi zawierać prawidłowy SAN dla nazwy hosta.
Klucze prywatne, tokeny usługowe i sekrety nie mogą trafiać do repozytorium,
artefaktów CI ani przeglądarki. Port 1512 pozostaje dla ADCS; publiczny ADC
1511, MariaDB, wewnętrzne API i porty developerskie pozostają niedostępne.

Testy CI używają wyłącznie GitHub-hosted Ubuntu 22.04, 24.04 i 26.04.
Nie przywracamy self-hosted runnera i nie wykonujemy zdalnego `sudo` przez CI.
Uruchomienie publicznego panelu wymaga oddzielnego odbioru Etapu IV oraz zgody
`OWNER WEBADMIN PUBLIC ACTIVATION GO`.
