# Plan rozwoju

## Ukończone: alpha-0.1.1 Core

- Struktura projektu i budowanie C++20/CMake.
- Uruchamianie, zatrzymywanie, stany Core i obsługa SIGINT/SIGTERM.
- Podstawowe logowanie, CLI, testy i dokumentacja.

## Ukończone: alpha-0.1.2 Network

- Nieblokujący serwer TCP IPv4/IPv6 z wieloma portami i klientami.
- `epoll`, RAII gniazd, `eventfd` do natychmiastowego budzenia stopu.
- Opcje CLI, limit połączeń i timeout bezczynności.
- Izolacja błędów peer, odzyskiwanie po braku zasobów i zwijanie błędnego startu.
- Testy transportu, integracji z Core, CLI i dokumentacja.

## Ukończone: alpha-0.1.3 ADC — składnia

- Oddzielny parser nagłówków, routingu, komend i parametrów.
- Escaping, UTF-8/NFC, limity i odzyskiwanie po błędnych danych.
- Strumieniowe ramkowanie i niezależny dekoder każdego klienta TCP.
- Liczniki bez logowania treści, testy C++/TCP i początkowego HSUP z ncdc.

## Kolejne etapy — niewykonane

1. ADC: serializer, semantyka komend i pól, automat handshake/sesji,
   przydział SID, bezpieczne kierowanie wiadomości, ograniczone bufory
   wyjściowe i backpressure; pełne logowanie ncdc.
2. ADCS: infrastruktura TLS, certyfikaty i testy handshake.
3. Configuration: walidowany format pliku i kontrola błędów konfiguracji.
4. Users/Security/Database: sesje, konta, migracje i uwierzytelnianie.
5. Chat, Search i Transfers, wprowadzane oraz testowane niezależnie.
6. Zabezpieczone API i szkielet WebAdmin React/TypeScript/Vite z Apache2.

Numery następnych wydań i dokładny zakres zostaną ustalone osobno.
