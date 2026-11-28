# Etap V — kwalifikacja bezpieczeństwa i testy end-to-end

**Kwalifikacja pozostaje otwarta. Oczekują decyzja OWNER STAGE V ACCEPTANCE i odrębna zgoda na scalenie.** [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10) · [English](../us/STAGE-V-SECURITY-E2E-ACCEPTANCE.md)

## Źródła i środowisko

Gałąź Etapu V powstała ze wskazanego SHA `3486b268bb6c44e73072f4e7490ae0417d6dd5b7` po PR #30. Po scaleniu PR #31 podstawą został `6ff15ad4f03be3b3cdce475fe7b0c31ca0134168`. Kwalifikację lokalną wykonano na SHA `d6f80e5fe2f0dee8c14db995407deaef0bf47f89` na VPS z Ubuntu 24.04.5 LTS 28 listopada 2026 r. UTC. Ten commit dodaje testy i zbieranie dowodów w CI. Kolejne commity z raportem wymagają własnych wyników GitHub Actions. [PR #30](https://github.com/adrianf87/dc24h.eu-twilight/pull/30) · [PR #31](https://github.com/adrianf87/dc24h.eu-twilight/pull/31)

Wdrożony WebAdmin na TCP 7178 ma backend `6011b09d089af4fc837493a9a703a93601a35bf8` z lokalną zmianą portu i interfejs `be1d0d232608277e5d26bedfd0910db4598f9421`. Drzewo źródeł backendu jest zgodne z `3486b26`, a interfejsu z `6ff15ad`. Jedenaście plików zgadzało się z manifestem wdrożenia. Suma uruchomionego obrazu ADCS na TCP 1512 odpowiada SHA `795b9a91bacea49d4a9eca7745430fa316d64133`. Identyfikatory procesów usług produkcyjnych pozostały stabilne przez kwalifikację. [Obserwacje wdrożenia](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/deployed-observations.log)

PASS oznacza spełnienie oczekiwania danego testu. FAIL oznacza niespełnione oczekiwanie. SKIP oznacza świadomie pominięty przypadek. NOT TESTED oznacza brak wyniku. WebAuthn, passkeys i rpID mają status N/A dla zatwierdzonego logowania hasłem i kodem e-mail OTP.

## WebAdmin i HTTPS

