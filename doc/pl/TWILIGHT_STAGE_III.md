# dc24h.eu-twilight — Etap III: nazwa w komunikatach operatora (do przeglądu)

Ten etap zmienia *teksty wyświetlane*, a nie tożsamość techniczną, zaufanie sieciowe ani nazwy zainstalowanych usług.

## Zmiany proponowane w Etapie III

- `dc24h --version` pokazuje teraz `dc24h.eu-twilight beta-0.9.20`; plik wykonywalny nadal nazywa się `dc24h`.
- Log startowy, pomoc CLI w PL/EN, domyślne powitanie ADC oraz przykładowy plik INI pokazują nazwę Twilight. Powitanie skonfigurowane wcześniej przez operatora w `welcome_message` nie jest przepisywane.
- Opisy przykładowych jednostek systemd i komentarze Apache/INI używają nowej marki. Nazwy jednostek, ścieżki binarek, `ExecStart`, pakiety i działające usługi pozostają bez zmian.
- Testy procesu, logów, powitania ADC oraz izolowanej instalacji/rollbacku dostosowano do nowego tekstu. Historyczne binarki zachowują *oryginalną* nazwę wersji; pole dowodowe dla bieżącego kandydata nadal zapisuje wyłącznie `beta-0.9.20`.

## Bez zmian do osobnej decyzji OWNER

| Identyfikator | Obecna wartość / działanie | Uzasadnienie |
| --- | --- | --- |
| C++ `server.name` w status API v1 | `dc24h.eu` | Ścisły kontrakt danych, a nie etykieta UI |
| Parsery i fixture statusu w WebAdmin | Wymagają `dc24h.eu` | Nadal odrzucają obce i nieprawidłowe odpowiedzi |
| Publiczne/Apache `ServerName`, przekierowania, kontrole Host i Origin | Dotychczasowe przykłady hosta | DNS, TLS i zaufane Origin to decyzje infrastrukturalne |
| WebAuthn `rpID`, skonfigurowany Origin, poświadczenia | Bez zmian | Rebranding nie może naruszyć weryfikacji kluczy |
| Binarki, pakiet npm, jednostki systemd, konfiguracja i ścieżki stanu | Dotychczasowe identyfikatory `dc24h*` | Migracja nazw i rollback należą do Etapu IV |
| Historia kodu, tag starego RC1 i odnośniki | Bez zmian | Zachowanie pochodzenia projektu |

OWNER musi osobno rozstrzygnąć, czy `server.name` pozostanie stabilnym identyfikatorem v1, czy zostanie zmieniony wraz z jawnie zaprojektowaną zgodnością i testami negatywnymi. Nie dopuszczać automatycznie obu nazw, nie zmieniać hosta DNS, Origin Passkey ani istniejącego prywatnego stanu.

## Granice odbioru

Sprawdzić dokładny końcowy SHA gałęzi w GitHub-hosted Ubuntu 22.04/24.04/26.04: C++ Debug/Release, rzeczywiste CLI/proces, ADC/ADCS, WebAdmin/przeglądarka, sanitizer/security, VM/systemd i izolowany rollback pakietu. Zielone CI nie upoważnia do produkcyjnego uruchomienia, zmiany nazwy pakietu, migracji danych ani wydania. Scalenie do chronionego `main` wymaga osobnej decyzji OWNER.
