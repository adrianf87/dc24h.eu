# Identity Mapping — alpha-0.7.6

**Rozszerzenie alpha-0.7.7:** [Password Security](PASSWORD_SECURITY.md) dodaje sprawdzenie danych uwierzytelniających. Samo mapowanie ani samo dopasowanie hasła nadal nie przyznają uprawnień sesji/konta. Historyczne plany poniżej opisują pierwotny etap alpha-0.7.6.

Etap rozpoczyna się bezpośrednio z main `d279245e3636d631c159ee8df53623c293fb36e8`.
Zachowuje Database Core, migracje, model i repozytorium kont oraz rejestrację.
Branch nie jest automatycznie scalany z main.

## Kontrakt

Dołącz bibliotekę `dc24h_identity_mapping` i nagłówek
`dc24h/accounts/identity_mapping.hpp`. `IdentityRepository` pożycza połączenie
Database Core, używane wyłącznie w jego wątku właściciela. Konstruktor nie
wykonuje SQL. Operacje są synchroniczne i muszą działać poza reaktorem Network.

To zaufane API zarządzania. Wywołujący odpowiada za autoryzację przypisania
tożsamości do konta. Nie dodano komendy ADC, trasy HTTP, akcji WebAdmin ani CLI
przyjmujących niezaufane żądania przypisania. Poprawny składniowo CID, nawet
z potwierdzoną relacją do PID, nie uwierzytelnia konta. Odczyt zwraca metadane;
nie zmienia sesji, statusu konta, roli, ostatniego logowania ani uprawnień.
Dotychczasowe logowanie gości ADC/ADCS pozostaje bez zmian.

| Operacja | Wynik |
| --- | --- |
| `IdentityKey::create(nickname, cid)` | Własna, sprawdzona para albo `nullopt`. |
| `bind(account_id, key, created_at)` | Niezmienne przypisanie zapisane we własnej transakcji; własny `StoredIdentity` zwracany po commit. |
| `find_by_account(id)` | Opcjonalne przypisanie trwałego ID konta. |
| `find_by_nickname(nickname)` | Dokładny odczyt po bajtach nicka; błędne dane są odrzucane. |
| `find_by_cid(TigerCid)` | Opcjonalny odczyt po kanonicznym CID. |
| `resolve(key)` | Wynik tylko wtedy, gdy **nick i CID** pasują do tego samego rekordu. Bez zastępczego dopasowania jednym kluczem. |

Jedno konto ma najwyżej jedno przypisanie. Nick i CID są osobno unikalne
między kontami. Ponowne przypisanie, również identyczne, oznacza konflikt.
Nie ma nadpisania, przeniesienia, automatycznego tworzenia konta ani przejęcia
tożsamości. Zmiana, cofnięcie i wiele tożsamości na konto to następne etapy,
wymagające jawnej polityki autoryzacji i konfliktów.

Nick jest odrębny od prywatnej nazwy rejestracyjnej konta. Musi być niepustym,
już znormalizowanym NFC tekstem UTF-8 do 64 bajtów, zgodnym z istniejącą
polityką publicznego tekstu ADC: bez znaków sterujących, formatujących,
separatorów linii/akapitów i spacji ASCII. Bajty i wielkość liter są zachowane:
`Alice` i `alice` są różne. Nie ma przycinania ani niejawnej normalizacji.
Parser `adc::TigerCid` wymaga 39 wielkich znaków base32 dla dokładnie 192 bitów,
z kanonicznymi bitami dopełnienia. PID, SID, adresy, weryfikatory i hasła nie
są parametrami ani polami rekordu. Obiekty po przeniesieniu są odrzucane.
Czas utworzenia jest jawną sekundą Unix UTC w zakresie 0–253402300799;
nie ma ukrytego zegara ani domniemanego logowania konta.

## Baza i błędy

Jawna migracja **v5** tworzy InnoDB `dc24h_account_identities` z kluczem
głównym `account_id BIGINT`, `nickname VARBINARY(64)`, `cid VARBINARY(39)`
i `created_at BIGINT`. Pełne unikalne indeksy nicka i CID oraz restrykcyjny
klucz obcy do kont chronią przypisania. CHECK ograniczają długości, postać CID,
dodatnie ID i czas. Bajty migracji 1–4 pozostają identyczne. Istniejące konta
i dane uwierzytelniające są zachowane; aktualizacja nie zgaduje tożsamości
na podstawie nazw kont.

