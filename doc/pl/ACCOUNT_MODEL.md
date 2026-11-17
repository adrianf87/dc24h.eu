# Model konta — alpha-0.7.3

**alpha-0.8.2 Operators:** [Spis zespołu i lokalne tworzenie/przekazanie własności](OPERATORS.md) zastępują wcześniejsze ograniczenia bootstrap/przekazania Ownera. Schemat v7 i zwykła ochrona Ownera pozostają.

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

**alpha-0.7.10 Roles:** [sprawdzone role kont](ROLES.md) są teraz przypisane do gotowych sesji, nadają uprawnienia Search/Transfers i pozwalają personelowi unieważniać sesje niższych ról. Limity globalne i jawne blokady połączeń nadal obowiązują. Zmiany roli w bazie wymagają restartu; bez nowej migracji SQL i publicznego API administracji.

Poniżej zachowano historyczny zakres alpha-0.7.3. Zapis wprowadzony w alpha-0.7.4 opisuje [Repozytorium kont](ACCOUNT_REPOSITORY.md).

Etap oparty bezpośrednio na main `aabe5ddcbc47a4c65a5069b077fd0b5e71a9281c`, obejmującym Database Core, Database Migrations i poprawkę CI bez sudo w zadaniach.

## Przeznaczenie i własność danych

`include/dc24h/accounts/account.hpp` i `src/accounts/account.cpp` tworzą niezależną bibliotekę C++20 `dc24h_accounts`. `accounts::Account` to zweryfikowane, samodzielne metadane trwałego konta. Model jest oddzielony od `users::User`, opisującego publicznego gościa na jednym połączeniu. ID konta nie jest SID/CID protokołu ADC ani `users::SessionId`. Nie ma automatycznej konwersji ani powiązania.

Model nie zawiera poświadczeń, weryfikatora hasła, wyniku uwierzytelnienia ani nadania uprawnień. Rola `administrator` w odłączonej wartości nie daje uprawnień sesji. Dotychczasowe logowanie gości ADC/ADCS i źródła uprawnień pozostają bez zmian. Zapis kont, migracje SQL aplikacji, wymuszanie unikalności, rejestracja, haszowanie/weryfikacja poświadczeń, uwierzytelnione powiązanie konta z sesją i administracja pozostają kolejnymi etapami.

## Kontrakt danych

| Pole | Zasady |
| --- | --- |
| `AccountId` | Dodatnia liczba od 1 do 9223372036854775807. Zero i wartości powyżej zakresu signed BIGINT są odrzucane. ID nada przyszłe repozytorium, nie model. |
| `AccountName` | 1–64 **bajty**, poprawne UTF-8 już w NFC. Odrzucane są błędne kodowanie, NUL, znaki sterujące, formatujące i separatory Unicode, w tym twarde spacje. Bez normalizacji, obcinania i zmiany wielkości liter. |
| `Role` | Wyłącznie `registered`, `moderator` albo `administrator`. Klucze danych są stałe, w EN/US i rozróżniają wielkość liter; nie są etykietami interfejsu. Nieznane wartości są odrzucane. |
| `Status` | Wyłącznie `disabled` albo `active`. Nieznane wartości/liczby enum są odrzucane. Domyślny import oznacza konto wyłączone i zwykłego zarejestrowanego użytkownika. |
| `Times` | Sekundy Unix UTC ze znakiem, od 0 do 253402300799 (1970-01-01 do 9999-12-31T23:59:59Z). `created_at <= updated_at`, a opcjonalne `last_login_at` należy do tego przedziału. Zero epoki różni się od braku daty logowania. |

Nazwy zachowują bajty i wielkość liter: `Alice` i `alice` to różne wartości. Podobne wizualnie znaki nie są łączone. Model nie gwarantuje globalnej unikalności ani bezpieczeństwa jako fragment SQL, HTML lub nazwa pliku. Przyszłe repozytorium musi świadomie dobrać reguły unikalności/porównywania i przekazywać nazwy jako parametry zapytań przygotowanych. Ograniczenia publicznego pseudonimu protokołu należą do adaptera.

