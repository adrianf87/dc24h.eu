# Fundament zarządzania — beta-0.1.1

beta-0.1.1 przygotowuje kontrakt administracyjny dla operacji konfiguracji i huba. Punktem wyjścia jest main `075081f`. Nazwa gałęzi to `beta-0.1.1`.

Hub, API i WebAdmin zgłaszają `beta-0.1.1`. Numer projektu CMake i numer pakietu WebAdmin pozostają `0.1.0`. Schemat v9 pozostaje bez zmian. To rozwojowa Beta. To nie jest wydanie produkcyjne. Żadne zagrożenie szczątkowe nie jest zamknięte.

`configuration.manage` i `hub.manage` pozostają niezaimplementowane. `permission_implemented` pozostaje fałszem dla obu. Ten etap nie odczytuje pliku konfiguracji, nie zapisuje pliku INI, nie przywraca kopii, nie uruchamia procesu, nie zatrzymuje procesu i nie wywołuje systemd. Późniejsza wersja wdraża te silniki.

## Katalog

`GET /api/v1/management` pozostaje odczytem odkrywania. Każdy żywy grant może go odczytać, gdy ten sam slot przedstawia też `X-DC24H-Session` i `X-DC24H-Csrf`. Ścieżka jest dokładna. Query string nie jest tą trasą. Odpowiedź nie zawiera identyfikatora aktora, grantu, nazwy konta, podmiotu bana, hasła ani tokenu.

Treść ma schemat 2. `maintenance` i `restart_required` są prawdziwe. `areas` ma pięć obiektów, w tej kolejności:

1. `accounts`, uprawnienie `accounts.manage`, implemented prawda.
2. `roles`, uprawnienie `roles.manage`, implemented prawda.
3. `bans`, uprawnienie `users.ban+blocks.manage`, implemented prawda.
4. `configuration`, uprawnienie `configuration.manage`, implemented fałsz.
5. `hub`, uprawnienie `hub.manage`, implemented fałsz.

`allowed` jest liczone na serwerze z aktora wczytanego dla tego grantu i z domyślnej tabeli ról. Skonfigurowana maska VIP nie jest stosowana. Właściciel widzi konfigurację i hub jako dozwolone i niezaimplementowane. Administrator widzi konfigurację jako dozwoloną, a hub jako niedozwolony. Operator widzi oba jako niedozwolone. Dwanaście szablonów tras grantu pozostaje bez zmian i nie wymienia tych operacji.

## Kontrakt autoryzacji

`POST /api/v1/management/authorize` jest jedyną nową trasą. Nie jest jednym z dwunastu szablonów. Proces konserwacji wymaga tokenu bearer, żywego grantu, `X-DC24H-Session` i `X-DC24H-Csrf` z tego samego slotu. Bramka przyjmuje każdy żywy grant, aby obsługa odróżniła brak uprawnienia od braku grantu. Potem sprawdza uprawnienie operacji. Brakujący, wygasły albo nieznany grant to `401` `admin_grant_required`. Brakujący albo niezgodny identyfikator sesji lub sekret CSRF to `403` `csrf_required`. `GET` na tej ścieżce to `405` z `Allow: POST`. `POST /api/v1/management` pozostaje `405` z `Allow: GET`.

Formularz ma dokładnie jedno pole, `operation`. Każde inne pole, w tym `path`, `command`, `service` i `file`, to `400` `invalid_input`. Wartość musi być jedną z tych nazw:

- `configuration.read`, `configuration.validate`, `configuration.write` i `configuration.restore` wymagają `configuration.manage`.
- `hub.status`, `hub.start`, `hub.stop` i `hub.restart` wymagają `hub.manage`.

Nieznana nazwa to `400` `invalid_input`. Znana nazwa bez uprawnienia to `403` `management_refused`. Znana nazwa z uprawnieniem to `501` `not_implemented`. Odpowiedź nie powtarza grantu, hasła ani polecenia. Obsługa nie otwiera pliku i nie uruchamia procesu.

Status i health zostają tylko z bearer. `Require local` zostaje. Kick i mute w przeglądarce pozostają niewysyłane.

## WebAdmin i proxy

Strona Zarządzanie wymienia pięć obszarów. Konfiguracja i hub pokazują, że nie są zaimplementowane. Strona nie ma formularza konfiguracji, pola hasła dla tych obszarów ani przycisku Start, Stop lub Restart. Odnośniki kont, ról i banów zostają przy obszarach zaimplementowanych. Katalog, który oznacza konfigurację albo hub jako zaimplementowane, jest odrzucany, a strona pozostaje niedostępna.

Apache przekazuje `POST /admin-api/v1/management/authorize` i nadal odmawia `POST /admin-api/v1/management`, dłuższego sufiksu oraz `GET` na ścieżce autoryzacji. Adapter pętli zwrotnej używa tej samej dokładnej ścieżki. Sekrety klienta pozostają usunięte. Zobacz [Zarządzanie WebAdmin](WEBADMIN_MANAGEMENT.md) i [API administracyjne](ADMIN_API.md).

## Poza tą wersją

Odczyt konfiguracji, rozróżnienie zapisanej i aktywnej, walidacja proponowanego pliku, wykrywanie konfliktów, atomowy zapis INI, kopia, przywracanie, kontrola uprawnień pliku, wiersze audytu tych operacji, schemat v10, edytor WebAdmin, niezależny kontroler usługi oraz wykonanie Start, Stop i Restart należą do późniejszych wersji.
