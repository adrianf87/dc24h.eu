# Account Management — alpha-0.7.11

**alpha-0.8.2 Operators:** [Spis zespołu i lokalne tworzenie/przekazanie własności](OPERATORS.md) zastępują wcześniejsze ograniczenia bootstrap/przekazania Ownera. Schemat v7 i zwykła ochrona Ownera pozostają.

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

**alpha-0.7.14 WebAdmin Accounts:** [lokalne zarządzanie kontami](WEBADMIN_ACCOUNTS.md) dodaje stronicowanie metadanych, tworzenie wyłączonych kont, aktywację, wyłączenie/reset i zmianę hasła przez osobny program bez listenerów huba. Przed zapisem zatrzymaj hub; operacje chronią hasło administratora i rewizje. Schemat v6 i migawki uwierzytelniania pozostają. CI używa czterech workerów na polecenie; nie czekamy na jego zakończenie.

Etap powstaje z main `c81c126b082c10833d4f85829c948fd2ee125184` i dodaje zaufaną,
synchroniczną bibliotekę C++ `dc24h_account_management`: zmianę hasła konta,
administracyjne wyłączenie oraz reset hasła. Bez automatycznego scalenia, nowej
migracji SQL i publicznych poleceń ADC/HTTP/WebAdmin. Nadal obowiązuje schemat v6
i dotychczasowy scrypt o stałym koszcie.

## Operacje i autoryzacja

| Operacja | Wymagane potwierdzenie i polityka | Wynik |
| --- | --- | --- |
| `change_password(expected, current_password, new_password, changed_at)` | Dokładna świeża migawka aktywnego konta i jego obecne hasło; nowe musi różnić się bajtowo | Nowy solony weryfikator scrypt; status i rola bez zmian; usunięcie hasła ADC |
| `disable_account(actor, actor_password, expected, changed_at)` | Świeża migawka aktywnego administratora i jego hasło; cel registered/moderator, nigdy własne konto lub administrator | Wyłączenie celu; weryfikator scrypt bez zmian; usunięcie hasła ADC |
| `reset_password(actor, actor_password, expected, new_password, changed_at)` | Ta sama polityka administratora; cel może być wyłączony lub nie mieć rekordu scrypt | Nowy weryfikator z losową solą; cel pozostaje wyłączony; usunięcie hasła ADC |

Nazwa, rola, ID, czas utworzenia, historia ostatniego logowania i powiązanie
tożsamości celu pozostają zachowane. Sukces zwiększa rewizję celu dokładnie raz
i ustawia jawny czas UTC. Metadane osobnego administratora nie są zmieniane.
Czas musi mieścić się w zakresie modelu i nie być wcześniejszy niż czas aktualizacji
obu migawek. Wyłączone konto nie może samo zmienić hasła. Administrator może
zmienić własne hasło z jego potwierdzeniem; reset/wyłączenie kont administratorów
przez API personelu jest odrzucane. Etap nie dodaje anonimowego odzyskiwania administratora.

`security::Permission::manage_accounts` jest dostępne wyłącznie administratorowi;
`security::can_manage` odrzuca także cele administratora i nieznane wartości.
Sam numer konta, podana rola lub kopia AuthSession nie są potwierdzeniem.
Obie migawki są ponownie czytane z MariaDB; podrobione/stare metadane, odebranie
roli, wyłączenie i równoczesna zmiana hasła unieważniają wcześniejsze potwierdzenie.
Zaufany adapter nadal musi powiązać żądanie z rzeczywistym wykonawcą i zapewnić
bezpieczny transport, ograniczenia żądań oraz politykę odpowiedzi przed dodaniem
jakiegokolwiek publicznego interfejsu.

