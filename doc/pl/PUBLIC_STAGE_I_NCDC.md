# Publiczny hub — Etap I: Debian 13 / ncdc / ADCS

**Zadanie:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), Etap I (P0).
**Zakres:** wyjaśnienie zachowania klienta i poprawa wiarygodności testów; **bez uruchamiania portów publicznych, migracji danych lub wydania**.
**Status:** prace inżynierskie; **PUBLIC / PRODUCTION NO-GO** do oddzielnej akceptacji OWNER i powtórnych testów na Debianie 13.

## Decyzja OWNER — polityka zaufania TLS/ADCS (zatwierdzona 2026-11-26)

**Dla przyszłego publicznego huba obowiązują następujące zasady:**

1. **Wymóg po stronie serwera:** użyć zaakceptowanej przez OWNER **publicznej domeny DNS**, aktualnego certyfikatu wystawionego przez **publicznie zaufany CA** z pełnym, poprawnie weryfikowalnym łańcuchem i `subjectAltName` zawierającym **dokładną nazwę DNS, pod którą łączy się klient**. Identyfikator `server.name = dc24h.eu-twilight` nie jest domeną. Sam adres IPv4 nie zastępuje tożsamości DNS; dla połączenia po adresie IP klient weryfikujący nazwę IP wymaga właściwego **IP SAN**. Nie akceptujemy błędnego SAN jako docelowej konfiguracji publicznej.
2. **Opcjonalnie KEYP w ncdc:** dopuszczalne jest świadome połączenie `adcs://<zatwierdzony-host>:<port>/?kp=SHA256/<odcisk-base32>`, ale **wyłącznie po niezależnym, bezpiecznym potwierdzeniu dokładnego odcisku SHA-256 certyfikatu przez uwierzytelniony kanał poza pasmem** (np. osobno zweryfikowany komunikat podpisany przez OWNER). Odcisk należy porównać znak po znaku **przed pierwszym połączeniem**. Adres i odcisk przekazane razem przez niezweryfikowane źródło nie stanowią niezależnego potwierdzenia. Nie pobierać odcisku z tego samego niezaufanego połączenia TLS.
3. **Modele zaufania są różne:** test ncdc 1.23.1 wskazuje, że zgodny KEYP może uwierzytelnić certyfikat mimo niezgodnego SAN. **Nie dowodzi to jednoczesnej walidacji CA + SAN**. Dopuszczenie trybu KEYP po stronie klienta **nie znosi wymogu** zaufanego CA i prawidłowego DNS SAN dla publicznego serwera.
4. **Odmowa i rotacja:** błędny/nieaktualny pin, podstawiony certyfikat, nieodpowiednia ważność lub łańcuch, albo błędny SAN w trybie CA+SAN muszą skutkować odmową. Zakaz automatycznego TOFU, pomijania weryfikacji TLS i cichego przejścia na `adc://`. Odnowienie certyfikatu wymaga zaplanowania bezpiecznej dystrybucji i **niezależnego potwierdzenia nowego odcisku liścia przed przełączeniem** oraz ponownego testu klientów ncdc. Nie publikować kluczy prywatnych w Git/CI.
5. **Ta zgoda dotyczy tylko polityki zaufania.** Domena, potwierdzenie własności DNS, wystawienie i instalacja certyfikatu, otwarcie portów oraz uruchomienie publicznego nasłuchu należą do późniejszego Etapu III i wymagają odrębnej decyzji OWNER o aktywacji.

**Warunki testu odbiorowego:** zanotować nazwę DNS, SAN, wystawcę i łańcuch; PASS dla poprawnego CA+SAN; FAIL dla niewłaściwego SAN i obcego CA, nieważnego lub jeszcze nieważnego certyfikatu; PASS dla ncdc z niezależnie potwierdzonym KEYP; FAIL dla błędnego/starego pina, wymienionego certyfikatu i próby fallback na plaintext; bezpieczna rotacja, dokładne wersje klienta/hosta i SHA źródeł. Przy każdym PASS rozróżnić walidację **CA+SAN** od **KEYP**.

## Materiał wejściowy

[Oczyszczony raport Debian 13 + ncdc](../../logs/tests/2026-11-26-debian13-ncdc-1f2ce42.redacted.log) dotyczy wcześniejszego commita `1f2ce425e192a71c6ca2499e9c613e5d8d14ba78`.

- Kompilacja Release, ADC, dwa profile ncdc i stabilność: **PASS** w raporcie, tylko lokalne IPv4.
- CTest: **40 PASS, 2 FAIL**, a 218 testów nie włączono do wybranego podzbioru. `adc.ctm.process` i `adc.rcm.process` próbowały nasłuchu na IPv6 `::1` w izolowanym środowisku wyłącznie IPv4.
- ADCS: raport oznaczył **FAIL**, ponieważ klient referencyjny odrzucił niezgodny SAN IP, ale ncdc połączył się z **jawnie przypiętym i wcześniej zweryfikowanym** odciskiem SHA-256 certyfikatu. Te dwa tryby zaufania nie są równoważne.

Pełny pierwotny prompt, surowe logi i CTest XML **nie są jeszcze dostępne** w repozytorium. Nie traktować tych materiałów jako niezależnie potwierdzonych.

## ADCS — polityka certyfikatów

