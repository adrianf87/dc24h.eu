# Bezpieczna integracja API WebAdmin

Status **zaimplementowano w roboczym PR #30 i zweryfikowano lokalnie**. Gałąź łączy bramę OTP Etapu IV z usługą kont i opcjonalnym połączeniem odczytowym do działającego huba. Zmiana gałęzi chronionej i wdrożenie wymagają osobnego przeglądu.

## Trasy i uprawnienia

| Operacja WebAdmin | Źródło | Zasady dostępu |
| --- | --- | --- |
| Logowanie, rejestracja, sesja, wylogowanie, zmiana własnego hasła, unieważnianie sesji | Brama Etapu IV oraz dowód hasła w C++ przez `POST /api/v1/admin/access` | Hasło i OTP przez e-mail. Operacje zapisu używają właściwego tokena CSRF. |
| `GET identity-summary` | Agregaty usługi kont z `GET /api/v1/stage4/identity-summary` | Owner lub Administrator. Odpowiedź zawiera liczniki bez nazw kont i adresów pocztowych. |
| `GET hub/health`, `hub/ready`, `hub/status`, `hub/users` | Działający hub i cztery dokładne mapowania mostu do `GET /api/v1/{health,ready,status,users}` | Owner lub Administrator. Gotowość, telemetria, użytkownicy online, efektywna konfiguracja, role, reguły moderacji i liczniki czatu, wyszukiwania, transferów, rozłączeń i wyciszeń. Lista pomija identyfikatory sesji i SID. |
| `POST admin` z `management` lub `admin` | Natywny katalog uprawnień i liczniki administracyjne | Owner lub Administrator z CSRF i aktualną sesją. |
| `POST admin` z `accounts.list`, `accounts.staff`, `accounts.detail` | Katalog kont oraz filtr personelu w C++ | `accounts.manage`. Paginacja kursorem lub odczyt według identyfikatora. |
| `POST admin` z `accounts.create`, `accounts.activate`, `accounts.disable`, `accounts.reset`, `accounts.role` | Natywny silnik kont i audyt transakcyjny | `accounts.manage`, przy rolach również `roles.manage`. Aktualne hasło, a przy zmianie istniejącego konta także oczekiwana rewizja. Natywne reguły chronią własne konto oraz równe i wyższe role. Tworzenie daje wyłączone konto bez rejestracji e-mail. |
| `POST admin` z `bans.list`, `bans.create`, `bans.revoke` | Natywny katalog banów i audyt transakcyjny | `users.ban` przy odczycie. Zapis wymaga także `blocks.manage` i aktualnego hasła. Ban może być bezterminowy, czasowy lub do określonej daty. Cofnięcie wymaga oczekiwanej rewizji. Hub egzekwuje wpis, gdy jego baza i aktywna migawka zawierają ten rekord. |
| `POST admin` z `audit.list` | Natywny, stronicowany audyt | `audit.read` i aktualne hasło. Odpowiedź zawiera identyfikatory, czasy, działania i wyniki. |

Trasy publiczne są względne wobec `/stage4/v1/`. Node sprawdza pola, CSRF i limity, wylicza skrót sesji z ciasteczka odczytanego po stronie serwera i ponownie sprawdza sesję po wywołaniu. C++ weryfikuje skrót, aktualną rewizję i zweryfikowane konto w MariaDB, po czym egzekwuje natywne uprawnienia i warunki transakcji. Przeglądarka przekazuje nazwaną operację. Ścieżkę wewnętrzną, konto wykonujące operację i dane uwierzytelniające usługi wyznacza serwer. Przy niepotwierdzonym wyniku zapisu odśwież rekord przed ponowną próbą.

## Odczyt danych działającego huba

Cztery odczyty używają `/run/dc24h-stage3-api/hub.sock` i `/var/lib/dc24h-stage3/api.token`. W zatwierdzonym wdrożeniu skonfiguruj istniejące `[api]` huba z tymi ścieżkami. Jednostka tworzy katalog runtime `dc24h-stage3-api` w trybie `0700`. Oddzielny token należy do UID huba i ma tryb `0600`. Most sprawdza właściciela i prawa oraz odrzuca nagłówki grantów przy odczycie. Brak poprawnych plików oznacza niedostępną telemetrię przy działającym panelu kont.

Binarne komponenty, brama, pliki React, most i konfiguracja Apache stanowią jedną wersję wdrożeniową. W zatwierdzonym oknie Etapu IV uruchom chroniony inicjalizator, aby nadać usłudze kont ograniczone uprawnienia tabel, potem sprawdź działanie przed udostępnieniem. Praca w gałęzi odbyła się bez zmian konfiguracji i działających usług VPS-a.

## Narzędzia prywatne i akceptacja

- Przegląd zapisanej konfiguracji, walidacja zmian, zapis i przywracanie pozostają w prywatnym interfejsie. Panel publiczny odczytuje efektywne ustawienia działającego huba.
- Status kontrolera usługi, start, stop, restart, inicjalizacja i przeniesienie Ownera, wydawanie grantów C++ i surowe trasy administracyjne pozostają prywatne. Panel używa sond dostępności samego huba.
- Akcje rozłączania i wyciszania działają przez autoryzowane polecenia klienta DC. Panel pokazuje reguły oraz liczniki. Kod huba nie udostępnia mutacji kick/mute przez HTTP.
- PR #30 zawiera testy C++/MariaDB i testy mostu, TypeScript oraz Chromium. Testy lokalne wykonano na Ubuntu 24.04 z odizolowanymi danymi. CI i decyzje o wydaniu pozostają osobnymi etapami.