Hasła zachowują politykę Registration: minimum 15 znaków Unicode, maksimum
1024 bajty UTF-8, odrzucanie znaków sterujących i dokładne bajty bez normalizacji
lub przycinania. Weryfikacja używa ścisłego parsera scrypt o stałym koszcie
i porównania klucza w stałym czasie. Zastąpienie używa nowej losowej soli 32-bajtowej.
Bez historii haseł i automatycznego generowania sekretów. Tekst jawny należy do
wywołującego; wynik `StoredAccount` i stałe komunikaty błędów nie zawierają sekretów.

Świeże potwierdzenie hasła przy zmianach wrażliwych odpowiada ogólnej zasadzie
[ponownego uwierzytelnienia OWASP](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html#require-re-authentication-for-sensitive-features).
Reset administratora jest operacją serwisową; odzyskiwanie przez e-mail/token
pozostaje osobnym etapem ([OWASP o odzyskiwaniu](https://cheatsheetseries.owasp.org/cheatsheets/Forgot_Password_Cheat_Sheet.html)).

## Transakcje i błędy

Wywołanie sprawdza aktualny/czysty schemat kont, scrypt i ADC przed zapisem;
odrzuca transakcję wywołującego i niewłaściwy wątek połączenia. Kosztowne KDF
wykonuje przed blokadami wierszy. W jednej własnej transakcji:

1. Blokuje wiersze wykonawcy i celu w rosnącej kolejności ID; porównuje pełne
   oczekiwane migawki i rewizje.
2. Blokuje i porównuje dokładny wcześniej sprawdzony rekord hasła wykonawcy,
   łącznie z weryfikatorem i czasem jego utworzenia. Nawet zmiana samego
   weryfikatora bez zwiększenia rewizji konta unieważnia stare potwierdzenie.
3. Aktualizuje metadane/rewizję celu, w razie potrzeby zastępuje weryfikator
   i usuwa jego zaszyfrowane dane ADC.
4. Wykonuje jeden commit i zwraca wcześniej przygotowaną wartość metadanych,
   bez alokacji po zatwierdzeniu. Błąd przed commit wycofuje wszystkie zapisy.

Zapis hasła ADC także zapamiętuje dokładną migawkę konta i weryfikatora
użytą do potwierdzenia i ponownie sprawdza obie pod blokadą przed dodaniem
szyfrogramu. Operacja rozpoczęta ze starym hasłem nie może przywrócić ADC po
zmianie/resetowaniu/wyłączeniu lub równoczesnej zmianie samego weryfikatora.

Prywatne metody składania AccountRepository są dostępne tylko zaufanym usługom;
zwykłe publiczne zapisy repozytorium nadal mają własne transakcje. Kontrole
silnika, kolumn, kluczy i kluczy obcych zachowują atomowość tabel. Migracje
schematu i operacje serwisowe należy serializować; usługa nie wykonuje DDL.

`ManagementError` zawiera stałe kategorie: wejście/hasło, brak autoryzacji, stara
migawka, błędny rekord, kryptografia i schemat. Błędy bazy zachowują istniejące
`outcome_unknown`. Brak automatycznego reconnect i ponawiania zapisów. Po utracie
potwierdzenia commit odczytaj stan z nowego połączenia serwisowego, zanim ustalisz
wynik. Przyszły publiczny adapter musi odpowiednio uogólnić kategorie błędów.

Usługa zeruje własne odczytane/zablokowane weryfikatory, bufor nowego weryfikatora
i bajty parametrów SQL przy sukcesie oraz wyjątku. Bufory wywołującego, biblioteki
SQL/kryptografii, zrzuty systemu i kopie zapasowe pozostają poza tą gwarancją.

## Wymagana przerwa serwisowa i hasła ADC

**Zatrzymaj hub przed tymi operacjami.** Działający proces zachowuje niezmienny
katalog startowy kont/ról/haseł; sam zapis SQL nie zmienia katalogu, nie anuluje
oczekującego GPA i nie rozłącza gniazd. Unieważnienie samej sesji nie blokuje
świeżego logowania wobec starego katalogu. Bez przeładowania na żywo i kolejki
administracyjnych poleceń reaktora w tym etapie.

1. Zatrzymaj hub; użyj osobnego połączenia serwisowego z potrzebnymi uprawnieniami
   SQL i odczytaj świeże migawki kont.
2. Wykonaj operację. Hasło właściciela konta i hasło ADC są oddzielne: każda
   z trzech operacji celowo usuwa stary zaszyfrowany rekord ADC.
3. Po zmianie hasła zapisz świeże, odrębne hasło ADC przez
   `AdcCredentialService::enroll` z nowym hasłem konta i dotychczasowym kluczem.
   Po resecie najpierw jawnie aktywuj konto przez zaufane AccountRepository,
   jeśli taki jest zamiar, a potem zapisz ADC z potwierdzeniem hasła po resecie.
   Samo wyłączenie nie wymaga aktywacji ani ponownego zapisania hasła.
4. Uruchom hub ponownie. Brak danych ADC, konto wyłączone lub niezgodne mapowanie
   oznaczają odmowę logowania bez przejścia na gościa. Klient używa nowo zapisanego
   hasła ADC, a nie hasła właściciela konta.

API pożycza hasła i zwraca tylko metadane:

```cpp
#include "dc24h/accounts/management.hpp"
// connection, current, pozyczone sekrety i czas UTC sa zaufanymi danymi.
auto updated = dc24h::accounts::AccountManagementService(connection)
    .change_password(current, old_secret, new_secret, utc_changed_at);
// Ponowne zapisanie hasla ADC i restart sa osobnymi krokami serwisowymi.
```

Zarządzanie wymaga SELECT metadanych migracji/kont/danych uwierzytelniających,
UPDATE kont, INSERT/UPDATE scrypt dla zmiany/resetu i DELETE rekordów ADC.
Wyłączenie nie zapisuje tabeli scrypt. Usługa nie wymaga klucza odszyfrowania
ani tekstu hasła ADC. Ponowny zapis ADC osobno wymaga dotychczasowego klucza
i uprawnień. Czytnik startowy nie potrzebuje nowych praw zapisu. Brak nowych
argumentów CLI i publicznego endpointu haseł, wysyłania e-mail i logowania sekretów.
Reset nie aktywuje automatycznie użytkownika i nie nadaje roli.

## Weryfikacja

`accounts.management.process` korzysta z prywatnej MariaDB i prawdziwego
scrypt/AES-GCM. Sprawdza błędne potwierdzenie/rolę/czas/wejście, rewizje, zachowanie
ostatniego logowania i mapowań, nową sól, odrzucanie starego hasła, wyłączenie,
reset kont wyłączonych/bez weryfikatora, usunięcie ADC i jawne ponowne zapisanie.
Weryfikuje rollback po odmowie DELETE już po wcześniejszych zapisach, zagnieżdżoną
transakcję, błędny schemat, odebranie roli wykonawcy, wyścig samego weryfikatora
oraz dwa równoczesne resety, z których dokładnie jeden zmienia rewizję.

Przegląd błędów alokacji używa wyłącznie testowego zastępnika KDF i sprawdza
niezmienione wiersze konta/weryfikatora/ADC po rollback oraz zerowanie buforów.
Produkcja nie ma trybu obniżonego kosztu KDF. Końcowe konto po resecie jest jawnie
aktywowane i otrzymuje świeże hasło ADC; prawdziwy ncdc sprawdza odmowę błędnego
hasła i poprawne ADC/ADCS, reconnect, powitanie i czat. Testy uwierzytelniania,
ról, repozytorium oraz rejestracji pozostają włączone.

[GitHub CI](GITHUB_CI.md) uruchamia Debug/Release i WebAdmin na Ubuntu 24.04
z `nproc`, bez czekania na wynik. Wyniki lokalne i ograniczenia opisują
[Testowanie](TESTING.md) i [Changelog](../../CHANGELOG.md).
