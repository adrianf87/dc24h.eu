# Plan rozwoju

## Ukończone: alpha-0.1.1 Core

- Struktura projektu i budowanie C++20/CMake.
- Uruchamianie, zatrzymywanie, stany Core i obsługa SIGINT/SIGTERM.
- Podstawowe logowanie, CLI, testy i dokumentacja.

## Kolejne etapy — niewykonane

1. Configuration: walidowany format pliku i kontrola błędów konfiguracji.
2. Network: TCP, zarządzanie połączeniami, limity, timeouty i stop listenera.
3. ADC: parser/serializer i testy względem oficjalnej specyfikacji.
4. ADCS: infrastruktura TLS, certyfikaty i testy handshake.
5. Users/Security/Database: sesje, konta, migracje i uwierzytelnianie.
6. Chat, Search i Transfers, wprowadzane oraz testowane niezależnie.
7. Zabezpieczone API i szkielet WebAdmin React/TypeScript/Vite z Apache2.

Numery następnych wydań i dokładny zakres zostaną ustalone dla poszczególnych
etapów. Pełny hub ADC/ADCS jest celem projektu, nie właściwością tej alphy.
