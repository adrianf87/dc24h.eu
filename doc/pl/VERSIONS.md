# Wersjonowanie

| Wersja | Gałąź | Zakres |
| --- | --- | --- |
| alpha-0.1.1 | alpha-0.1.1 | Core |
| alpha-0.1.2 | alpha-0.1.2 | Network |
| alpha-0.1.3 | alpha-0.1.3 | Parser ADC |
| alpha-0.1.4 | alpha-0.1.4 | ADCS/TLS, zachowana historia chatgpt-alpha-0.1.4 |
| alpha-0.1.5 | alpha-0.1.5 | Konfiguracja: INI, CLI, ścieżki i logowanie plikowe |
| alpha-0.1.6 | alpha-0.1.6 | Logi startu, połączeń, ostrzeżeń i błędów |

CMake PROJECT_VERSION to 0.1.6; wersja programu to alpha-0.1.6. Dokładną kompilację identyfikuje wersja wraz z SHA commita. Budowanie nie zmienia plików źródłowych. Istotne zmiany trafiają do CHANGELOG.md. Alpha oznacza rozwój, nie pełną funkcjonalność huba. Tagi i wdrożenie nie powstają automatycznie.

Wcześniejsze gałęzie i historia commitów są zachowane. alpha-0.1.4 zachowuje przez merge historię referencyjnego TLS i zawiera poprawki.

alpha-0.1.5 jest scalona z main. alpha-0.1.6 powstaje z tego main i nie zmienia go automatycznie.
