# ADCS / TLS — alpha-0.1.4

**Zarządzane odnawianie:** Wdrożenie Ubuntu pozwala wczytać certyfikat i klucz w działającym hubie przez prywatne API Unix. Nowe połączenia używają zweryfikowanego certyfikatu, a zestawione zachowują swój stan TLS. [Automatyczne odnawianie TLS](TLS_AUTO_RENEWAL.md)

**alpha-0.9.5 Bezpieczeństwo TLS:** Hub sprawdza certyfikat liścia przed nasłuchem i ogranicza nasłuch do TLS 1.2 i 1.3 AEAD. Klient nadal sprawdza nazwę i łańcuch zaufania. [Polityka certyfikatu i szyfrów ADCS](TLS_SECURITY.md)

**Uściślenie Etapu I publicznego wdrożenia:** weryfikacja CA/nazwy hosta i jawne przypięcie odcisku SHA-256 certyfikatu w ADC KEYP są różnymi trybami zaufania klienta. W szczególności ncdc ze świadomie zaufanym `?kp=SHA256/...` może zaakceptować certyfikat bez SAN zgodnego z adresem URL; hub nie jest w stanie stwierdzić, czy klient sprawdził nazwę. Zobacz [macierz odbioru Debian 13/ncdc](PUBLIC_STAGE_I_NCDC.md). Publiczne użycie wymaga osobnego zatwierdzenia tożsamości certyfikatu i zasad wdrożenia.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

**Etap dostarczony w alpha-0.2.10:** [Bezpieczeństwo negocjacji](NEGOTIATION_SECURITY.md) dodaje limity oczekujących sesji, łączne budżety wejścia do gotowości i wspólne limity prób przed INF/Tiger. Opisy historyczne poniżej zachowują pierwotny zakres.

**Etap dostarczony w alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

**Integracja dostarczona w alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

ADCS szyfruje strumień ADC przez TLS, bez zmiany składni i bez STARTTLS. Źródła: [rozszerzenie ADCS](https://dc-protocols.github.io/ADC-EXT.html), [błędy I/O OpenSSL](https://docs.openssl.org/3.5/man3/SSL_get_error/) i [SSL_accept](https://docs.openssl.org/3.5/man3/SSL_accept/).

`adcs::Context` wczytuje pełny łańcuch PEM i zgodny, nieszyfrowany klucz prywatny PEM przed otwarciem portów. Brak plików, błędne dane, niezgodny lub zaszyfrowany hasłem klucz powodują kod 1 bez ogłoszenia gotowości i bez pytania o hasło. Kontrola dotyczy wczytania i zgodności klucza; zaufanie, nazwę hosta i ważność certyfikatu sprawdza klient. Brak wzajemnego TLS i uwierzytelniania kont.

Minimum to TLS 1.2. Kompresja, renegocjacja, cache sesji i bilety sesyjne (również TLS 1.3) są wyłączone. Nasłuch używa wyłącznie wybranych szyfrów AEAD. Certyfikat końcowy nie może mieć `CA:TRUE`; jawne użycie klucza musi dopuszczać `digitalSignature`. Klient nadal weryfikuje łańcuch zaufania i nazwę serwera. TLS nie nadaje uprawnień kontom ADC.

```sh
./build/dc24h --adcs-listen 127.0.0.1:1512 \
  --tls-cert /etc/dc24h/tls/fullchain.pem \
  --tls-key /etc/dc24h/tls/privkey.pem
```

Samo `--adcs-listen` uruchamia wyłącznie TLS. Dodaj jawne `--listen`, aby włączyć również zwykły TCP. Wszystkie porty współdzielą limit klientów. Nieblokujące SSL_accept/SSL_read_ex obsługują WANT_READ/WANT_WRITE. Kolejka błędów OpenSSL jest czyszczona przed operacją, a odszyfrowane dane buforowane są obsługiwane także bez nowego zdarzenia gniazda. Błąd klienta zamyka tylko jego sesję. Program ignoruje SIGPIPE.

Negocjacja ma termin `min(idle_timeout, tls_handshake_timeout)` (domyślny termin TLS: 10 sekund) od przyjęcia; częściowe dane nie przedłużają go. Po negocjacji odczyty danych odnawiają zwykły timeout. `received_bytes` obejmuje tylko odszyfrowane dane ADC. `handshakes` liczy udane negocjacje, `incomplete` sesje zamknięte przed ich ukończeniem, także przez timeout i stop. Nie oznacza to logowania ADC.

Klucze przechowuj poza Git, z dostępem ograniczonym do konta usługi. Przy instalacji poza mechanizmem zarządzania zaplanuj restart do zastosowania certyfikatu. Zatrzymanie zamyka gniazda od razu, bez wymiany TLS close_notify. Testy obejmują negocjacje ncdc przez ADCS. [Certyfikat testowy](../us/ADCS_TLS.md) · [Testy](TESTING.md)

Ścieżki TLS można teraz podać w INI; względne odnoszą się do jego katalogu. `--check-config` wczytuje certyfikat i klucz bez otwierania portów/logu. Zobacz [konfigurację](CONFIGURATION.md).

Od alpha-0.1.7 wspólne limity IP działają przed alokacją sesji TLS. Termin bezwzględny sprawdzany jest przed uznaniem negocjacji za ukończoną. Zobacz [SECURITY.md](SECURITY.md).