| Wymaganie | Wynik | Dowód i zakres |
| --- | --- | --- |
| Hasło i ośmiocyfrowy OTP, błędne hasło i kod, wygaśnięcie i wykorzystanie kodu | PASS | Bramka Node korzystała z rzeczywistej usługi C++ i MariaDB z pocztą testową. Brak OTP pozostawiał sesję nieuwierzytelnioną. Równoczesna weryfikacja zaakceptowała kod tylko raz. Chromium sprawdził także interfejs z odpowiedziami testowego API. |
| Rejestracja i weryfikacja adresu e-mail | PASS | Rejestracja wymagała potwierdzenia skrzynki i nowego logowania. Sześć testów SMTP sprawdziło lokalny TLS, dostarczenie, odmowę i przekroczenie limitu czasu. |
| Zewnętrzna dostawa SMTP i logowanie na produkcji w przeglądarce | NOT TESTED | Ten przebieg korzystał z jednorazowych kont i lokalnego serwera testowego SMTP. Obserwacje wdrożenia wykonywano bez uwierzytelniania. |
| Wygaśnięcie sesji, wylogowanie, cofnięcie dostępu i zmiana hasła | PASS | Magazyn sesji wygaszał dostęp po 120 sekundach bezczynności i po 600 sekundach całkowitego czasu. Zmiana hasła, jawne cofnięcie sesji i zmiana rewizji konta unieważniały dostęp. |
| Limity logowania i wysyłki OTP | PASS | Kontrolowane próby kończyły się oczekiwaną odmową. Piąta próba wysyłki w oknie konta nie generowała wiadomości. |
| Owner, Administrator, Registered i konto nieaktywne | PASS | Test na jednorazowych danych zweryfikował rolę po logowaniu, odmowę Registered dla kont, banów i audytu oraz cofnięcie sesji po zmianie roli lub stanu konta. Testy natywne sprawdziły hierarchię i poświadczenia. |
| Bieżące hasło i audyt operacji uprzywilejowanych | PASS | Błędne hasło blokowało zapis oraz odczyt audytu. Odmowa zapisu pozostawiła stan kont i audyt operacji zakończonych powodzeniem bez zmian. Dozwolone operacje kont i banów zapisały zdarzenia. |
| Certyfikat HTTPS, zaufanie CA, nazwa hosta i SAN | PASS | VPS Ubuntu połączył się z użyciem systemowego magazynu CA na 7178 i 1512. Obie usługi prezentowały ten sam certyfikat co plik lokalny, ważny do 25 lutego 2027 r. o 22:51:28 UTC. |
| Flagi ciasteczek i wydanie sesji | PASS | Ciasteczko wyzwania z HTTPS miało `Secure`, `HttpOnly`, `SameSite=Strict`, `Path=/` i brak `Domain`. Izolowany test uwierzytelnienia potwierdził te atrybuty oraz wydanie osobnej sesji po OTP. |
| Oczekiwane odmowy żądań | PASS | Izolowane testy obejmowały brak CSRF i reguły Origin, Host oraz nagłówków proxy. Zwykłe żądania HTTPS zwróciły 401 dla odczytów bez sesji oraz 403 dla niedozwolonej metody i nieznanej ścieżki. |
| Aktywne próby obejścia i odtwarzania podatności | SKIP | Kwalifikacja obejmowała defensywne asercje, oczekiwane odmowy i dozwolone zmiany stanu na danych testowych. |
| Lokalne nasłuchy usług prywatnych | PASS | C++ i most działały przez zabezpieczone uprawnieniami gniazda Unix. MariaDB miała `skip_networking=1`. Node nasłuchiwał na `127.0.0.1:4278`. Porty TCP 1511 i 3306 nie miały nasłuchu. |
| Niezależna weryfikacja izolacji od Internetu i dostępu HTTPS | NOT TESTED | Weryfikacja wdrożonych reguł HTTPS, portów prywatnych i administracji wymaga oddzielnego, autoryzowanego punktu pomiaru. |

## Klienci ADC i ADCS

