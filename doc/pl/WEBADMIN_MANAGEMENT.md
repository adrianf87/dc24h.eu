# Zarządzanie WebAdmin

alpha-0.8.13 dodaje katalog utrzymania dla obszarów kont, ról i banów oraz dwujęzyczną stronę WebAdmin, która prowadzi do istniejących przepływów. Gałąź powstaje z main.

`GET /api/v1/management` obsługuje proces konserwacji. Każdy żywy `X-DC24H-Admin-Grant` może go odczytać, według tej samej reguły odkrywania co `GET /api/v1/admin`. Ścieżka jest dokładna. Proces huba nie publikuje tej trasy. Schemat v9 pozostaje bez zmian. Odpowiedź nie zawiera identyfikatora aktora, grantu, nazw kont ani podmiotów banów.

alpha-0.8.13 używało schematu 1 z trzema obszarami: `accounts`, `roles` i `bans`. beta-0.1.1 używa schematu 2. `maintenance` i `restart_required` są prawdziwe. `areas` ma pięć obiektów, w kolejności: `accounts` z `accounts.manage`, `roles` z `roles.manage`, `bans` z `users.ban+blocks.manage`, `configuration` z `configuration.manage` oraz `hub` z `hub.manage`. Każdy obiekt ma logiczne `allowed` i logiczne `implemented`. Pierwsze trzy są zaimplementowane. Konfiguracja i hub nie są.

`allowed` jest liczone na serwerze z aktora wczytanego dla tego grantu i z domyślnej tabeli ról. Skonfigurowana maska VIP nie jest stosowana. Operator może otwierać bany i nie może otwierać operacji kont, ról, konfiguracji ani huba. Administrator może otworzyć pierwsze trzy i konfigurację, a nie może otworzyć huba. Właściciel może otworzyć konfigurację i hub, a oba pozostają niezaimplementowane. Moderator może odczytać katalog i widzi obszary zapisu jako niedozwolone. Dwanaście szablonów tras grantu pozostaje bez zmian. Zobacz [Fundament zarządzania](MANAGEMENT_FOUNDATION.md).

Strona Zarządzanie pokazuje pięć obszarów. Prowadzi do Kont, katalogu ról tylko do odczytu oraz Banów. Nie wysyła tych formularzy. Konfiguracja i hub pokazują, że nie są zaimplementowane. Strona nie ma formularza konfiguracji ani przycisku Start, Stop lub Restart. Zapisy kont, ról i banów zostają na tych stronach i nadal wymagają zatrzymanego huba, świeżego dowodu hasła i bieżącej rewizji tam, gdzie dana strona już tego wymaga. Zmiana hasła zostaje na stronie kont. Bootstrap i przekazanie właściciela zostają w lokalnym narzędziu właściciela. Kick, mute i unmute w przeglądarce pozostają poza tą stroną. `+kick`, `+mute` i `+unmute` nadal działają z uwierzytelnionego klienta Direct Connect.

Brak grantu, API huba albo zniekształcony katalog pokazuje stronę jako niedostępną, a nie jako pustą listę zezwoleń. Grant pozostaje w pamięci strony i nie jest zapisywany w pamięci przeglądarki.

Check status tej gałęzi używa runnerów GitHub-hosted Ubuntu 24.04. Każde zadanie sprawdza cztery logiczne CPU i używa workerów `nproc`. Nie czekamy na zakończenie zdalnego CI.

Ten etap nie obejmuje wdrożenia produkcyjnego, migracji schematu ani zmiany kont.