Po kopii zapasowej i zatrzymaniu zapisów operator uruchamia istniejące
`--migrate-database` swoim kontem migracyjnym. Baza v4 stosuje jedną migrację
i raportuje `version=5 latest=5 pending=0 dirty=no`; pusta baza stosuje pięć.
Zwykły start i wywołania repozytorium nie wykonują migracji. Trwały dziennik
applying/applied nadal blokuje zgadywanie po częściowym DDL lub zastanej tabeli:
stan dirty wymaga analizy operatora, bez automatycznego ponowienia.

Operacje wymagają aktualnego, czystego katalogu i zgodnych tabel kont oraz
przypisań. Sprawdzają silnik, dokładne kolumny, pełne unikalne indeksy i klucz
obcy do właściwego konta. To ograniczona kontrola zgodności, a nie audyt
triggerów, treści CHECK, wartości domyślnych, ingerencji administratora lub
akcji referencyjnych ani równoczesnego DDL. Odczyt metadanych nie wymaga
szerokich uprawnień ujawniających akcje referencyjne. Oddziel konta migracyjne
od wykonawczych. Odczyt wymaga
SELECT na używanych tabelach, zapis dodatkowo INSERT na tabeli przypisań.
Samo API nie wymaga UPDATE, DELETE, DDL ani dostępu do tabeli weryfikatorów.

Przypisanie blokuje istniejący rekord konta i zapisuje parametryzowany INSERT
we własnej transakcji z rollback przy wyjściu. Brak konta jest odrzucany,
częściowy wynik nie jest publikowany. Wynik jest alokowany przed zmianą SQL
i przenoszony bez alokacji po commit. Transakcja wywołującego i obcy wątek są
odrzucane. Błędy Database Core zachowują kody numeryczne i `outcome_unknown`;
po utracie potwierdzenia należy sprawdzić stan nowym połączeniem zamiast
automatycznie powtarzać zapis.

`MappingErrorKind` rozróżnia błędną tożsamość/czas, brak konta, konflikt,
błędny rekord i niezgodny schemat. Stałe komunikaty EN/US nie zawierają nicka,
CID ani SQL. Błędy schematu kont, migracji i Database Core mogą być propagowane.
Rekordy zaimportowane przez SQL są walidowane przy odczycie.

## Weryfikacja i granice

`accounts.identity_mapping` sprawdza granice nicka/CID, Unicode, dopełnienie
base32, wielkość liter i obiekty po przeniesieniu.
`accounts.identity_mapping.process` uruchamia prywatną MariaDB bez sieci:
trwałość, dokładne pary i odrzucenie skrzyżowanych kluczy, duplikaty, brak konta,
czasy graniczne, niezmienione metadane kont, ograniczone uprawnienia,
własność wątku/transakcji, wyścig zapisów, zmiany indeksu/klucza obcego,
uszkodzony CID i rollback przy błędach alokacji aż do udanego zapisu.
Testy migracji obejmują zachowanie danych v4→v5 i odmowę powtarzania dirty.
Dotychczasowe testy ncdc ADC/ADCS, MariaDB, Apache i WebAdmin oraz Debug/Release
pozostają. Faktyczne wyniki opisują
[Testowanie](TESTING.md) i
[Changelog](../../CHANGELOG.md); wystartowane CI nie oznacza sukcesu.

Weryfikacja hasła, uwierzytelnione przypisanie sesji, cofanie tożsamości,
logowanie kont, przyznawanie ról i UI/API kont pozostają kolejnymi etapami.
Pełna lista Debian/Ubuntu jest celem wsparcia; zdalne CI tej zmiany używa
Ubuntu 24.04. Nie migrujemy bazy produkcyjnej ani nie wdrażamy huba.

Źródła: MariaDB [ciągi binarne](https://mariadb.com/docs/server/reference/data-types/string-data-types/varbinary)
i [klucze obce](https://mariadb.com/docs/server/ha-and-performance/optimization-and-tuning/optimization-and-indexes/foreign-keys).
