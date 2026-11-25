# dc24h.eu-twilight — Etap III: branding operatora i tożsamość statusu

Pierwszy merge Etapu III zmieniał wyłącznie *teksty wyświetlane*. Po osobnej zgodzie OWNER kolejny PR zmienia też ścisły identyfikator `server.name` w API v1 i jego odbiorcach WebAdmin. Zaufanie sieciowe i nazwy zainstalowanych usług pozostają bez zmian.

## Branding operatora scalony w Etapie III

- `dc24h --version` pokazuje teraz `dc24h.eu-twilight beta-0.9.20`; plik wykonywalny nadal nazywa się `dc24h`.
- Log startowy, pomoc CLI w PL/EN, domyślne powitanie ADC oraz przykładowy plik INI pokazują nazwę Twilight. Powitanie skonfigurowane wcześniej przez operatora w `welcome_message` nie jest przepisywane.
- Opisy przykładowych jednostek systemd i komentarze Apache/INI używają nowej marki. Nazwy jednostek, ścieżki binarek, `ExecStart`, pakiety i działające usługi pozostają bez zmian.
- Testy procesu, logów, powitania ADC oraz izolowanej instalacji/rollbacku dostosowano do nowego tekstu. Historyczne binarki zachowują *oryginalną* nazwę wersji; pole dowodowe dla bieżącego kandydata nadal zapisuje wyłącznie `beta-0.9.20`.

## Zmiana tożsamości statusu zatwierdzona przez OWNER (osobny PR)

- C++ publikuje teraz `server.name = "dc24h.eu-twilight"` w dotychczasowej odpowiedzi status API v1; `api_version`, struktura JSON, wersja i liczniki pozostają bez zmian.
- Oba walidatory WebAdmin (`parseSnapshot` i `parseRemoteSummary`), wyniki normalizacji i fixture wymagają **dokładnie** `dc24h.eu-twilight`.
- Stara wartość `dc24h.eu`, nazwy obce, puste i brakujące są odrzucane. Nie dodano niejawnej zgodności akceptującej obie nazwy ani domyślania się tożsamości z nagłówka HTTP Host.
- To **świadome zerwanie kompatybilności** konsumentów statusu: przy ewentualnym, osobno zatwierdzonym wdrożeniu należy aktualizować C++ i WebAdmin razem. Stary WebAdmin nie obsłuży nowego serwera, a nowy WebAdmin odrzuci status starego źródła. Przed wdrożeniem trzeba zinwentaryzować i zaktualizować zewnętrznych klientów statusu.
- Testy sprawdzają nazwę zwracaną przez C++ oraz ścisłe przyjmowanie/odrzucanie w obu parserach WebAdmin. Ten PR wymaga osobnego GitHub-hosted CI na finalnym SHA i zgody OWNER przed merge.
- W izolowanym teście VM po rzeczywistym downgrade historyczne C++ `beta-0.5.16` działa wyłącznie z **testową, tymczasową kopią** bramy Node, w której parser statusu jest pobrany z niezmiennego historycznego commita. Dzięki temu test nadal sprawdza rzeczywiste logowanie, TOTP, Passkey i odzyskiwanie stanu, ale **nie** twierdzi, że nowy produkcyjny WebAdmin obsługuje stary serwer. Bieżące parsery nadal odrzucają `dc24h.eu`.

## Bez zmian do kolejnej osobnej decyzji OWNER

| Identyfikator | Obecna wartość / działanie | Uzasadnienie |
| --- | --- | --- |
| Publiczne/Apache `ServerName`, przekierowania, kontrole Host i Origin | Dotychczasowe przykłady hosta | DNS, TLS i zaufane Origin to decyzje infrastrukturalne |
| WebAuthn `rpID`, skonfigurowany Origin, poświadczenia | Bez zmian | Rebranding nie może naruszyć weryfikacji kluczy |
| Binarki, pakiet npm, jednostki systemd, konfiguracja i ścieżki stanu | Dotychczasowe identyfikatory `dc24h*` | Migracja nazw i rollback należą do Etapu IV |
| Historia kodu, tag starego RC1 i odnośniki | Bez zmian | Zachowanie pochodzenia projektu |

Nowa wartość `server.name` **nie jest** domeną DNS, nazwą certyfikatu TLS, identyfikatorem WebAuthn RP ani ścieżką konfiguracji. Nie zmieniać hostów, Origin Passkey ani prywatnego stanu w ramach tej poprawki kodu.

## Granice odbioru

Sprawdzić dokładny końcowy SHA gałęzi w GitHub-hosted Ubuntu 22.04/24.04/26.04: C++ Debug/Release, rzeczywiste CLI/proces, ADC/ADCS, WebAdmin/przeglądarka, sanitizer/security, VM/systemd i izolowany rollback pakietu. Zielone CI nie upoważnia do produkcyjnego uruchomienia, zmiany nazwy pakietu, migracji danych ani wydania. Scalenie do chronionego `main` wymaga osobnej decyzji OWNER.