| Klient lub właściwość | Wynik | Dowód i zakres |
| --- | --- | --- |
| ncdc 1.23.1 na ADC i ADCS | PASS | Logowanie gościa na loopback, czat publiczny i prywatny, wyszukiwanie, tryb aktywny i pasywny, CTM, RCM i ponowne połączenie. Przeszło dziesięć profili klienta. |
| Dostęp do kont ADC | PASS | Testy procesu C++ z MariaDB sprawdziły poświadczenia i odmowy niezależnie od logowania gościa ncdc. |
| Referencyjny TLS i KEYP ncdc | PASS | Izolowane testy TLS sprawdziły zaufany łańcuch i SAN, odrzuciły niezaufany łańcuch, błędny SAN i wygasły certyfikat oraz sprawdziły przełączenie certyfikatu. ncdc odrzucił KEYP innego certyfikatu i zalogował się z poprawnym. |
| Rozłączenia, limity i wykorzystanie zasobów | PASS | Testy natywne objęły ponowne użycie slotu, odmowę po osiągnięciu limitu, zerwanie połączenia, limit deskryptorów i ograniczanie tempa czatu. Próba lokalna zakończyła 12/12 operacji ADC i 12/12 ADCS. |
| AirDC++ Web Client 2.14.0 na ADC i ADCS | PASS | Oficjalny przenośny klient linuksowy zakończył po dwie sesje gościa na każdym transporcie. Hub zwolnił wszystkie cztery połączenia. ADCS użył jawnego testowego KEYP. Próba obejmowała logowanie, rozłączenie i ponowne połączenie. [Klient linuksowy](https://airdcpp.net/docs/installation/linux-binaries.html) · [Wynik](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/airdcpp.log) |
| DC++ | SKIP | Klient producenta wymaga środowiska testowego Windows lub Wine, którego nie było na VPS Ubuntu. [Wydania DC++](https://dcplusplus.sourceforge.io/download.html) |
| Kwalifikacja Debian 13 | SKIP | Ten przebieg objął obecny VPS Ubuntu. Wcześniejsze dowody z portu 7278 na Debianie dotyczą innego wdrożenia. |

## Odtwarzanie i otwarte punkty P0

| Wymaganie | Wynik | Dowód i zakres |
| --- | --- | --- |
| Izolowany restart i odtworzenie stanu | PASS | Tymczasowa jednostka systemd z hubem na koncie bez praw roota odzyskała działanie po przerwaniu procesu. Jednorazowa MariaDB przeszła restart, odmowę zmienionej sumy migracji oraz odtworzenie dopasowanych danych i konfiguracji z unieważnieniem sesji. |
| Gotowość do odnowienia certyfikatu | FAIL | `certbot.timer` był wyłączony, a systemd wyklucza wykonywanie zadania cron z pakietu. Nie znaleziono wykonywalnych hooków wdrożeniowych Certbota. ADCS zwracał `CanReload=no`. Izolowana wymiana certyfikatu przeszła. Harmonogram odnowienia i sposób przyjęcia certyfikatu przez usługi wymagają weryfikacji. |
| Przegląd logów i artefaktów | PASS | W ograniczonej próbce journala i logów Apache nie znaleziono dopasowań do sprawdzanych wzorców sekretów. Opublikowane dowody zawierają statusy i nazwy testów bez surowych transkryptów i poświadczeń. |
| Utrata skrzynki Owner | NOT TESTED | Izolowany test sprawdził odmowę dostawy, a natywny test administracyjny przeniesienie własności. We wdrożeniu jest jeden aktywny zweryfikowany Owner i zero aktywnych zweryfikowanych Administratorów. Odzyskanie skrzynki u dostawcy lub kwalifikacja ścieżki zastępczej pozostaje otwarta. |
| Rollback pomiędzy scalonymi wersjami interfejsu | PASS | Chromium sprawdził na odizolowanym serwerze powrót z interfejsu PR #31 do PR #30, w tym formularz logowania i sumy czterech plików. |
| Rollback pełnego wydania pomiędzy różnymi wersjami | NOT TESTED | Odtworzenie MariaDB użyło dopasowanych danych i konfiguracji jednej wersji. Rollback pełnego wydania pomiędzy różnymi zaakceptowanymi wersjami backendu pozostaje do kwalifikacji. |
| CI GitHub dla dokładnego SHA | NOT TESTED | GitHub Actions ma skonfigurowane Ubuntu 22.04, 24.04 i 26.04. Wyniki tych zadań pozostają oczekujące w tym raporcie. |

Przegląd P0 pozostaje otwarty ze względu na odnowienie certyfikatu, niezależną weryfikację ekspozycji, odzyskanie skrzynki Owner, pełny rollback i brakujące wyniki klientów oraz CI. Decyzja OWNER i odrębna zgoda na scalenie wymagają przeglądu macierzy. [Issue #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10)

## Dowody i odtworzenie testów

Naprawiono uruchamianie zestawu procesowego TLS, który wcześniej kończył się bez wykonania asercji. Po poprawieniu generowania wygasłego certyfikatu i próby wznowienia sesji przeszło osiem jego testów. Lokalnie przeszły 33 przypadki natywne CTest, 22 przypadki Node, pięć przypadków Chromium, przypadek piaskownicy systemd i kwalifikacja MariaDB. Jeden przypadek odtwarzania przeznaczony dla GitHub-hosted zgłosił pominięcie wewnątrz CTest. Dodatkowo wykonano lokalne testy SMTP, mostu Unix, KEYP ncdc i połączeń loopback. Workflow rejestruje dokładny SHA i przesyła raport JUnit, log statusów i sumy SHA-256 dla każdego obrazu Ubuntu. Zawiera filtr CTest i polecenia uruchomienia testów. [Workflow Ubuntu](../../.github/workflows/stage5-security-e2e.yml)

- [JUnit](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/qualification.xml), [log po redakcji](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/qualification.log) i [podsumowanie wyników](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/qualification.json)
- [Środowisko, SHA i sumy plików wykonywalnych](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/provenance.json), [pomiary loopback](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/loopback-probes.json) i [manifest SHA-256](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/SHA256SUMS)
- [Obserwacje na VPS](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/operations-observations.log), [izolowany restart](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/systemd-restart.log), [próba AirDC++](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/airdcpp.log) i [rollback interfejsu](../../logs/tests/stage5-ubuntu-24.04-d6f80e5/ui-rollback.log)
