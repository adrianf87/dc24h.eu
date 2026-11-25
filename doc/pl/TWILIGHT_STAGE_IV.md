# dc24h.eu-twilight — Etap IV: inwentaryzacja tożsamości technicznej i decyzja OWNER

Stan: **OWNER zatwierdził wariant nazw technicznych 2026-11-25; zakres kodowy PR #6 scalono do `main`**. Osobna gałąź `rebrand/stage-iv-migration-rollback-proof` dodaje dowód kontrolowanej migracji **nazw testowych pakietów i jednostek**, do weryfikacji w CI i osobnej decyzji OWNER. **Nie wykonano migracji ani aktywacji produkcyjnej.** Konta usługowe, prywatny stan, ścieżki danych, API/DB, DNS/TLS oraz WebAuthn pozostają bez zmian.

## Mapa dawnych nazw → zatwierdzonych nazw docelowych

| Obszar | Stan obecny | Propozycja / decyzja |
| --- | --- | --- |
| Nazwa produktu w interfejsie oraz `server.name` API v1 | `dc24h.eu-twilight` | **Bez zmian** (zrealizowane w Etapach II–III). |
| Wewnętrzny projekt/target CMake | `project(dc24h)`; `dc24h`, `dc24h-owner`, `dc24h-account-admin`, `dc24h-service-controller` | Zachować wewnętrzne targety; zatwierdzone nazwy wyjściowe: `dc24h-twilight`, `dc24h-twilight-owner`, `dc24h-twilight-account-admin`, `dc24h-twilight-service-controller`. Wywołania i testy używają nowych ścieżek. |
| Pakiet WebAdmin w npm | `dc24h-webadmin` w `webadmin/package.json` i w dwóch miejscach lockfile | Zatwierdzone `dc24h-twilight-webadmin` w **obu** plikach, bez aktualizacji zależności. |
| Główna jednostka systemd | `dc24h.service`; przykład `config/dc24h.service.example` | Proponowane `dc24h-twilight.service` tylko po zdefiniowaniu procedury upgrade, wykluczenia równoległego startu i rollbacku. |
| Kontroler jednostek | `dc24h-control.service`; `config/dc24h-control.service.example`; binarka `dc24h-service-controller` | Proponowane `dc24h-twilight-control.service` i nowa nazwa binarki; sprawdzić zamkniętą listę komend Polkit i socket. |
| Zdalna brama Node | `dc24h-remote-gateway.service`; `config/dc24h-remote-gateway.service.example` | Proponowane `dc24h-twilight-remote-gateway.service` po testach izolacji, Origin, `rpID` i restartów. |
| Użytkownicy i grupy | `dc24h`, `dc24h-control`, `dc24h-maintenance`, `dc24h-admin`; przykłady `config/*.sysusers.example` | **Zachować** istniejące nazwy i tożsamości uprawnień (UID/GID), jeśli nie zostanie zatwierdzona odrębna migracja kont; nazwa usługi nie musi oznaczać nowego konta. |
| Katalogi i pliki instalacji | `/etc/dc24h`, `/var/lib/dc24h`, `/run/dc24h`, `/var/log/dc24h`, `/opt/dc24h/webadmin`, `/usr/local/share/dc24h/webadmin` | **Zachować** istniejące ścieżki i pliki, dopóki nie ma osobnej migracji stanu z backupem/rollbackiem. Żadnych aliasów wskazujących na dwa aktywne źródła zapisu. |
| Klucze API, konfiguracja, schemat DB i namespace | `DC24H_*`, `dc24h`, `include/dc24h`, `namespace dc24h`, tabela/schemat `dc24h`, ADC/ADCS | **Zachować** jako identyfikatory protokołu, danych i kodu; rebranding nie jest migracją schematu. |
| CI-only pakiet `dpkg` | `dc24h-high-starter-ci-fixture` i inertna jednostka w `scripts/beta07_ci_deb.py` | Proponowane `dc24h-twilight-ci-fixture`, ale **nie** utożsamiać zmiany nazwy pakietu z testem aktualizacji starego pakietu: `dpkg` traktuje je jako odrębne pakiety. Wymaga osobnego testu migracji/nazw pakietów, konfliktu i downgrade. |
| Publiczne nazwy sieciowe i uwierzytelnianie | `dc24h.eu` jako przykładowy host/Origin, WebAuthn `rpID`, TLS i certyfikaty | **Bez zmian**; wymagają odrębnej decyzji infrastrukturalnej. |

## Zatwierdzony wariant P1 — implementacja na gałęzi

