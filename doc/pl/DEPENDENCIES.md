# Zależności — alpha-0.9.17

Kontrola bibliotek i automatyczne testy bezpieczeństwa. Gałąź powstaje z main `6940992`. Nazwa gałęzi to `alpha-0.9.17`.

Konfiguracja wymaga OpenSSL 3. Pliki wykonywalne huba muszą bezpośrednio łączyć te same pięć bibliotek produktu i żadnej innej biblioteki produktu. Zależności WebAdmin zostają na dokładnych wersjach z rejestru npm. Istniejące testy bezpieczeństwa zostają zarejestrowane. Nie ma nowej trasy, klucza INI, migracji schematu, ustawienia Apache, flagi CLI, strony WebAdmin ani pola statusu. To nie jest baza podatności i to nie uruchamia `npm audit`.

## Pliki wykonywalne huba

`dependencies.audit` czyta sekcję dynamiczną `dc24h`, `dc24h-owner` i `dc24h-account-admin`. Każdy z nich musi potrzebować wszystkich tych bibliotek:

- `libssl.so.3`
- `libcrypto.so.3`
- `libgcrypt.so.20`
- `libmariadb.so.3`
- `libutf8proc.so.3`

Środowisko C++ może dodatkowo potrzebować `libc.so.6`, `libstdc++.so.6`, `libgcc_s.so.1` i `libm.so.6`. Każda inna biblioteka dzielona kończy test niepowodzeniem. Pliki nie mogą mieć `RPATH` ani `RUNPATH`.

Build AddressSanitizer może dodatkowo potrzebować `libasan.so.8`. Build UndefinedBehaviorSanitizer może dodatkowo potrzebować `libubsan.so.1`. Build ThreadSanitizer może dodatkowo potrzebować `libtsan.so.2`. Zwykły build kończy się niepowodzeniem, gdy obecne jest jedno z tych środowisk. Wybrane środowisko sanitizera musi być obecne w tym buildzie. Zwykłe buildy Debug i Release zostawiają `DC24H_SANITIZER` puste.

## WebAdmin

Zależności i zależności deweloperskie w `package.json` są dokładnymi wersjami `major.minor.patch`. Muszą zgadzać się z pakietem głównym w `package-lock.json`. Każdy zablokowany pakiet, który ma adres pobrania, musi używać `https://registry.npmjs.org/` i integralności `sha512`. Audyt nie instaluje pakietów i nie pyta rejestru, czy istnieje nowsza wersja.

## Testy bezpieczeństwa

`tests/security_tests.manifest` wymienia testy bezpieczeństwa, które CMake rejestruje zawsze. `dependencies.audit` kończy się niepowodzeniem, gdy jednej z tych nazw nie ma w buildzie. `systemd.sandbox` pozostaje warunkowy od narzędzi systemd, tak jak dotychczas. Manifest nie oznacza, że te testy pokrywają każde zagrożenie.

## Granice

Check status w GitHub nadal buduje Debug i Release na Ubuntu 24.04. Audyt działa wewnątrz tego istniejącego kroku testów. Nie dodaje zadania, macierzy sanitizerów ani skanu sieci. Schemat v9 pozostaje bez zmian. `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Kick i mute w przeglądarce pozostają bez wysyłki. Status i health zostają tylko z bearer. `Require local` zostaje. Żadne zagrożenie szczątkowe nie jest zamknięte.
