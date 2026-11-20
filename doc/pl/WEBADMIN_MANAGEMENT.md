# Zarządzanie WebAdmin

alpha-0.8.13 dodaje katalog utrzymania dla obszarów kont, ról i banów oraz dwujęzyczną stronę WebAdmin, która prowadzi do istniejących przepływów. Gałąź powstaje z main.

`GET /api/v1/management` obsługuje proces konserwacji. Każdy żywy `X-DC24H-Admin-Grant` może go odczytać, według tej samej reguły odkrywania co `GET /api/v1/admin`. Ścieżka jest dokładna. Proces huba nie publikuje tej trasy. Schemat v9 pozostaje bez zmian. Odpowiedź nie zawiera identyfikatora aktora, grantu, nazw kont ani podmiotów banów.

Treść ma schemat 1. `maintenance` i `restart_required` są prawdziwe. `areas` ma trzy obiekty, w kolejności: `accounts` z uprawnieniem `accounts.manage`, `roles` z uprawnieniem `roles.manage` oraz `bans` z uprawnieniem `users.ban+blocks.manage`. Każdy obiekt ma logiczne `allowed`.

`allowed` jest liczone na serwerze z aktora wczytanego dla tego grantu i z domyślnej tabeli ról. Skonfigurowana maska VIP nie jest stosowana. Operator może otwierać bany i nie może otwierać zapisów kont ani ról. Administrator może otworzyć wszystkie trzy. Moderator może odczytać katalog i widzi wszystkie trzy jako niedozwolone. Dwanaście szablonów tras grantu pozostaje bez zmian. `configuration.manage` i `hub.manage` pozostają niewdrożone.

Strona Zarządzanie pokazuje te trzy obszary i prowadzi do Kont, katalogu ról tylko do odczytu oraz Banów. Nie wysyła tych formularzy. Zapisy kont, ról i banów zostają na tych stronach i nadal wymagają zatrzymanego huba, świeżego dowodu hasła i bieżącej rewizji tam, gdzie dana strona już tego wymaga. Zmiana hasła zostaje na stronie kont i nie jest jednym z tych trzech obszarów. Bootstrap i przekazanie właściciela zostają w lokalnym narzędziu właściciela. Zapis konfiguracji, bieżąca administracja huba oraz kick, mute i unmute w przeglądarce pozostają poza tą stroną. `+kick`, `+mute` i `+unmute` nadal działają z uwierzytelnionego klienta Direct Connect.

Brak grantu, API huba albo zniekształcony katalog pokazuje stronę jako niedostępną, a nie jako pustą listę zezwoleń. Grant pozostaje w pamięci strony i nie jest zapisywany w pamięci przeglądarki.

Check status tej gałęzi używa runnerów GitHub-hosted Ubuntu 24.04. Każde zadanie sprawdza cztery logiczne CPU i używa workerów `nproc`. Nie czekamy na zakończenie zdalnego CI.

Ten etap nie obejmuje wdrożenia produkcyjnego, migracji schematu ani zmiany kont.
