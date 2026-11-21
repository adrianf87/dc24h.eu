# Operacje systemd tylko dla Owner — beta-0.1.6

Etap rozwojowy rozszerza niezależny kontroler stanu z beta-0.1.5 o **Start, Stop i Restart** jednej stałej jednostki systemd: `dc24h.service`. Proces huba ADC/ADCS nie wykonuje tych poleceń i może pozostawać wyłączony. Nie dodajemy przycisków przeglądarkowych, publicznego proxy, dowolnych nazw jednostek ani powłoki zdalnej. Interfejs WebAdmin zaplanowano na beta-0.1.7.

## Granice zaufania i API

Odrębny proces obsługowy `dc24h-account-admin` przyjmuje dokładnie:

| Metoda i ścieżka | Wymagane dane formularza | Oczekiwany sprawdzony stan |
| --- | --- | --- |
| `POST /api/v1/management/hub/start` | `confirm=START` | `active` |
| `POST /api/v1/management/hub/stop` | `confirm=STOP` | `inactive` |
| `POST /api/v1/management/hub/restart` | `confirm=RESTART` | `active` |

Wymagany jest istniejący token procesu, aktualny grant obsługowy, powiązany identyfikator sesji i tajny CSRF. `AccessGate` sprawdza aktualną rolę konta w bazie dla każdego żądania i wpuszcza **wyłącznie Owner**. Identyfikator wykonawcy pochodzi ze sprawdzonego grantu, nigdy z formularza. Dodatkowe pola, parametry zapytania, inne metody HTTP, nieznane operacje oraz obce jednostki są odrzucane. Tych tras nie dodano do reguł proxy Apache2 ani adaptera Node WebAdmin.

API obsługowe musi działać jako osobny, nieuprzywilejowany użytkownik `dc24h-maintenance`, inny niż użytkownik huba. Za pomocą jądrowego `SO_PEERCRED` sprawdza UID serwera gniazda `dc24h-control`. Kontroler sam wymaga dokładnego **UID** użytkownika `dc24h-maintenance` dla operacji sterujących; sama grupa `dc24h-admin` pozwala jedynie odczytać `STATUS`. Polecenia `START`, `STOP`, `RESTART` są ściśle ograniczone. Kontroler wywołuje `/usr/bin/systemctl` z ustalonymi parametrami `--system --no-pager --no-ask-password` i nazwą `dc24h.service`, bez powłoki. Status działa bez huba, MariaDB i API obsługowego. Do wykonania akcji API potrzebuje bazy danych, aby potwierdzić rolę Owner i zapisać audyt.

## Rezultaty, audyt i błędy

- Migracja MariaDB **v11** rozszerza ograniczenia tabeli `dc24h_audit_events`. Każde zatwierdzone żądanie najpierw trwale zapisuje wpis `service`/`1`, akcję `hub.start`, `hub.stop` lub `hub.restart`, sprawdzone ID Owner i stan `started`. Dopiero potem kontaktuje się z kontrolerem.
- Po prawidłowym zakończeniu procesu systemctl kontroler dodatkowo sprawdza stan jednostki. Potwierdzony sukces otrzymuje `succeeded`, definitywna odmowa `failed`. Konflikt (usługa już w danym stanie lub zajęta) zwraca HTTP `409`, brak dostępu lub odmowa `503`. Sukces zwraca HTTP `200` z operacją, `outcome:succeeded` i sprawdzonym `active_state`.
- Brak konta procesu obsługowego, brak gniazda kontrolera lub niepoprawna tożsamość jego procesu **przed wysłaniem polecenia** oznacza pewne `503 service_unavailable`; wpis audytu otrzymuje `failed`. Zerwane połączenie **po wysłaniu polecenia**, timeout, niepewny wynik systemd albo brak potwierdzenia końcowego wpisu audytu daje **`503 outcome_unknown`**. Wpis `started` pozostaje do ręcznego wyjaśnienia: operacja mogła już nastąpić. Przed kolejnym działaniem sprawdź `systemctl show dc24h.service` i audyt; **nie ponawiaj automatycznie**. Odmowy autoryzacji przed wpisem audytu nie tworzą zdarzenia usługi.
- Polecenia systemowe mają stały program, parametry i ograniczony czas. Osoba bez roli Owner jest odrzucana przed wywołaniem kontrolera. Stan `active` systemd nie dowodzi jeszcze poprawnego działania aplikacji.

## Wdrożenie po ręcznym przeglądzie

1. Przejrzyj [przykład sysusers](../../config/dc24h-control.sysusers.example), [jednostkę kontrolera](../../config/dc24h-control.service.example) i [przykładową regułę Polkit](../../config/49-dc24h-control.rules.example). Utwórz odrębne konta systemowe `dc24h-control` i `dc24h-maintenance`, ogranicz grupę i dostęp do `/run/dc24h-control/status.sock`. Proces `dc24h-account-admin` należy uruchomić jako `dc24h-maintenance` z prywatnym INI, tokenem API i danymi MariaDB; przykłady nie uruchamiają go automatycznie.
2. Jeśli chcesz włączyć sterowanie usługą, po przeglądzie zainstaluj regułę w `/etc/polkit-1/rules.d` zgodnie z polityką hosta. Daje wyłącznie `org.freedesktop.systemd1.manage-units`, tylko jednostce `dc24h.service` i czasownikom `start`, `stop`, `restart`, dla `dc24h-control`. Nie przydzielaj kontrolerowi szerokiego sudo, roota ani zarządzania innymi usługami.
3. Przed startem nowego API wykonaj migrację v11 istniejącą procedurą. W środowisku testowym sprawdź żądania Owner, Administratora i bez uwierzytelnienia, brak systemd, stan jednostki i historię audytu. Budowanie ani instalacja projektu nie włączają usług, nie dodają użytkowników i nie instalują reguły Polkit.

Widok uprawnień `hub.manage` pozostaje oznaczony jako *nie w pełni wdrożony* do etapu WebAdmin beta-0.1.7; **trzy konkretne lokalne trasy POST dostępne tylko dla Owner** działają niezależnie od tego widoku. Dawna trasa odkrywania `POST /management/authorize` nadal zwraca `501 not_implemented` i nie wykonuje akcji. Protokół ADC/ADCS pozostaje bez zmian.
