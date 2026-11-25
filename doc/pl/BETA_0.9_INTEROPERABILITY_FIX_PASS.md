# Beta 0.9.x — poprawki ADC/ADCS, klientów, TLS i uprawnień

Gałąź robocza: `beta-0.9.x-stage-full-fix-pass`, utworzona na bazie
`beta-0.9.x-stage-full` w commicie `eaafa082713e8d45de3125d36b31aa3b8d436525`.
To dodatkowy przegląd na prośbę Właściciela, **a nie** automatyczna
akceptacja żadnego z pięciu formalnych Fix Passów zadania #242.
Ten dokument nie upoważnia do zmian w `main`, uprawnieniach produkcyjnych,
schemacie bazy danych, domyślnych nasłuchach ani architekturze unieważniania kont.

## Wprowadzone poprawki

1. **Certyfikat serwera ADCS:** odrzucany jest certyfikat X.509 typu CA
   użyty jako końcowy certyfikat TLS huba — również wtedy, gdy certyfikat CA
   dopuszcza `serverAuth` i `digitalSignature`. Nieprawidłowe rozszerzenie
   `basicConstraints` także powoduje odmowę. Dzieje się to podczas uruchamiania
   lub `--check-config`, zanim otwarty zostanie port ADCS.
2. **Uprawnienie do podpisu w TLS 1.2:** przy dopuszczonych zestawach szyfrów
   ECDHE-only certyfikat z jawnym rozszerzeniem `keyUsage` musi umożliwiać
   `digitalSignature`. Samo `keyEncipherment` albo `keyAgreement` nie wystarcza.
   Aktualne certyfikaty podpisujące RSA/P-256/Ed25519 pozostają obsługiwane.
   Samodzielne polecenie `openssl req -x509` może domyślnie wygenerować
   `CA:TRUE`, dlatego izolowane testy ADCS/VM jawnie ustawiają `CA:FALSE`.
   Przed aktualizacją należy sprawdzić i wymienić takie certyfikaty serwera:
   dawny certyfikat CA używany jako końcowy będzie celowo odrzucany.
3. **Regresja klienta ADC/ADCS i autoryzacji:** symulowany klient w stylu DC++
   ogłasza pola `APDC++`, `VE`, `LO`, `CT1` i `SU` przy logowaniu zarówno po
   niezaszyfrowanym ADC, jak i ADCS z weryfikacją certyfikatu. Pola publiczne
   są przekazywane bez tajnego pola `PD` i nieznanego pola `XX`. Późniejsza
   deklaracja uprawnień operatora `CT4` zostaje odrzucona, a sesja rozłączona,
   niezależnie od identyfikatora klienta przypominającego DC++.

## Zakres potwierdzenia testami

- Test polityki TLS w C++ kompiluje się niezależnie z OpenSSL 3 i sprawdza
  akceptację lub odmowę dla certyfikatów prawidłowych, spoza CA, wygasłych,
  ze słabymi parametrami, błędnym EKU, typu CA oraz bez uprawnienia do podpisu.
  Testy procesowe sprawdzają także odmowę `--check-config` dla certyfikatów
  typu CA i bez uprawnienia do podpisu.
- Test ADC/ADCS **symuluje metadane protokołu klienta**. Nie uruchamia
  prawdziwych programów DC++ ani AirDC++. Ich zgodność nie jest zatem
  certyfikowana. Niezależne dowody dla przypiętej wersji ncdc pozostają ważne.
- Test uprawnień sprawdza **odrzucenie publicznych deklaracji przywilejów**.
  Nie wprowadza nowych ról, delegowania VIP, uprawnień moderacyjnych ani
  mechanizmu unieważniania kont bez restartu. Autorytet ról nadal wynika
  z uwierzytelnionej sesji i obrazu kont po stronie serwera.
- Istniejące testy nazwy hosta, SNI i CA nadal wiążą zaufanie z decyzją
  klienta. Nie oznacza to filtrowania nazw hostów SNI po stronie serwera.

## Granica wydania

Commit można poddać testom CI na hostowanych przez GitHub Ubuntu 22.04,
24.04 i 26.04, osobno zapisując dokładny SHA. Pomyślny izolowany test lub
oczekujące workflow nie oznaczają pełnej akceptacji wydania dla tego SHA.
Certyfikacja prawdziwych klientów, formalny rejestr Fix Passów, akceptacja
Właściciela oraz osobna zgoda na merge do chronionego `main` nadal są wymagane.
