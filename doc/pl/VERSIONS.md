# Wersjonowanie

**Bieżący etap — alpha-0.3.2:** [Session Manager](SESSION_MANAGER.md) jest centralnym właścicielem modeli wszystkich zarejestrowanych sesji; zapewnia limit, stabilny odczyt, liczenie faz i uporządkowane strony kopii. Obiekty protokołu pożyczają modele, a kolejki i indeksy publiczne pozostają w ADC. Starsze opisy zachowują zakres historyczny.

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
| alpha-0.2.3 | alpha-0.2.3 | ADC State: niezależny automat połączenia huba |
| alpha-0.2.4 | alpha-0.2.4 | SUP: negocjacja funkcji/hash i integracja stanów |
| alpha-0.2.5 | alpha-0.2.5 | SID: generowanie, rezerwacje RAII i powiązanie sesji |
| alpha-0.2.6 | alpha-0.2.6 | INF: informacja klienta i walidacja tożsamości Tiger |
| alpha-0.2.7 | alpha-0.2.7 | CID/PID: dedykowana weryfikacja Tiger i obsługa błędów |
| alpha-0.2.8 | alpha-0.2.8 | Login: NORMAL gościa, ograniczone wyjście TCP/TLS, publiczna obecność |
| alpha-0.2.9 | alpha-0.2.9 | STA/QUI: diagnostyka protokołu, routing i ograniczone zamykanie |
| alpha-0.2.10 | alpha-0.2.10 | Security: oczekujące sesje, łączne logowanie i limity prób tożsamości |
| alpha-0.3.1 | alpha-0.3.1 | Session Core: cykl Users, stabilne lokalne ID i snapshoty |
| alpha-0.3.2 | alpha-0.3.2 | Session Manager |

Dokładną kompilację identyfikuje wersja wraz z SHA commita. Budowanie nie zmienia plików źródłowych. Istotne zmiany trafiają do CHANGELOG.md. Alpha oznacza rozwój, nie pełną funkcjonalność huba. Tagi i wdrożenie nie powstają automatycznie.

Wcześniejsze gałęzie i historia commitów są zachowane. alpha-0.1.4 zachowuje przez merge historię referencyjnego TLS i zawiera poprawki.

## Wersja historyczna — alpha-0.2.7

CMake PROJECT_VERSION i główne wersje npm package/lock to **0.2.7**; program/WebAdmin mają **alpha-0.2.7**. Kontrola metadanych wymusza spójność. Gałąź powstaje z main w `9d3443a` po scaleniu INF i dodaje [dedykowaną weryfikację CID/PID](ADC_IDENTITY.md). Wymagania Libgcrypt/OpenSSL są dotychczasowe. Podłączenie Network, rejestr/dopuszczenie i pełne logowanie pozostają planowane. Main i wcześniejsze gałęzie są zachowane.


## alpha-0.2.8

**Aktualna integracja — alpha-0.2.8:** [Logowanie gości](ADC_LOGIN.md) podłącza state/SUP/SID/INF/Tiger do rzeczywistego reaktora C++ TCP/TLS, z ograniczonym uporządkowanym wyjściem, unikalnością nick/CID, listą obecności i pełnymi testami ncdc NORMAL/reconnect. Historyczne opisy poniżej zachowują pierwotny zakres; wzmianki o planowanym podłączeniu/wyjściu/logowaniu dotyczą wcześniejszych wersji. Konta/hasła, czat, wyszukiwanie i przekazywanie transferów pozostają planowane.

Wersja CMake/npm 0.2.8; runtime alpha-0.2.8. Gałąź alpha-0.2.8 pochodzi z main b8978491fa8e146527eba818f9bc1ab0a31aa90d. Bez automatycznego scalania main i tagu wydania.


## alpha-0.2.9

CMake/npm 0.2.9; runtime/WebAdmin alpha-0.2.9. Podstawa: main `da1521502c4a8936b3427a7d38fea6b904686b39`. [STA/QUI](ADC_STATUS.md) rozwija sesje gości bez nowego schematu API, zależności i migracji SQL. Bez automatycznego merge/tagu.


## alpha-0.2.10

CMake/npm 0.2.10, runtime/WebAdmin alpha-0.2.10. Podstawa: main `a3ca70d65a0fcbb563147e8a44a1ce2669a0ec53`. [Security](NEGOTIATION_SECURITY.md) dodaje lokalną politykę negocjacji bez zależności, migracji SQL i zmian schematu API. Bez automatycznego scalania main i tagu.


## alpha-0.3.1

CMake/npm 0.3.1, runtime/WebAdmin alpha-0.3.1. Podstawa: main `5bc79d0a12eb37d0869651eb48b57d2bf338d7fe`. [Session Core](SESSION_CORE.md) dodaje dc24h_users bez zależności, migracji, opcji bezpieczeństwa i zmian API. Bez automatycznego scalania main/tagu.
