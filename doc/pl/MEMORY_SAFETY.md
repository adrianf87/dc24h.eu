# Bezpieczeństwo pamięci — alpha-0.9.15

AddressSanitizer, UndefinedBehaviorSanitizer i ThreadSanitizer. Gałąź powstaje z main `a773395`. Nazwa gałęzi to `alpha-0.9.15`.

Każdy sanitizer to osobny build Debug. Pusty przełącznik zostawia hub, protokół i testy na zwykłym buildzie. Nie ma nowej trasy, klucza INI, migracji schematu, ustawienia Apache, flagi CLI, strony WebAdmin ani pola statusu. Zaliczenie próby nie zastępuje zestawu testów. To nie jest deklaracja pojemności.

## Tryby budowania

Skonfiguruj jeden tryb. AddressSanitizer i ThreadSanitizer nie mogą dzielić jednego programu.

```sh
cmake -S . -B build-address -DCMAKE_BUILD_TYPE=Debug -DDC24H_SANITIZER=address -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake -S . -B build-undefined -DCMAKE_BUILD_TYPE=Debug -DDC24H_SANITIZER=undefined -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake -S . -B build-thread -DCMAKE_BUILD_TYPE=Debug -DDC24H_SANITIZER=thread -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
```

`address` kompiluje i linkuje `-fsanitize=address` i zatrzymuje się na pierwszym błędzie, łącznie z wyciekami. `undefined` kompiluje i linkuje `-fsanitize=undefined` i zatrzymuje się na pierwszym błędzie. `thread` kompiluje i linkuje `-fsanitize=thread` i zatrzymuje się na pierwszym wyścigu danych w kodzie huba. OpenSSL nie jest instrumentowany. Jego własne blokady chronią równoczesne wyprowadzanie haseł, więc ten tryb pomija zgłoszenia, których wywołujący interceptor to `libcrypto.so` albo `libssl.so`. Wyścig w kodzie huba nadal przerywa przebieg. Każda inna wartość przerywa konfigurację. Zwykłe buildy Debug i Release zostawiają `DC24H_SANITIZER` puste.

CTest w takim buildzie ustawia `ASAN_OPTIONS`, `UBSAN_OPTIONS` albo `TSAN_OPTIONS`. Ustawia też `DC24H_SANITIZER` na nazwę trybu.

## Próba

`memory.safety` alokuje małą tablicę, łączy dwa wątki na liczniku atomowym i liczniku z muteksem oraz mnoży dwie mieszczące się liczby ze znakiem. Buildy AddressSanitizer i ThreadSanitizer odrzucają próbę, gdy jednostka tłumaczenia nie jest instrumentowana. Linia sukcesu to `Alpha 0.9.15 memory safety probe OK` oraz `mode=address`, `mode=undefined`, `mode=thread` albo `mode=none`.

## Piaskownica

Dostarczana jednostka systemd nadal ustawia `MemoryDenyWriteExecute=yes`. To ustawienie zostaje. Uruchomienie huba w piaskownicy pomija build AddressSanitizer i ThreadSanitizer, bo te środowiska nie mogą zmapować swojego cienia przy tym ograniczeniu. Tekst jednostki, prywatne sprawdzenie sysusers i odmowa zapisu na hoście nadal działają. UndefinedBehaviorSanitizer nadal uruchamia hub w piaskownicy.

## Granice

Check status w GitHub nadal buduje Debug i Release na Ubuntu 24.04. Nie buduje trzech drzew sanitizerów. Schemat v9 pozostaje bez zmian. `configuration.manage` i `hub.manage` pozostają niezaimplementowane. Kick i mute w przeglądarce nadal nie są wysyłane. Status i health nadal wymagają tylko bearer. `Require local` zostaje. To nie zamyka zagrożenia szczątkowego.
