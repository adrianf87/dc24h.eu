# Przegląd konfiguracji — beta-0.1.2

Gałąź `chatgpt-beta-0.1.2` dodaje odczyt i walidację wyłącznie w pamięci do **osobnego lokalnego procesu administracyjnego** (`dc24h-account-admin --config ...`). Nie uruchamia drugiego huba, nie udostępnia edytora WWW, nie zmienia usługi ADC i nie zapisuje plików. Operacje wymagają uprawnienia `configuration.manage` (Administrator lub Owner), tokenu procesu, aktywnego grantu administratora, powiązanej sesji oraz sekretu CSRF.

## Dokładne lokalne trasy

`GET /api/v1/management/configuration` zwraca schemat 1, `read_only: true`, `restart_required: true`, niejawny identyfikator `revision`, dozwolone wartości `saved` (ponownie odczytane z INI przy każdym żądaniu) oraz `active` (wartości startowe procesu administracyjnego). `matches_active` porównuje cztery pola. `active_scope` ma wartość `maintenance_process`, a `hub_active_available` wynosi **false**: konfiguracji uruchomionego huba nie można wywnioskować z pliku na dysku. Odpowiedź nie jest więc stanem aktywnego huba.

Zwracane i walidowane są wyłącznie niesekretne pola:

| Pole formularza/JSON | Pole INI | Dozwolone wartości |
|---|---|---|
| `network_max_connections` | `[network] max_connections` | Liczba całkowita 1–65535 |
| `network_idle_timeout` | `[network] idle_timeout` | Liczba całkowita 1–86400 sekund |
| `logging_level` | `[logging] level` | `info`, `warning`, `error`, `off` |
| `general_language` | `[general] language` | `us`, `pl` |

`POST /api/v1/management/configuration` przyjmuje formularz `application/x-www-form-urlencoded` z `revision=<64 małe znaki hex>` i od jednego do czterech różnych, dozwolonych pól. Ponownie odczytuje zapisany plik, porównuje rewizję, stosuje propozycje do **tymczasowego obiektu w pamięci** i uruchamia dotychczasową walidację całej konfiguracji. Dla poprawnej propozycji zwraca `{ "valid": true, "written": false, "restart_required": true }`. Błędna wartość daje `valid: false` bez ujawniania zawartości INI ani szczegółów parsera. Nic nie zapisuje. Nie wolno podawać pola ścieżki ani nazwy pliku.

Rewizja to HMAC-SHA256 całej zawartości pliku z losowym kluczem istniejącym tylko w pamięci procesu. Nie jest tokenem uwierzytelnienia i zmienia się po restarcie procesu administracyjnego. Jeśli plik zmienił się od odczytu, POST zwraca `409` i `stale`; trzeba odczytać aktualny stan. Duplikaty i nieznane pola, błędna rewizja oraz za duże żądania są odrzucane. Błędy odczytu, dowiązania symboliczne i błędnie zapisany INI skutkują bezpieczną odmową z ogólnym komunikatem. Parser nadal zabrania dołączania innych plików i rozwijania zmiennych środowiska; limit wynosi 64 KiB.

Ta trasa **nie** jest udostępniona przez Apache ani adapter WebAdmin w tym etapie. Działa tylko przez istniejący chroniony lokalny socket Unix. `GET /api/v1/status` pozostaje osobną trasą tylko z tokenem bearer. Dotychczasowy kontrakt `POST /api/v1/management/authorize` nadal zwraca `501` dla nazw operacji, a katalog pięciu obszarów nadal oznacza pełne zarządzanie konfiguracją jako niezaimplementowane. Nie ma zapisu, kopii, cofania zmian, formularza WebAdmin ani sterowania systemd; to zakres późniejszych etapów beta.

Nie dodano migracji SQL ani wpisu audytu; schemat v9 pozostaje. Testy obejmują podgląd, konflikt po zmianie pliku, błędne wartości, uszkodzony plik i odrzucanie dowiązań symbolicznych. To beta rozwojowa, a nie produkcyjny edytor konfiguracji.
