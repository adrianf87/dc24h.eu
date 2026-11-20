# Rejestracja — alpha-0.7.5

**Rozszerzenie alpha-0.7.7:** [Password Security](PASSWORD_SECURITY.md) sprawdza niezmieniony format v1 przez zaufany serwis tylko do odczytu. Rejestracja i migracje SQL pozostają identyczne. Historyczne plany poniżej dotyczą pierwotnego zakresu alpha-0.7.5.

Podstawa: bezpośrednio main `493e146af8192e0a87288261a9c9548ec2ee66da`. Zachowano Database Core, Migracje, Model konta i Repozytorium kont. Gałąź `alpha-0.7.5` nie jest automatycznie scalana.

## Kontrakt i zakres

Dołącz cel CMake `dc24h_registration` i nagłówek `dc24h/accounts/registration.hpp`. `RegistrationService` pożycza połączenie Database Core i udostępnia `create_account(name, password, created_at)`. Zwraca własny `StoredAccount` po jednym poprawnym commit. Konstruktor nie wykonuje I/O. Połączenie musi istnieć dłużej, pozostać na swoim wątku i nie mieć aktywnej transakcji wywołującego.

To zaufana synchroniczna usługa C++ aplikacji. Wywołujący zapewnia autoryzację i wykonuje ją poza reaktorem Network; nie ma publicznej rejestracji ADC, HTTP, WebAdmin ani CLI przyjmującego hasło. Rejestracja nie zmienia logowania gości, nie sprawdza hasła przy logowaniu i nie nadaje sesji uprawnień. Nowe konto ma zawsze **registered/disabled**, rewizję 1, podaną sekundę UTC jako utworzenie/zmianę i brak ostatniego logowania. Brak wejściowej roli/stanu, ID konta i dopisywania poświadczeń istniejącemu kontu. Konta metadanych z v3 pozostają bez zmian i bez wymyślonych haseł.

| Wejście | Walidacja |
| --- | --- |
| Nazwa | Dotychczasowe `AccountName`: UTF-8 już w NFC, 1–64 bajty, bez znaków sterujących/formatujących/separatorów. Dokładne bajty i wielkość liter decydują o unikalności; bez przycinania i casefold. |
| Hasło | Poprawne UTF-8, przynajmniej 15 wartości skalarnych Unicode i najwyżej 1024 bajty. Odrzuca NUL, C0 i DEL. Zachowuje spacje, wielkość liter i postać Unicode; bez normalizacji, przycinania, skracania i reguł składu. Same limity nie potwierdzają entropii. |
| Czas utworzenia | Jawne sekundy Unix UTC 0–253402300799. Bez niejawnego zegara/strefy; wywołujący zapewnia zaufaną politykę czasu. |

Argumenty pożyczają pamięć na cały czas wywołania. Wywołujący posiada i czyści jawne hasło; usługa nie zapisuje jego kopii ani nie umieszcza hasła/weryfikatora w migawce konta, błędach, logach czy API. Nie przekazuj sekretów przez argumenty procesu ani logowane struktury żądań. Nazwa konta jest zwykłą metadaną zapisaną w tabeli kont.

## Przechowywanie hasła

Istniejący OpenSSL dostarcza `RAND_bytes` i `EVP_PBE_scrypt`. Stałe parametry produkcyjne: **N=131072, r=8, p=1**, świeża losowa sól 32 bajty i klucz 32 bajty. KDF potrzebuje około 128 MiB plus narzut, z limitem pamięci dostawcy 256 MiB. Wywołujący nie wybiera kosztu, trybu testowego ani słabszego algorytmu. Błąd losowości/KDF daje stały błąd `crypto` i nie tworzy konta. KDF działa przed transakcją zapisu i blokadami rekordów.

Format: `$dc24h-scrypt$v=1,ln=17,r=8,p=1$<64 małe znaki hex soli>$<64 małe znaki hex klucza>`. To własna wersjonowana otoczka aplikacji, bez obietnicy zgodności z parserem PHC innej biblioteki. Przyszła weryfikacja musi dopuszczać tylko znany format i ograniczać parametry przed obliczeniami. Rotacja, rehash, reset i sprawdzanie hasła należą do kolejnych etapów.

Tymczasowe bufory soli/klucza/otoczki są stałe, niekopiowalne i czyszczone przy wyjściu. Jedyna własna kopia otoczki na stercie użyta jako binarny parametr SQL jest czyszczona przed zwolnieniem. Nie oznacza to wymazania pamięci wywołującego, rejestrów, wewnętrznych alokacji OpenSSL, kopii Connector/C, buforów SQL, kopii zapasowych ani obserwacji zewnętrznych. Ogranicz dostęp do tabeli poświadczeń i kopii zapasowych. Poświadczenia i wynik nie należą do logów.

