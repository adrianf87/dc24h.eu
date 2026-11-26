# Etap I — wyniki Debian 13 + ncdc

**STAGE I: BLOCKED**  
**PUBLIC DEPLOYMENT: NO-GO**

Lokalny przebieg z 26 listopada 2026 r., 01:08–01:21 UTC, dotyczył commitu `1bbcd541e86c133b884f8e29933c4b4f36363302` z gałęzi `public/stage-i-debian13-ncdc-adcs`. Środowisko to Debian 13.8, jądro `6.12.117+deb13-amd64`, GCC 14.2.0, CMake 3.31.6, OpenSSL 3.5.9 oraz ncdc 1.23.1 z GnuTLS 3.8.9. Sesje używały wyłącznie pętli zwrotnej w izolowanych przestrzeniach nazw. [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) · [PR #12](https://github.com/adrianf87/dc24h.eu-twilight/pull/12)

| Obszar | Wynik | Dowód |
| --- | --- | --- |
| Release | **PASS** | C++20, `beta-0.9.20`, kompilacja i `--check-config` z prawidłowym certyfikatem zwróciły 0. |
| IPv4-only | **PASS** | `adc.ctm.process`, `adc.rcm.process` i `stage1.ipv6_loopback_policy` przeszły. Dwa wpisy CTM/RCM jawnie zgłosiły pominięcie w sumie czterech wariantów IPv6. |
| IPv6 strict | **PASS** | `adc.ctm.process` i `adc.rcm.process` z `DC24H_REQUIRE_IPV6_LOOPBACK=1` wykonały osiem wariantów ADC/ADCS po IPv4 i IPv6 bez pominięć. |
| ADCS CA + SAN | **PASS** | `adcs.process` przeszedł 13 prób. Klient referencyjny przyjął zaufany certyfikat dla `localhost`, odrzucił niezgodny IP SAN (64) i obce CA (20). |
| ADCS KEYP w ncdc | **PASS** | Niezależnie potwierdzony odcisk dopuścił logowanie, również przy niezgodnym SAN. Błędny lub nieaktualny odcisk spowodował odmowę. Zaobserwowano wyłącznie TLS na porcie ADCS. |
| Dwa ncdc | **PASS na ADC i ADCS** | Czat publiczny i prywatny, SCH z bezpośrednim UDP URES i przekazanym DRES, CTM, RCM i odpowiedź CTM, wyjście i ponowne logowanie. |
| Autoryzacja | **PASS** | Złe hasło i duplikat nicku odrzucone. Poprawne ponowne logowanie i zwolnienie limitu sesji potwierdzone. |
| Stabilność | **PASS** | 20 cykli, 10 powrotów po odmowie i dwa ncdc po restarcie. Hub przyjął i zamknął po 41 sesji przed restartem. |
| Kompletność dowodów | **FAIL** | `systemd.sandbox` wymaga odizolowanego menedżera systemd. Formalny odbiór wymaga decyzji OWNER o samodzielnym zaufaniu do zweryfikowanego KEYP. |

## Przebiegi CTest

Szeroki przebieg zwrócił **256 PASS, 4 FAIL, 0 SKIP na poziomie CTest**, kod 8. W powtórce z `umask 022` wszystkie cztery wpisy poniżej zwróciły **PASS**, kod 0. Wcześniejsza maska `077` ograniczała prawa plików testowych.

| Test | Pierwszy przebieg | Powtórka |
| --- | --- | --- |
| `beta.07.ci.deb.prototype` | FAIL | PASS |
| `apache2.https` | FAIL | PASS |
| `apache2.https.proxy` | FAIL | PASS |
| `apache2.remote.gateway` | FAIL | PASS |

`systemd.sandbox` miał status **BLOCKED / NOT RUN**. `beta.05.stage4.rollback` wewnętrznie pominął jedyną próbę, a `apache2.remote.gateway` wewnętrznie pominął próbę długotrwałą. Obie pominięte próby fixture są przeznaczone dla hosted Ubuntu.

## Dowody

- [Oczyszczony log z poleceniami i wynikami](2026-11-26-debian13-ncdc-stage-i-1bbcd54.redacted.log)
- SHA-256 logu `c4b0b04b8107a5ccf05f04ce1a4ad7dd2247d740623ea0a0369a88afd9335ec9`
- SHA-256 lokalnego raportu źródłowego `8bd64d4fef0fdc61ab71aecdcf1ac1eeb6a8a37751c798d5b3cc55435e99f258`
- SHA-256 binarki `a14d47bd7d4418038e336d35f138fdf69385e92a07ab2f0156757ed75a61b941`