Specyfikacja [ADC KEYP](https://dc-protocols.github.io/ADC-EXT.html#_keyp_certificate_substitution_protection_in_conjunction_with_adcs) dopuszcza uwierzytelnienie certyfikatu przez SHA-256 keyprint przekazany zaufanym kanałem. [ncdc](https://dev.yorhel.nl/ncdc/man) obsługuje adresy `adcs://.../?kp=SHA256/<base32-keyprint>`. To inny model niż weryfikacja CA oraz SAN DNS/IP.

- **CA + nazwa:** certyfikat z samym `DNS:localhost` powinien działać dla `localhost`, ale zostać odrzucony dla `127.0.0.1`. Dodano regresję w referencyjnym kliencie TLS.
- **Świadomy pin w ncdc:** zgodny, bezpiecznie uzyskany odcisk może wystarczyć do uwierzytelnienia bez zgodnego SAN. Nie nazywać tego automatycznie błędem huba; decyzję OWNER zapisano powyżej.
- **Nieprawidłowy/stary pin:** podstawiony certyfikat powinien zostać odrzucony; powtórzyć test dla dokładnej wersji ncdc.
- **Ryzyko:** odcisk otrzymany z niepewnego źródła może zostać podmieniony razem z adresem. Dla publicznego huba **wymagana** zatwierdzona domena, poprawny DNS SAN i certyfikat zaufanego CA; pin wyłącznie z niezależnie zweryfikowanego źródła. Wymiana certyfikatu może wymagać aktualizacji przypiętego odcisku.

Nie wyłączać walidacji TLS ani nie przechodzić po cichu na ADC bez TLS.

## CTest — brak IPv6 kontra błąd kodu

Testy CTM/RCM sprawdzają teraz możliwość rzeczywistego zbindowania `::1`:

1. Gdy IPv6 działa, uruchamiają warianty **IPv4 oraz IPv6** dla ADC/ADCS.
2. Gdy IPv6 loopback jest niedostępny w izolowanym środowisku, testują IPv4 i jawnie wypisują `SKIP IPv6 ... IPv4 coverage only`. CTest PASS w takim środowisku **nie oznacza pełnego pokrycia IPv6**.
3. Zmienna `DC24H_REQUIRE_IPV6_LOOPBACK=1` wymusza błąd, jeśli host deklarowany jako dual-stack nie może zbindować `::1`. Nieoczekiwane błędy gniazd pozostają błędami.
4. Nowy test `stage1.ipv6_loopback_policy` sprawdza rozstrzyganie tych przypadków.

Przykładowe polecenia na **izolowanym** hoście:

```sh
ctest --test-dir build --output-on-failure -R '^(adc\.ctm\.process|adc\.rcm\.process|stage1\.ipv6_loopback_policy)$'
DC24H_REQUIRE_IPV6_LOOPBACK=1 ctest --test-dir build --output-on-failure -R '^(adc\.ctm\.process|adc\.rcm\.process)$'
ctest --test-dir build --output-on-failure -R '^adcs\.process$'
```

## Powtórka Debian 13.8 — materiały Etapu I

[Podsumowanie testów Debian 13.8 i ncdc 1.23.1](../../logs/tests/2026-11-26-debian13-ncdc-stage-i-1bbcd54.summary.md) oraz [zanonimizowany log](../../logs/tests/2026-11-26-debian13-ncdc-stage-i-1bbcd54.redacted.log) dokumentują PASS dla CA+SAN, KEYP poprawnego i błędnego, IPv6 strict oraz dwóch klientów ncdc. Szeroki CTest uzyskał **256 PASS / 4 FAIL** przy `umask 077`; cztery przypadki osobno przeszły przy `umask 022`, ale **nie zastępuje to pełnej powtórki** w jednakowych warunkach. `systemd.sandbox` jest nadal **BLOCKED/NOT RUN**. **ETAP I: BLOCKED; PUBLIC NO-GO**.

## Kryteria odbioru Etapu I (otwarte)

- [ ] Powtórzyć CTM/RCM na prawdziwym Debianie 13 IPv4-only i na hoście z działającym IPv6; zachować jawne PASS/SKIP/FAIL.
- [ ] Powtórzyć macierz certyfikatów: poprawny/wadliwy SAN, poprawny/zły pin, obcy certyfikat, ważność, restart i ponowne logowanie.
- [x] Ustalić politykę OWNER dla publicznego ADCS: obowiązkowy zaufany CA+DNS/SAN po stronie serwera, opcjonalny niezależnie potwierdzony pin KEYP w ncdc; nie przypisywać klientowi obu walidacji, jeśli faktycznie działa w trybie tylko KEYP. **Decyzja 2026-11-26; wdrożenie i testy końcowe nadal otwarte.**
- [ ] Dostarczyć pierwotny prompt i zanonimizowane logi/CTest XML, wersje środowiska, dokładny SHA oraz SHA-256 dowodów.
- [ ] Potwierdzić testy negatywne, dwa profile ncdc, 20 cykli, 10 reconnectów i brak niezałatwionych P0.
- [ ] Przejść GitHub-hosted CI dla finalnego SHA i uzyskać osobną zgodę OWNER na merge.

**Uwaga:** zakończenie prac w repozytorium nie oznacza zgody na publiczne uruchomienie huba ani WebAdmin.
