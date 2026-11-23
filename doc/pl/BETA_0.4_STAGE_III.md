# Beta 0.4.9–0.4.12 — Etap III: sesje, HTTPS i monitoring

Realizacja [Issue #222](https://github.com/adrianf87/dc24h.eu-high-starter/issues/222) wyłącznie na jednej gałęzi `beta-0.4.9-0.4.12-stage3`: **cztery kolejne commity patchy i jeden PR do `main`**. Po każdym pushu **czekać na całkowite zakończenie wymaganych testów CI**. Bez automatycznego scalenia, otwarcia portu publicznego lub wdrożenia produkcyjnego.

| Patch | Zakres |
| --- | --- |
| **0.4.9 — Session Protection** | Rotacja identyfikatora sesji i CSRF po rejestracji klucza lub zmianie nazwy urządzenia; usuwanie starszych sesji i kontrola uprawnień po opóźnionych żądaniach. |
| **0.4.10 — Anti-Bruteforce** | Prywatne, trwałe limity prób, wspólne dla hasło+TOTP i hasło+passkey; restart nie resetuje blokady, a uszkodzony magazyn blokuje dostęp. |
| **0.4.11 — HTTPS Edge Security** | Dodatkowa kontrola SNI, Host i Origin w odizolowanym Apache2; testy certyfikatu, metod, ścieżek, limitów treści i nagłówków proxy. |
| **0.4.12 — Monitoring & Audit** | Okresowa weryfikacja ciągłości łańcucha audytu, alarmy o odmowach, awariach API, utracie logów i liczników. |

## Granice i wymagania systemowe

Serwer C++20 Direct Connect nadal obsługuje **ADC/ADCS** i jest jedynym autorytetem haseł, ról i grantów. Baza MariaDB pozostaje na schemacie **v11**, bez migracji. WebAdmin: React, TypeScript, Vite; brama Node pozostaje na `127.0.0.1:4182`, a przykładowe Apache2 HTTPS na `127.0.0.1:8443`. Bez publicznych tras zarządzania, zdalnego zapisu, ujawnienia tokenów usług i logowania bez hasła (Passkey wciąż zastępuje TOTP jako drugi składnik).

Systemy docelowe: Ubuntu Server 22.04, 24.04, 26.04; Debian 12 i 13. Maszyna operatora do osobnej walidacji: Debian 13 Trixie, Intel Xeon D-1521 (8 × 2,7 GHz), RAM 31 997 MiB. Zielone CI na Ubuntu nie zastępuje testów na tym serwerze.

## Nowy prywatny plik limitów

Do przykładowego systemd dodano:

```ini
Environment=DC24H_REMOTE_RATE_LIMIT_FILE=/var/lib/dc24h-remote/rate-limits.json
```

Katalog musi należeć do użytkownika bramy i mieć tryb `0700`; plik otrzymuje `0600`. Magazyn jest atomowo zapisywany z `fsync`, odrzuca symlinki, niebezpieczne uprawnienia, uszkodzone dane i równoległych pisarzy. Zawiera jedynie skróty HMAC-SHA256 kluczy limitów, liczniki oraz początki okien — **bez jawnych identyfikatorów kont, adresów IP, haseł, OTP, kluczy prywatnych i tokenów**.

Wspólne limity w oknie minuty: 20 wyzwań, 12 prób globalnych i cztery na konto; osobne limity obejmują dodawanie i zmianę urządzeń. Pełny magazyn odrzuca nowe próby, nie usuwa istniejących limitów. Klucz HMAC pochodzi z prywatnego tokenu C++: **rotacja tokenu zmienia przestrzeń skrótów i nie zachowuje wcześniejszych limitów na konto** — wymaga osobnego przeglądu operatora. Nie kasuj magazynu dla obejścia blokady. Awaria pliku powoduje odmowę dostępu i unieważnienie sesji.

Rejestracja Passkey i zmiana nazwy urządzenia zwracają nowe `csrf` i ciasteczko Secure/HttpOnly/SameSite=Strict. Poprzednie ciasteczko jest nieważne. Po dodaniu klucza pozostałe sesje konta są unieważnione. Cofnięcie klucza kończy wszystkie sesje konta. Uprawnienia C++ są ponownie sprawdzane także po opóźnionym odczycie treści POST.

## Testy HTTPS i certyfikatu

Przykład `config/apache2-webadmin-remote.conf.example` **nie jest instalowany ani uruchamiany automatycznie**. W odizolowanym teście Apache2 kontroluje SNI i Host `localhost`, dokładny Origin każdej operacji Passkey, nagłówki Fetch Metadata, metody, ścieżki, wielkości i sposób przesyłania POST; blokuje niebezpieczne zasoby i nagłówki przekazujące tożsamość. Testy wymagają zaufanego certyfikatu o poprawnym SAN.

Ręczne sprawdzenie na zatwierdzonym serwerze prywatnym (certyfikat CA operatora):

```sh
openssl s_client -connect 127.0.0.1:8443 -servername localhost -verify_return_error -CAfile /path/to/trusted-ca.pem
curl --fail --cacert /path/to/trusted-ca.pem https://localhost:8443/remote.html
```

Nie używaj `curl -k` jako dowodu poprawnej weryfikacji TLS. Nie zmieniaj bindowania na `0.0.0.0`, nie usuwaj `Require local` i nie wystawiaj Vite ani bramy w publicznym Internecie.

## Audyt, alarmy, retencja i reakcja

Prywatny plik JSONL o maksymalnym rozmiarze 8 MiB ma kolejne numery i łańcuch SHA-256. Co najwyżej co 15 sekund przy kolejnych żądaniach brama ponownie sprawdza cały zapis, w tym podmiany w miejscu bez zmiany rozmiaru. Wykrycie naruszenia, zapełnienia, uszkodzenia lub utraty zapisu **blokuje dostęp**. Łańcuch bez podpisu nie chroni przed uprzywilejowanym napastnikiem, który może przepisać całą historię.

Do dziennika systemd trafiają wyłącznie stałe kody zdarzeń i powody alarmów z czasem, bez danych konta, IP, cookies, tokenów czy haseł. W ciągu minuty najwyżej jeden alarm danego typu: dziesięć odmów uwierzytelnienia, trzy awarie API, utrata magazynu limitów lub audytu. Dziennik systemd nie zastępuje trwałego audytu.

Procedura operatora:

1. Po alarmie utraty audytu lub limitów zachowaj dziennik i **zatrzymaj lokalną bramę**. Nie resetuj plików, aby dopuścić nowe logowania.
2. Zweryfikuj uprawnienia, sekwencję i końcowy skrót audytu, odizoluj zmienione pliki i wyjaśnij przyczynę.
3. Zachowuj zaszyfrowane kopie audytu, katalogu Passkeys, TOTP/replay i limitów zgodnie z zatwierdzoną retencją. Przed osiągnięciem 8 MiB zatrzymaj usługę, zachowaj cały zweryfikowany łańcuch i dopiero utwórz nowy plik.
4. Eksport poza serwer tylko po osobnej akceptacji: uwierzytelniony, szyfrowany kanał, ograniczony dostęp, retencja i usuwanie po obu stronach. Etap III **nie uruchamia automatycznego eksportera**.
5. Wznów usługę po kontroli przyczyny, sesji, grantów i czynników MFA. Publiczny WebAdmin wymaga osobnej decyzji.

## Testy i wydanie

W `webadmin` wykonaj `npm ci`, `npm run typecheck`, `npm test`, `npm run build` i `npm run test:e2e`. Osobno wymagane C++20 Debug/Release, CTest, sanitizery, izolowane Apache2 i kontrola Debian 12/13. Dodano testy trwałości limitów po restarcie, uszkodzeń audytu, alarmów i rotacji sesji.

**Czekaj na pełny wynik każdej wymaganej pracy GitHub CI dla najnowszego SHA**. Przy błędzie napraw na tej samej gałęzi, wypchnij i zaczekaj ponownie. Jeden PR do `main`, bez automatycznego scalania przy brakujących akceptacjach i bez wdrożenia produkcyjnego.
