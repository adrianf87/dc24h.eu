# Password Security — alpha-0.7.7

**alpha-0.7.8:** [ADC Authentication](ADC_AUTHENTICATION.md) wdraża logowanie kont GPA/PAS, osobne hasło ADC szyfrowane AES-256-GCM i prywatny klucz poza bazą, zachowując scrypt. Wymagane są jawna migracja v6, aktywne konto i dokładna para nick/CID; domyślnie obowiązuje ADCS. Zmiany i blokady w katalogu startowym wymagają restartu. Publiczne przygotowanie kont, reset, rotacja i nadawanie ról pozostają kolejnymi etapami. Ten wpis zastępuje wcześniejsze opisy planowanego uwierzytelniania.

Etap rozpoczyna się bezpośrednio z main `75c82eea4d5799460bebe93ed1b6accef175ce25`.
Zachowuje rejestrację i mapowanie tożsamości, dodaje zaufane API sprawdzania
haseł i pozostawia migracje SQL 1–5 bez zmian. Nie scala się automatycznie
z main ani nie modyfikuje bazy produkcyjnej.

## Kontrakt i granice

Dołącz bibliotekę `dc24h_password_security` i nagłówek
`dc24h/accounts/password_security.hpp`. `PasswordSecurityService` pożycza
połączenie Database Core w jego wątku właściciela. Konstruktor nie wykonuje I/O.
`verify(AccountId, password)` zwraca wyłącznie enum:

| Wynik | Znaczenie |
| --- | --- |
| `matched` | Podane hasło pasuje do odczytanego weryfikatora tego ID konta. |
| `mismatch` | Poprawny zapis sprawdzono stałymi parametrami KDF; hasło nie pasuje. |
| `unavailable` | Brak rekordu uwierzytelniającego, również przy braku konta lub koncie zawierającym tylko metadane. |

Wynik nie zawiera hasła, soli, weryfikatora, wyniku SQL ani pożyczonego widoku.
To zaufany, synchroniczny element aplikacji, a nie endpoint logowania.
Wywołujący autoryzuje dostęp, ogranicza równoczesną pracę KDF i wykonuje ją
poza reaktorem Network. Nie dodano komendy hasła ADC, trasy HTTP, akcji
WebAdmin ani CLI przyjmującego sekrety. Bajty hasła są pożyczone na czas
wywołania; wywołujący odpowiada za ich przechowywanie i wymazanie.

Dopasowanie nie aktywuje konta, nie nadaje roli, nie ustawia ostatniego
logowania, nie rozstrzyga właściciela CID/nicka i nie dopuszcza sesji.
Hasło wyłączonego konta także może pasować. Status, autoryzacja i polityka
sesji są odrębne. Wynik dotyczy odczytanej wersji rekordu, bez gwarancji,
że administrator później nie zmienił danych. KDF nie utrzymuje blokad SQL
ani transakcji.

Zachowano politykę v1: poprawny UTF-8, minimum 15 wartości skalarnych Unicode,
maksimum 1024 bajty, bez NUL, C0 i DEL. Spacje, wielkość liter i kompozycja
Unicode pozostają dokładne; nie ma przycinania, normalizacji ani obcinania.
Błędne dane są odrzucane przed SQL i kosztowną pracą.

## Format i kryptografia

Format rejestracji v1 pozostaje identyczny:
`$dc24h-scrypt$v=1,ln=17,r=8,p=1$<64 małe znaki hex soli>$<64 małe znaki hex klucza>`.
Dekoder wymaga dokładnej długości, prefiksu, separatorów i małych znaków hex.
Nadmiarowe bajty, NUL, nieobsługiwana wersja/algorytm/koszt, inne kodowanie
lub zbyt długi rekord są odrzucane przed scrypt. Parametry są stałe;
zapis w bazie nie wybiera kosztu KDF.

Używamy istniejącego OpenSSL `EVP_PBE_scrypt`: **N=131072, r=8, p=1**,
32 bajty soli i wyniku oraz limit pamięci dostawcy 256 MiB. Jedno wywołanie
potrzebuje około 128 MiB i narzutu; limit nie jest globalnym budżetem
równoległości. Rejestracja nadal tworzy nową losową sól i używa tych samych
parametrów. Nie ma słabszego fallbacku ani publicznego wstrzykiwania dostawcy.

Końcowe 32-bajtowe klucze porównuje OpenSSL `CRYPTO_memcmp`, którego czas
porównania nie zależy od zawartości kluczy. Nie oznacza to stałego czasu całej
operacji: SQL, brak danych, walidacja, format, alokacje i scrypt mogą trwać
różnie. Przyszła publiczna warstwa logowania musi określić limity przyjęć,
bezpieczny transport, ograniczanie żądań, ochronę przed enumeracją kont
i budżet równoczesnych KDF.

