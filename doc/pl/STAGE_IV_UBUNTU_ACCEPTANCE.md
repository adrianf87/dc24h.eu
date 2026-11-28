# Weryfikacja WebAdmina Etapu IV na Ubuntu 24.04

28 listopada 2026 r. właściciel upoważnił do przeprowadzenia prac nad zakończeniem Etapu IV na VPS-ie z Ubuntu 24.04. Panel HTTPS działa pod wskazanym adresem `server-747585786.dc24h.eu:7178`. Weryfikacja lokalna zakończyła się pomyślnie. Niezależny test dostępu i akceptacja bezpieczeństwa pozostają otwarte w zgłoszeniu #10. Historyczny adres na porcie 7278 dotyczy odrębnego wdrożenia na Debianie 13.

## Dowody wdrożenia

System to Ubuntu 24.04.5 LTS. Wersja źródeł to `795b9a91bacea49d4a9eca7745430fa316d64133`. Wdrożenie używa lokalnej zamiany portu `7278` na `7178` w konfiguracji Apache, jednostce WebAdmina, szablonie inicjalizacji i dwóch plikach konfiguracji Node. SHA-256 różnicy Git wynosi `d851436e81752e14c8c43684af4de95f45eb3cedf8dc048f9b46de2497818150`. Nakładka systemd zawiera takie samo sprawdzenie gotowości gniazda mostu jak zaktualizowany szablon jednostki. Przy instalacji systemd raz automatycznie ponowił start. Po dodaniu sprawdzenia kontrolowany start Etapu IV zakończył się bez automatycznych restartów. Hub ADCS Etapu III nadal działał na porcie TCP 1512.

Sumy binariów, pakietów Node, mostu i konfiguracji Apache podano w [raporcie angielskim](../us/STAGE_IV_UBUNTU_ACCEPTANCE.md#deployment-evidence). Apache miał jeden wirtualny host HTTPS i przeszedł `configtest`. Node 24.22.3 działał jako `dc24h-webadmin` na `127.0.0.1:4278`. Serwis C++ i most działały jako `dc24h-stage3` na osobnych gniazdach uniksowych, odpowiednio z grupami `dc24h-stage3` i `dc24h-webadmin`. MariaDB 10.11.14 korzystała z gniazda uniksowego, a TCP było wyłączone. Baza produkcyjna miała 11 migracji natywnych i dwie migracje Etapu IV zgodne z katalogiem źródeł.

## Wyniki

| Zakres | Wynik |
| --- | --- |
| Działający serwer | Potwierdzono zaufany TLS 1.3, SNI, nazwę domeny i bezpieczny kontekst w Chromium. Apache odrzucił niepoprawny Host, Origin, metodę i prywatne trasy. Brama odmówiła dostępu do podsumowania bez sesji oraz odrzuciła żądanie bez CSRF. Ciasteczko wyzwania ma prefiks `__Host-`, `Secure`, `HttpOnly`, `SameSite=Strict`, `Path=/` i brak `Domain`. Zakres hosta dla ciasteczek obejmuje porty HTTPS. |
| Rzeczywiste logowanie na VPS-ie | Potwierdzono hasło Administratora, dostarczenie i akceptację ośmiocyfrowego OTP, rolę Owner i działanie usługi kont. Po wylogowaniu serwer odrzucił dostęp do podsumowania. Audyt zawierał zdarzenia weryfikacji adresu, logowania i wylogowania. |
| Testy aplikacji na danych syntetycznych | Przeszło 20 testów Node, 3 testy Chromium, 16 testów granic w Pythonie, 4 testy mostu i 2 testy wstępne. |
| Izolowana integracja C++/MariaDB | Potwierdzono odmowę przy awarii SMTP, błędnym lub wygasłym OTP, pięciu błędnych próbach i równoległym powtórzeniu kodu. Przeszły testy limitów logowania i rejestracji, rejestracji z weryfikacją adresu, odmowy nadania roli przy rejestracji, ról Owner/Administrator/Registered, CSRF przy zmianie hasła, unieważnienia obu aktywnych sesji po zmianie hasła oraz reakcji na zmianę roli lub statusu konta. |
| Izolowane odzyskiwanie | Przeszły restart osobnej MariaDB, wykrycie błędnej sumy migracji z zachowaniem danych kont, odtworzenie dopasowanej bazy i konfiguracji, ponowna weryfikacja schematu i uprawnień SQL oraz usunięcie sesji i wyzwań. Test używał osobnego katalogu danych, gniazda i przestrzeni montowań na tym VPS-ie. Odtworzenie używało tej samej wersji aplikacji. |

Na `795b9a9` GitHub raportował 6/6 workflowów i 41/41 checków zakończonych sukcesem. Na Ubuntu 24.04 wykonano testy Python, Node i Chromium, a workflow prywatnego magazynu sprawdził awarie. Dedykowany workflow publicznego WebAdmina z osobną MariaDB nie uruchomił się w tym pushu. Na wcześniejszym squash commicie `1faf188c` nie przeszły trzy checki Etapu III. PR #28 usunął automatyczne publiczne sprawdzenia Etapu III. [Bieżący job Ubuntu 24.04](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/46083343545/job/140787158373), [wcześniejszy przebieg Etapu III](https://github.com/adrianf87/dc24h.eu-twilight/actions/runs/46079787777).

## Odzyskiwanie i warunki akceptacji

W razie utraty lub przejęcia skrzynki Ownera użyj niezależnie uwierzytelnionej sesji operatora i [procedury obsługi Etapu IV](STAGE_IV_DEDICATED_SERVICE.md). Przy wątpliwościach co do kontroli skrzynki lub integralności aplikacji wycofaj wirtualny host WebAdmina i zachowaj dopasowaną, zaszyfrowaną kopię bazy oraz konfiguracji. Odzyskanie skrzynki wymaga procedury dostawcy. Przeniesienie roli Owner poza panelem wymaga innego uprawnionego, zweryfikowanego administratora i wymaganych dowodów. Obecna konfiguracja ma jedno zweryfikowane konto uprzywilejowane. Wstrzymaj odzyskiwanie, jeśli brakuje dowodów, sumy się różnią, kopia nie daje się powiązać z konfiguracją lub weryfikacja uprawnień kończy się błędem. Po autoryzowanym odtworzeniu unieważnij sesje i wyzwania, a przed przywróceniem dostępu sprawdź świeże logowanie hasłem i OTP.

Poza zakresem testu na jednym VPS-ie pozostają niezależna weryfikacja HTTPS i dostępu, rejestracja zewnętrznego użytkownika, rzeczywista utrata skrzynki lub przeniesienie roli Owner oraz rollback między różnymi zaakceptowanymi wydaniami. Do formalnej akceptacji potrzebne są te wyniki lub jawna decyzja właściciela o zmianie zakresu. [Zgłoszenie #10](https://github.com/adrianf87/dc24h.eu-twilight/issues/10).
