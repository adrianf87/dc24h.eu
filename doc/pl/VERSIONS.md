# Wersjonowanie

| Wersja | Gałąź | Zakres |
| --- | --- | --- |
| alpha-0.1.1 | alpha-0.1.1 | Core |
| alpha-0.1.2 | alpha-0.1.2 | Network |
| alpha-0.1.3 | alpha-0.1.3 | Parser ADC |
| alpha-0.1.4 | alpha-0.1.4 | ADCS/TLS, zachowana historia chatgpt-alpha-0.1.4 |

CMake PROJECT_VERSION to 0.1.4; wersja programu to alpha-0.1.4. Dokładną kompilację identyfikuje wersja wraz z SHA commita. Budowanie nie zmienia plików źródłowych. Istotne zmiany trafiają do CHANGELOG.md. Alpha oznacza rozwój, nie pełną funkcjonalność huba. Tagi i wdrożenie nie powstają automatycznie.

Pierwsze trzy etapy istnieją już w historii main. Nowa gałąź alpha-0.1.4 powstała z main; przez merge zachowuje historię referencyjnego TLS i zawiera poprawki. Main i wcześniejsze gałęzie pozostają zachowane.
