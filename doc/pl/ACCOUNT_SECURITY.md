# Bezpieczeństwo konta — alpha-0.9.6

Hasła, GPA/PAS i ochrona sekretów. Gałąź powstaje z main `22bc5bc`. Nazwa gałęzi to `alpha-0.9.6`.

Hub już przechowuje hasło konta jako weryfikator scrypt o stałym koszcie i osobne hasło ADC w AES-256-GCM. Wystawia jedno wyzwanie Tiger GPA na połączenie i ogranicza powtarzane próby. Ten etap dodaje dwie stałe reguły. Nie są opcjonalne i nie są kluczami INI.

## Różne hasła

Zapis odmawia hasła ADC, które jest tym samym ciągiem bajtów co hasło konta potwierdzone w tym wywołaniu. Przy równej długości porównanie używa `CRYPTO_memcmp`. Inna długość, inna wielkość liter albo jeden dodatkowy znak to inne hasło. Sprawdzenie następuje dopiero po udanym dowodzie scrypt, więc zły dowód pozostaje błędem dowodu. Komunikat nie zawiera żadnego z haseł.

Już zapisane wiersze nie są przepisywane. Start nie porównuje obu haseł, bo hasło konta nie jest wtedy dostępne jako tekst jawny. Zmiana hasła konta nadal usuwa poświadczenie ADC. Następny zapis stosuje tę regułę. Nie ma migracji schematu.

## Zapieczętowane do akceptacji

`prepare` rozpoznaje gościa, odmowę albo konto. Nie odszyfrowuje. Reaktor odszyfrowuje hasło ADC tylko w `open_challenge`, po tym jak sprawdzenie bana konta i budżet prób tego konta obie zaakceptują połączenie. Próba zbanowanego albo ograniczonego konta zachowuje dotychczasową ogólną odmowę i nie dostaje GPA. Bufor tekstu jawnego jest czyszczony przy wystawieniu wyzwania. Start nadal odszyfrowuje każdy zapisany rekord raz, żeby odrzucić złą kopertę przed nasłuchem, i potem czyści tę kopię.

Akceptacja IP nadal jest przed `prepare`. Odmowa tożsamości, wyłączone konto, brak poświadczenia albo zwykłe ADC przy włączonym `require_tls` nadal wracają przed odszyfrowaniem. Dubler testowy, który wystawia wyzwanie w `prepare`, zachowuje to zachowanie. Katalog produkcyjny tego nie robi.

## Bez zmian

GPA/PAS pozostaje Tiger(hasło sklejone z wyzwaniem), jedno wyzwanie na połączenie, zużywane przy każdej próbie. Odmowa pozostaje ogólnym `ISTA 223 Authentication failed`. Logi nadal pomijają hasła, ładunki GPA/PAS i szyfrogram. `require_tls` nadal domyślnie jest włączone. `require_tls=off` nadal pozwala atakować offline przechwycony PAS. Kubełki prób nadal zerują się przy restarcie procesu i nadal należą do jednego procesu huba. To nie zamyka zagrożeń szczątkowych T02 ani T03. Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, strony WebAdmin, publicznego punktu haseł, argumentu hasła w CLI ani ustawienia Apache. Automatyczna rotacja klucza nie jest zaimplementowana. HTTP/2, OCSP i automatyczne odnawianie certyfikatu pozostają niezaimplementowane. Publiczna uwierzytelniona brama pozostaje osobną pracą.

`accounts.account_security` sprawdza, że wyczerpany budżet konta i ban konta nie otwierają zapieczętowanego sekretu, że inne konto nadal dostaje GPA oraz że gość i niezgodna tożsamość go nie otwierają. `accounts.adc_authentication.process` sprawdza odmowę identycznego hasła, zapis hasła ADC o innej wielkości liter, zapieczętowane `prepare` i jednorazowe `open_challenge`.