Daty są jawnymi danymi wejściowymi; biblioteka nie odczytuje zegara i nie przyjmuje lokalnej strefy czasowej. Daty przyszłe mieszczące się w zakresie są strukturalnie poprawne — ocenę zegara i polityki wykonuje wywołujący. Konwersja na typ SQL należy do przyszłego adaptera. Zmiana metadanych nie potwierdza rzeczywistego logowania.

## Tworzenie i kandydaci zmian

`AccountData` jest niezaufaną strukturą importu/migawki z własnymi danymi. `Account::create` zwraca `std::variant<Account, AccountError>` po sprawdzeniu ID, enum, dat i nazwy. Błędy zawierają wyłącznie kategorie `invalid_id`, `invalid_name`, `invalid_role`, `invalid_status`, `invalid_times`, bez kopiowania wejścia do diagnostyki. Zweryfikowanych kont, nazw i ID nie można utworzyć konstruktorem domyślnym.

```cpp
#include "dc24h/accounts/account.hpp"
using namespace dc24h::accounts;

AccountData input{42, "Żółw", Role::registered, Status::disabled,
                  Times{1700000000, 1700000000, std::nullopt}};
auto result = Account::create(input);
if (auto* account = std::get_if<Account>(&result)) {
    auto candidate = account->with_access(Role::registered, Status::active,
                                          1700000001);
    // Sprawdź wynik i zatwierdź go w przyszłej zaufanej usłudze/repozytorium.
    // Pierwotne konto i podłączeni użytkownicy pozostają bez zmian.
}
```

`with_name`, `with_access` i `with_last_login` zwracają nowych, zweryfikowanych kandydatów. Zachowują ID i datę utworzenia, wymagają daty zmiany nie wcześniejszej od poprzedniej i zachowują pozostałe metadane. Równe daty są dopuszczalne. `with_last_login` aktualizuje daty ostatniego logowania i zmiany; jest wyłącznie metadanymi historycznymi, także dla kont wyłączonych. Nie uwierzytelnia, nie autoryzuje i nie włącza konta.

`snapshot()` zwraca niezależne `AccountData`; jego zmiana nie wpływa na konto. Przypisanie kopiujące przygotowuje kompletną wartość tymczasową przed zamianą, więc błąd alokacji nie zmienia celu. Błąd alokacji jest przekazywany jako `std::bad_alloc`, a nie błąd walidacji. Długości wejścia sprawdzane są przed normalizacją/kopiowaniem. Konto/nazwa po przeniesieniu są niepoprawne do ponownego przypisania; `valid()` i metody zmian je odrzucają. Akcesory są tylko do odczytu. Biblioteka nie wykonuje I/O i nie zależy od SQL, Network ani ADC.

Są to odłączone wartości, nie transakcje bazy ani kontrola wersji. Usługa zatwierdzająca musi zapewnić autoryzację, unikalność i ochronę przed równoczesnymi zmianami. Poświadczenia wymagają osobnego, sprawdzonego komponentu i nie mogą trafić do publicznych migawek.

## Budowanie i weryfikacja

Zwykły build CMake obejmuje `dc24h_accounts`; ten target linkują odbiorcy natywni. Używa istniejących zależności text/utf8proc, bez nowego pakietu.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^accounts.model$' --output-on-failure
```

`accounts.model` sprawdza konstrukcję typów, zakres ID, bajtowe granice Unicode/NFC, odrzucanie znaków sterujących/separatorów, stałe klucze ról/stanów, nieznane wartości, brak/epokę/zakres/kolejność dat, własność migawek, niezmienność kandydatów, przeniesienia/przypisania do siebie i błędy alokacji podczas tworzenia, eksportu, zmian oraz przypisania. Dotychczasowe zestawy CTest, w tym rzeczywisty ncdc ADC/ADCS, MariaDB i Apache, pozostają włączone. Rzeczywiste wyniki opisują [Testowanie](TESTING.md) i [Changelog](../../CHANGELOG.md). Nie czekamy na zdalny Check status.

Wszystkie zadania GitHub Actions nadal używają własnego runnera Ubuntu 24.04. Przygotowanie zależności wykonuje jawnie administrator; zadania działają bez sudo. Zobacz [przygotowanie CI](CI_DEBIAN13.md).