Wybrano udokumentowany wariant scrypt dla sytuacji, gdy Argon2id nie jest dostępny w dotychczasowym zestawie zależności wszystkich systemów. Brak nowej paczki i własnej implementacji kryptografii. Źródła: [OWASP](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html), [OpenSSL scrypt](https://docs.openssl.org/1.1.1/man7/scrypt/), [losowość OpenSSL](https://docs.openssl.org/1.1.1/man3/RAND_bytes/).

## Baza i błędy

Jawna migracja v4 tworzy InnoDB `dc24h_account_credentials`: `account_id BIGINT` jako klucz główny/obcy do `dc24h_accounts.id`, `verifier VARBINARY(192)` i jawny czas UTC `created_at BIGINT`. Klucz obcy ogranicza aktualizacje/usuwanie wskazanych ID. Bajty migracji 1–3 pozostają bez zmian. Pusta baza stosuje 4 migracje, v1 trzy, v2 dwie, a v3 jedną; stan docelowy to `version=4 latest=4 pending=0 dirty=no`. Zwykły start nie migruje. Przed jawną migracją wykonaj kopię; v4 używa trwałego dziennika applying/applied i odmawia automatycznego odtwarzania częściowego DDL.

Przed haszowaniem rejestracja wymaga aktualnej czystej historii, zgodnej tabeli kont i zgodnego silnika, kolumn, klucza głównego oraz odwzorowania klucza obcego poświadczeń. To ograniczona kontrola zgodności, nie audyt wszystkich wartości domyślnych, ograniczeń, triggerów, zewnętrznych zapisów i równoczesnego DDL administratora. Reguły akcji klucza obcego definiuje dostarczona migracja; odczyt metadanych nie wymaga szerokich uprawnień DDL. Oddziel konta migracji i wykonania. Rejestracja wymaga SELECT metadanych migracji/kont/poświadczeń oraz INSERT tabel kont/poświadczeń, bez DDL, DELETE, UPDATE i ogólnych praw administratora. Aktualizacje repozytorium mają własne wymaganie UPDATE.

Prywatny punkt składania operacji repozytorium zapisuje metadane wewnątrz transakcji RAII należącej do rejestracji. Poświadczenia są zapisywane przed wspólnym commit. Duplikat, odmowa uprawnień, błąd alokacji i inny pewny błąd przed commit wycofuje oba zapisy. Duplikat rejestracji nigdy nie zastępuje istniejącego konta/hasła. Równoczesne próby tej samej nazwy dają jedną zatwierdzoną parę. ID mogą mieć luki po rollback. Publiczne metody repozytorium nadal posiadają własną transakcję i odrzucają transakcje wywołującego.

`RegistrationErrorKind`: `invalid_name`, `invalid_password`, `invalid_time`, `crypto`, `schema`, ze stałym tekstem EN/US. Propagowane są istniejące błędy repozytorium, Database Core i migracji. Błąd bazy zachowuje `outcome_unknown`: po utracie potwierdzenia zapisu/commit sprawdź stan nowym połączeniem i uzgodnij wynik przed ponowieniem przez operatora. Nie ma automatycznego reconnect ani odtwarzania zapisu; atomowość nie potwierdza wyniku niepotwierdzonego commit.

## Weryfikacja i dalsze etapy

`accounts.registration` sprawdza granice UTF-8/znaków/bajtów, zachowanie spacji, niezależny wektor scrypt, deterministyczne błędy RNG/KDF i czyszczenie buforów. `accounts.registration.process` używa tymczasowej MariaDB i sprawdza domyślne role/stan/czas, różne sole, niezależne `hashlib.scrypt` Pythona dla zapisanych rekordów, ochronę duplikatu, równoczesne rejestracje, ograniczone prawa, zmiany schematu, rollback i kolejne błędy alokacji aż do sukcesu. Tylko test wszystkich alokacji zastępuje pracę KDF wrapperem lokalnym dla programu testowego; zwykłe testy używają rzeczywistego KDF o produkcyjnym koszcie. Biblioteka produkcyjna nie ma wstrzykiwania dostawcy.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --parallel 4
ctest --test-dir build -R '^accounts.registration' --output-on-failure
```

Nadal wymagane są rzeczywiste testy ncdc ADC/ADCS, MariaDB i Apache. Zakres rzeczywistej weryfikacji: [Testowanie](TESTING.md).

Publiczne limity rejestracji, sprawdzanie wyciekłych haseł, uwierzytelniona aktywacja, weryfikacja/reset hasła, powiązanie konta z sesją, uprawnienia roli i UI/API kont pozostają dalszą pracą. Przyszły publiczny endpoint musi ograniczać równoczesnych pracowników KDF i zabezpieczać transport przed przyjęciem sekretów. Testy nie wdrażają produkcyjnego huba, bazy hosta ani kont użytkowników.