1. Przyjąć `dc24h-twilight` jako techniczny prefiks **nowych nazw plików wykonywalnych i usług**, bez zmiany stabilnych identyfikatorów danych, API, UID/GID, katalogów i domen.
2. Zmienić nazwę npm na `dc24h-twilight-webadmin`, aktualizując równocześnie manifest i lockfile, bez bump zależności.
3. Zachować wewnętrzne targety CMake, aby nie zmieniać linkowania i nazw bibliotek; nazwy artefaktów wyjściowych określić jawnie i dopasować wszystkie skrypty, przykłady, instalacje i testy.
4. Zapewnić **fail-closed** przy wykryciu starej jednostki lub starego procesu: nigdy nie uruchamiać obu hubów na tym samym stanie. Również kontroler Polkit nie może nabyć szerszych uprawnień przez zmianę nazwy jednostki.
5. Przygotować izolowane testy czystej instalacji, starej→nowej konfiguracji, wymuszonej awarii, rollbacku binarek i uprawnień; dowody muszą odnosić się do finalnego SHA na GitHub-hosted Ubuntu 22.04/24.04/26.04.

## Granice wykonania

- **Decyzja OWNER:** `dc24h-twilight` dla wyjściowych binarek i jednostek, `dc24h-twilight-webadmin` dla npm. Zmiana odbywa się wyłącznie w kodzie/fixture i przykładowych jednostkach; merge/instalacja/aktywacja wymagają osobnych zgód.
- Nowy przykład `dc24h-twilight.service` deklaruje `Conflicts=dc24h.service` i `AssertPathExists=/etc/dc24h/twilight-unit-migration-approved`. Operator musi **najpierw** zatrzymać stary hub, sprawdzić migrację i świadomie zapewnić znacznik; build/CI go nie tworzy. Kontroler i zdalna brama deklarują konflikt z odpowiadającymi dawnymi jednostkami.
- Zgoda na rozpoczęcie Etapu IV **nie** uprawnia do nadpisania realnej instalacji, przenoszenia `/var/lib/dc24h`, zmiany właścicieli/UID/GID, certyfikatów TLS, sekretów, `rpID`, Origin, schematu MariaDB ani publicznego listenera.
- `scripts/beta07_ci_deb.py` wytwarza wyłącznie inertny pakiet testowy; przejście na inny `Package:` w `.deb` nie jest automatycznym `dpkg` upgrade. Przed przyjęciem migracji należy udowodnić, że stara i nowa jednostka **nie** mogą działać równolegle.
- Historyczne wersje i `v0.9.20-rc.1` pozostają niezmienione. Każdy nowy kontrakt wymaga testów negatywnych, CI na końcowym SHA i osobnego odbioru OWNER przed merge.

## Osobny test migracji nazw — tylko GitHub-hosted CI

- `scripts/beta07_ci_deb.py` buduje **dwa odrębne** testowe pakiety `dc24h-high-starter-ci-fixture` i `dc24h-twilight-ci-fixture`, z symetrycznym `Conflicts`, bez `Replaces`, skryptów instalacyjnych i automatycznego uruchamiania. Stary pakiet zawiera rzeczywistą, przypiętą binarkę historyczną; nowy — binarkę z testowanego HEAD.
- `tests/twilight_stage4_migration_process_tests.py` sprawdza: czystą instalację starej nazwy, odmowę instalacji nowej bez jawnego usunięcia starej, awarię `preinst` z odtworzeniem poprzedniej nazwy, jawne przejście na nową nazwę, odmowę niejawnego cofnięcia oraz rzeczywisty powrót do starej nazwy. Porównuje SHA-256 obu binarek i narzędzi kont, własność plików `dpkg`, brak aktywnych i włączonych usług.
- Wyłącznie na jednorazowych runnerach GitHub-hosted Ubuntu 22.04/24.04/26.04 test tworzy **unikalne, syntetyczne** jednostki `sleep` bez listenerów: wymaga znacznika zgody, sprawdza konflikt starej i nowej jednostki, zatrzymanie starej przed aktywacją nowej oraz rollback do starej. Nigdy nie startuje realnego `dc24h.service` ani `dc24h-twilight.service` i nie tworzy znacznika w `/etc/dc24h`.
- `scripts/twilight_stage4_migration_gate.py` odrzuca niepełne dowody, błędny SHA/Ubuntu, brak odmowy w negatywnych scenariuszach i wszelką deklarację produkcyjnej gotowości. Raporty `twilight-stage4-name-migration.json` i `twilight-stage4-migration-gate.json` podlegają zwykłemu uploadowi dowodów VM.
- **Granica akceptacji:** pozytywny test w CI oznacza tylko przejście syntetycznych nazw pakietów i jednostek. **Nie jest** testem upgrade istniejącej instalacji, rzeczywistych danych, zamiany usług pod obciążeniem ani instalatora produkcyjnego. Te obszary wymagają dalszego planu operatora i osobnej zgody przed wdrożeniem.

## Wykonanie i odbiór

1. Wprowadzić tylko zatwierdzone zmiany tożsamości technicznej w CMake/npm/systemd i w przykładowych ścieżkach **bez** modyfikowania produkcyjnego stanu.
2. Dodać testy konfliktu jednostek, odrzucenia niekompletnej migracji oraz rzeczywistego rollbacku w izolowanej maszynie; zachować historyczną wersję binarki i kontrolę autentycznego pakietu.
3. Uruchomić komplet wymaganych testów, sprawdzić dowody, przedstawić PR do osobnej decyzji OWNER. Dopiero po akceptacji można rozważyć scalenie — nie wdrożenie.
