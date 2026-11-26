# Publiczny hub — Etap I: Debian 13 / ncdc / ADCS

**Zadanie:** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10), Etap I (P0).
**Zakres:** wyjaśnienie zachowania klienta i poprawa wiarygodności testów; **bez uruchamiania portów publicznych, migracji danych lub wydania**.
**Status:** prace inżynierskie; **PUBLIC / PRODUCTION NO-GO** do oddzielnej akceptacji OWNER i powtórnych testów na Debianie 13.

## Materiał wejściowy

[Oczyszczony raport Debian 13 + ncdc](../../logs/tests/2026-11-26-debian13-ncdc-1f2ce42.redacted.log) dotyczy wcześniejszego commita `1f2ce425e192a71c6ca2499e9c613e5d8d14ba78`.

- Kompilacja Release, ADC, dwa profile ncdc i stabilność: **PASS** w raporcie, tylko lokalne IPv4.
- CTest: **40 PASS, 2 FAIL**, a 218 testów nie włączono do wybranego podzbioru. `adc.ctm.process` i `adc.rcm.process` próbowały nasłuchu na IPv6 `::1` w izolowanym środowisku wyłącznie IPv4.
- ADCS: raport oznaczył **FAIL**, ponieważ klient referencyjny odrzucił niezgodny SAN IP, ale ncdc połączył się z **jawnie przypiętym i wcześniej zweryfikowanym** odciskiem SHA-256 certyfikatu. Te dwa tryby zaufania nie są równoważne.

Pełny pierwotny prompt, surowe logi i CTest XML **nie są jeszcze dostępne** w repozytorium. Nie traktować tych materiałów jako niezależnie potwierdzonych.

## ADCS — polityka certyfikatów

Specyfikacja [ADC KEYP](https://dc-protocols.github.io/ADC-EXT.html#_keyp_certificate_substitution_protection_in_conjunction_with_adcs) dopuszcza uwierzytelnienie certyfikatu przez SHA-256 keyprint przekazany zaufanym kanałem. [ncdc](https://dev.yorhel.nl/ncdc/man) obsługuje adresy `adcs://.../?kp=SHA256/<base32-keyprint>`. To inny model niż weryfikacja CA oraz SAN DNS/IP.

- **CA + nazwa:** certyfikat z samym `DNS:localhost` powinien działać dla `localhost`, ale zostać odrzucony dla `127.0.0.1`. Dodano regresję w referencyjnym kliencie TLS.
- **Świadomy pin w ncdc:** zgodny, bezpiecznie uzyskany odcisk może wystarczyć do uwierzytelnienia bez zgodnego SAN. Nie nazywać tego automatycznie błędem huba; wymagana jawna decyzja OWNER o polityce zaufania.
- **Nieprawidłowy/stary pin:** podstawiony certyfikat powinien zostać odrzucony; powtórzyć test dla dokładnej wersji ncdc.
- **Ryzyko:** odcisk otrzymany z niepewnego źródła może zostać podmieniony razem z adresem. Dla publicznego huba zalecana domena, poprawny SAN i certyfikat CA; pin wyłącznie z zaufanego źródła. Wymiana certyfikatu może wymagać aktualizacji przypiętego odcisku.

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

## Kryteria odbioru Etapu I (otwarte)

- [ ] Powtórzyć CTM/RCM na prawdziwym Debianie 13 IPv4-only i na hoście z działającym IPv6; zachować jawne PASS/SKIP/FAIL.
- [ ] Powtórzyć macierz certyfikatów: poprawny/wadliwy SAN, poprawny/zły pin, obcy certyfikat, ważność, restart i ponowne logowanie.
- [ ] Ustalić politykę OWNER dla publicznego ADCS: CA+DNS/SAN, ewentualnie zaufany pin KEYP; nie przypisywać klientowi potwierdzonego sprawdzania obu metod, jeśli działa tylko jedna.
- [ ] Dostarczyć pierwotny prompt i zanonimizowane logi/CTest XML, wersje środowiska, dokładny SHA oraz SHA-256 dowodów.
- [ ] Potwierdzić testy negatywne, dwa profile ncdc, 20 cykli, 10 reconnectów i brak niezałatwionych P0.
- [ ] Przejść GitHub-hosted CI dla finalnego SHA i uzyskać osobną zgodę OWNER na merge.

**Uwaga:** zakończenie prac w repozytorium nie oznacza zgody na publiczne uruchomienie huba ani WebAdmin.