Zdekodowana sól oraz oczekiwany i obliczony klucz mają stałe, niekopiowalne
bufory wymazywane przez `OPENSSL_cleanse` przy zwykłym i wyjątkowym wyjściu.
Serwis pożycza ograniczoną komórkę wyniku SQL i wymazuje ją przed zniszczeniem
wyniku, także przy błędnym formacie lub błędzie dostawcy. Nie kopiuje
weryfikatora. Dotychczasowe czyszczenie podczas rejestracji pozostaje aktywne.

Gwarancje dotyczą jawnie posiadanych buforów aplikacji. Nie obejmują pamięci
hasła wywołującego, rejestrów, nadmiarowej pojemności alokatora, wnętrza
Connector/C i OpenSSL, logów SQL, swapu, zrzutów pamięci ani kopii zapasowych.
Błędne zapytanie może pozostawić dane w warstwie bazy przed zwróceniem wyniku.
Ogranicz dostęp do procesu, bazy i kopii; nie loguj argumentów ani sekretów.

## SQL i błędy

Serwis wymaga aktualnych, czystych migracji oraz zgodnych tabel kont
i weryfikatorów. Wykorzystuje istniejące kontrole schematu. Nie migruje,
nie rejestruje kont, nie łączy się ponownie i nie zmienia żadnej tabeli.
Parametryzowany SELECT ma limit jednego wiersza, jednej kolumny i 192 bajtów.
Walidacja formatu jest ostrzejsza od limitu rozmiaru kolumny.

Konto wykonawcze potrzebuje SELECT na `dc24h_schema_migrations`,
`dc24h_accounts` i `dc24h_account_credentials`. Nie potrzebuje INSERT,
UPDATE, DELETE, DDL ani tabeli przypisań tożsamości. Brak widoczności tabeli
uwierzytelniającej oznacza odmowę. Ograniczone kontrole zgodności nie są
audytem wszystkich CHECK, triggerów, akcji referencyjnych i równoczesnego DDL.

`PasswordSecurityErrorKind` rozróżnia `invalid_password`, `invalid_record`,
`crypto` i `schema`. Stałe komunikaty EN/US nie zawierają wejścia ani SQL.
Awaria kryptografii rzuca błąd zamiast uznać hasło lub wybrać słabszy wariant.
Błędy repozytorium, migracji i Database Core mogą być propagowane ze swoją
dotychczasową redakcją danych. Transakcja wywołującego i obcy wątek są odrzucane.

## Weryfikacja i dalsze etapy

`accounts.password_security` sprawdza niezależny wzorzec scrypt, właściwe
i błędne hasła, uszkodzenie każdego bajtu rekordu, odrzucenie kosztu przed KDF,
dokładne spacje/kompozycję Unicode i granicę długości. Lokalne wrappery testowe
sprawdzają stałe parametry, użycie porównania w stałym czasie, błąd dostawcy
i czyszczenie pamięci. Zwykłe testy wykonują rzeczywisty scrypt produkcyjny.

`accounts.password_security.process` działa w prywatnej MariaDB bez sieci:
prawdziwe konta i brak danych, niezmienione metadane, minimalne uprawnienia
SELECT, odmowa dostępu do weryfikatorów, uszkodzone rekordy, brak klucza
obcego, wątek/transakcje, awarie dostawcy, wymazanie komórki SQL oraz błędy
alokacji. Tylko przegląd awarii alokacji zastępuje kosztowny KDF wewnątrz
pliku wykonywalnego testu; produkt nie ma takiej konfiguracji.

Kontrola metadanych wersji obejmuje teraz też oczekiwania CLI/log/API
i przeglądarki, aby podniesienie wersji nie zostawiało starych asercji.
Faktyczne wyniki: [Testowanie](TESTING.md), [Contabo CI](CONTABO_CI.md)
i [Changelog](../../CHANGELOG.md). Pełne CI Debug/Release, ncdc ADC/ADCS
i WebAdmin działa na jednym runnerze Ubuntu 24.04 z równoległością 6.
Nie czekamy w tym zadaniu na zakończenie zdalnego CI.

Zmiana/reset hasła, adaptacyjne przeliczanie KDF, inne formaty, kontrola
ujawnionych haseł, aktywacja i dopuszczanie uwierzytelnionych sesji oraz
publiczne endpointy kont są kolejnymi etapami. Nie dodajemy migracji SQL.

Źródła: [OpenSSL — porównanie](https://docs.openssl.org/3.0/man3/CRYPTO_memcmp/),
[OpenSSL — scrypt](https://docs.openssl.org/3.0/man7/EVP_KDF-SCRYPT/),
[OWASP — przechowywanie haseł](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html).
