# Wersjonowanie

| Wersja | Gałąź | Zakres |
| --- | --- | --- |
| alpha-0.1.1 | alpha-0.1.1 | Core |
| alpha-0.1.2 | alpha-0.1.2 | Network |
| alpha-0.1.3 | alpha-0.1.3 | Parser ADC |
| alpha-0.1.4 | alpha-0.1.4 | ADCS/TLS, zachowana historia chatgpt-alpha-0.1.4 |
| alpha-0.1.5 | alpha-0.1.5 | Konfiguracja: INI, CLI, ścieżki i logowanie plikowe |
| alpha-0.1.6 | alpha-0.1.6 | Logi startu, połączeń, ostrzeżeń i błędów |
| alpha-0.1.7 | alpha-0.1.7 | Ograniczone przyjęcia IP, ramki i terminy bezwzględne |
| alpha-0.1.8 | alpha-0.1.8 | Połączenie MariaDB i rejestr migracji |
| alpha-0.1.9 | alpha-0.1.9 | Lokalne API administracyjne HTTP/JSON tylko do odczytu |
| alpha-0.1.10 | alpha-0.1.10 | Układ WebAdmin React/TypeScript/Vite i lokalny adapter tylko do odczytu |
| alpha-0.1.11 | alpha-0.1.11 | Szablon HTTPS Apache2 i izolowane testy TLS/dostępu |
| alpha-0.2.1 | alpha-0.2.1 | Project: audyt alpha-0.1 i przygotowanie alpha-0.2 |
| alpha-0.2.2 | alpha-0.2.2 | ADC Parser: argumenty BASE, pola nazwane i ograniczona składnia liczb |

Dokładną kompilację identyfikuje wersja wraz z SHA commita. Budowanie nie zmienia plików źródłowych. Istotne zmiany trafiają do CHANGELOG.md. Alpha oznacza rozwój, nie pełną funkcjonalność huba. Tagi i wdrożenie nie powstają automatycznie.

Wcześniejsze gałęzie i historia commitów są zachowane. alpha-0.1.4 zachowuje przez merge historię referencyjnego TLS i zawiera poprawki.

## Aktualna wersja — alpha-0.2.2

CMake PROJECT_VERSION i główne wersje npm package/lock to **0.2.2**; program/WebAdmin mają **alpha-0.2.2**. `project.metadata` wymusza spójność. Etap parsera powstaje z main po scaleniu audytu (`ca464e0`); nie scala się samoczynnie do main i nie przesuwa historycznego znacznika alpha-0.2. Gałęzie audytu alpha-0.2.1 i alpha-0.2 pozostają zachowane. Zobacz [kontrakt parsera](ADC_PROTOCOL.md) i [historyczny audyt](AUDIT_ALPHA_0.1.md).
