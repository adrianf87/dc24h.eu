# dc24h.eu-twilight — Etap IV: inwentaryzacja tożsamości technicznej i decyzja OWNER

Stan: **propozycja do osobnej decyzji o mapie nazw; bez migracji i aktywacji**. OWNER zatwierdził rozpoczęcie Etapu IV 2026-11-25. Dokument opisuje rzeczywiste nazwy z `main` po Etapie III i proponowany wariant, ale **nie jest zgodą na zmianę użytkowników, katalogów ani systemowych usług**.

## Mapa obecnych nazw → proponowanych nazw docelowych

| Obszar | Stan obecny | Propozycja / decyzja |
| --- | --- | --- |
| Nazwa produktu w interfejsie oraz `server.name` API v1 | `dc24h.eu-twilight` | **Bez zmian** (zrealizowane w Etapach II–III). |
| Wewnętrzny projekt/target CMake | `project(dc24h)`; `dc24h`, `dc24h-owner`, `dc24h-account-admin`, `dc24h-service-controller` | Zachować wewnętrzne targety, a **po decyzji OWNER** rozważyć wyjściowe nazwy plików `dc24h-twilight`, `dc24h-twilight-owner`, `dc24h-twilight-account-admin`, `dc24h-twilight-service-controller`. Wszystkie wywołania i testy muszą wtedy użyć nowych ścieżek. |
| Pakiet WebAdmin w npm | `dc24h-webadmin` w `webadmin/package.json` i w dwóch miejscach lockfile | Proponowane `dc24h-twilight-webadmin` w **obu** plikach, bez aktualizacji zależności. |
| Główna jednostka systemd | `dc24h.service`; przykład `config/dc24h.service.example` | Proponowane `dc24h-twilight.service` tylko po zdefiniowaniu procedury upgrade, wykluczenia równoległego startu i rollbacku. |
| Kontroler jednostek | `dc24h-control.service`; `config/dc24h-control.service.example`; binarka `dc24h-service-controller` | Proponowane `dc24h-twilight-control.service` i nowa nazwa binarki; sprawdzić zamkniętą listę komend Polkit i socket. |
| Zdalna brama Node | `dc24h-remote-gateway.service`; `config/dc24h-remote-gateway.service.example` | Proponowane `dc24h-twilight-remote-gateway.service` po testach izolacji, Origin, `rpID` i restartów. |
| Użytkownicy i grupy | `dc24h`, `dc24h-control`, `dc24h-maintenance`, `dc24h-admin`; przykłady `config/*.sysusers.example` | **Zachować** istniejące nazwy i tożsamości uprawnień (UID/GID), jeśli nie zostanie zatwierdzona odrębna migracja kont; nazwa usługi nie musi oznaczać nowego konta. |
| Katalogi i pliki instalacji | `/etc/dc24h`, `/var/lib/dc24h`, `/run/dc24h`, `/var/log/dc24h`, `/opt/dc24h/webadmin`, `/usr/local/share/dc24h/webadmin` | **Zachować** istniejące ścieżki i pliki, dopóki nie ma osobnej migracji stanu z backupem/rollbackiem. Żadnych aliasów wskazujących na dwa aktywne źródła zapisu. |
| Klucze API, konfiguracja, schemat DB i namespace | `DC24H_*`, `dc24h`, `include/dc24h`, `namespace dc24h`, tabela/schemat `dc24h`, ADC/ADCS | **Zachować** jako identyfikatory protokołu, danych i kodu; rebranding nie jest migracją schematu. |
| CI-only pakiet `dpkg` | `dc24h-high-starter-ci-fixture` i inertna jednostka w `scripts/beta07_ci_deb.py` | Proponowane `dc24h-twilight-ci-fixture`, ale **nie** utożsamiać zmiany nazwy pakietu z testem aktualizacji starego pakietu: `dpkg` traktuje je jako odrębne pakiety. Wymaga osobnego testu migracji/nazw pakietów, konfliktu i downgrade. |
| Publiczne nazwy sieciowe i uwierzytelnianie | `dc24h.eu` jako przykładowy host/Origin, WebAuthn `rpID`, TLS i certyfikaty | **Bez zmian**; wymagają odrębnej decyzji infrastrukturalnej. |

## Proponowany wariant P1 do zatwierdzenia

1. Przyjąć `dc24h-twilight` jako techniczny prefiks **nowych nazw plików wykonywalnych i usług**, bez zmiany stabilnych identyfikatorów danych, API, UID/GID, katalogów i domen.
2. Zmienić nazwę npm na `dc24h-twilight-webadmin`, aktualizując równocześnie manifest i lockfile, bez bump zależności.
3. Zachować wewnętrzne targety CMake, aby nie zmieniać linkowania i nazw bibliotek; nazwy artefaktów wyjściowych określić jawnie i dopasować wszystkie skrypty, przykłady, instalacje i testy.
4. Zapewnić **fail-closed** przy wykryciu starej jednostki lub starego procesu: nigdy nie uruchamiać obu hubów na tym samym stanie. Również kontroler Polkit nie może nabyć szerszych uprawnień przez zmianę nazwy jednostki.
5. Przygotować izolowane testy czystej instalacji, starej→nowej konfiguracji, wymuszonej awarii, rollbacku binarek i uprawnień; dowody muszą odnosić się do finalnego SHA na GitHub-hosted Ubuntu 22.04/24.04/26.04.

## Granice wykonania

- **Do decyzji OWNER:** czy wdrażać zaproponowany prefiks dla binarek i usług, czy zachować ich istniejące nazwy dla zgodności; czy aktualizować nazwę npm.
- Zgoda na rozpoczęcie Etapu IV **nie** uprawnia do nadpisania realnej instalacji, przenoszenia `/var/lib/dc24h`, zmiany właścicieli/UID/GID, certyfikatów TLS, sekretów, `rpID`, Origin, schematu MariaDB ani publicznego listenera.
- `scripts/beta07_ci_deb.py` wytwarza wyłącznie inertny pakiet testowy; przejście na inny `Package:` w `.deb` nie jest automatycznym `dpkg` upgrade. Przed przyjęciem migracji należy udowodnić, że stara i nowa jednostka **nie** mogą działać równolegle.
- Historyczne wersje i `v0.9.20-rc.1` pozostają niezmienione. Każdy nowy kontrakt wymaga testów negatywnych, CI na końcowym SHA i osobnego odbioru OWNER przed merge.

## Wykonanie po decyzji

1. Wprowadzić tylko zatwierdzone zmiany tożsamości technicznej w CMake/npm/systemd i w przykładowych ścieżkach **bez** modyfikowania produkcyjnego stanu.
2. Dodać testy konfliktu jednostek, odrzucenia niekompletnej migracji oraz rzeczywistego rollbacku w izolowanej maszynie; zachować historyczną wersję binarki i kontrolę autentycznego pakietu.
3. Uruchomić komplet wymaganych testów, sprawdzić dowody, przedstawić PR do osobnej decyzji OWNER. Dopiero po akceptacji można rozważyć scalenie — nie wdrożenie.
