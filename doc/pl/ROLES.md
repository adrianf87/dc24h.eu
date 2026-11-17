# Roles — alpha-0.7.10

**alpha-0.8.2 Operators:** [Spis zespołu i lokalne tworzenie/przekazanie własności](OPERATORS.md) zastępują wcześniejsze ograniczenia bootstrap/przekazania Ownera. Schemat v7 i zwykła ochrona Ownera pozostają.

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

**alpha-0.7.11 Account Management:** [zaufana zmiana hasła, wyłączenie i reset](ACCOUNT_MANAGEMENT.md) łączą atomowy zapis konta/haseł ze świeżym potwierdzeniem, rewizją i usunięciem danych ADC. Wykonuj przy zatrzymanym hubie, następnie jawnie zapisz ADC/aktywuj konto według potrzeby i uruchom hub ponownie. Bez przeładowania na żywo, nowej migracji SQL i publicznego API zarządzania.

Etap powstaje z main `4390bd0ab8f4680f4ece63afb1354db66aaff9e9` i aktywuje
istniejące role kont `registered`, `moderator`, `administrator` w uwierzytelnionych
sesjach ADC/ADCS. Nie scala automatycznie gałęzi z main. Bez nowej migracji SQL;
schemat v6 i dane uwierzytelniające pozostają bez zmian.

## Zaufane powiązanie roli

Niezmienny katalog startowy `AdcAuthenticationDirectory` przechowuje sprawdzoną
rolę konta odczytaną z MariaDB. `PasswordAuthentication` przekazuje ją jako dane
zaufanego adaptera. Oczekujące GPA i sam poprawny PAS nie nadają jeszcze aktywnej
roli: dopiero ostatni LF własnego BINF zapisuje gotowość i sprawdzoną rolę w
`users::AuthSession`. Gość nie ma roli konta. Nieznane wartości enum są odrzucane
przez politykę, adapter uwierzytelniania i przejście gotowości, bez zmiany modelu.

Nazwa konta, nick, CID/SID, deklaracje SUP i pola INF klienta nie wybierają roli.
Odrzucanie deklaracji przywilejów klienta pozostaje aktywne. Etap nie dodaje
publicznych znaczników CT ani poleceń operatora. [Specyfikacja ADC](https://dc-protocols.github.io/ADC.html)
definiuje metadane protokołu i GPA/PAS; poniższa macierz jest wewnętrzną polityką
huba, a nie deklaracją uprawnień przesyłaną przez klienta.

## Macierz uprawnień

Operacje trwałe opisuje [Account Management](ACCOUNT_MANAGEMENT.md).

`dc24h_roles` udostępnia bezalokacyjne `security::allows(Role, Permission)` oraz
`security::can_revoke(actor_role, target_role)`. Role i uprawnienia są sprawdzane
jawnie, bez porównywania numerów enum i bez domyślnego zezwolenia.

| Operacja | Gość | registered | moderator | administrator |
| --- | --- | --- | --- | --- |
| Wysyłanie zapytań/wyników wyszukiwania | Polityka gości z konfiguracji | Tak | Tak | Tak |
| Inicjowanie/odbieranie CTM lub RCM | Polityka gości z konfiguracji | Tak | Tak | Tak |
| Trwały reset/wyłączenie kont registered/moderator | Nie | Nie | Nie | Świeże potwierdzenie hasła administratora |
| Unieważnienie sesji registered | Nie | Nie | Tak | Tak |
| Unieważnienie sesji moderatora | Nie | Nie | Nie | Tak |
| Unieważnienie sesji administratora | Nie | Nie | Nie | Nie |
| Unieważnienie własnej sesji przez API personelu | Nie | Nie | Nie | Nie |

Przy gotowości uwierzytelnionej sesji hub ustawia istniejące uprawnienia Search
i Transfers na podstawie roli. Goście zachowują ustawienia konfiguracyjne,
w tym dotychczasowy domyślnie włączony dostęp. Aby ograniczyć wyszukiwanie
i transfery do uwierzytelnionych kont, ustaw:

```ini
[search]
guest_queries = off
guest_results = off
[transfers]
guest_initiate = off
guest_receive = off
```

Globalne wyłączenia funkcji, lista protokołów, składnia/źródło/tożsamość,
wymagania aktywnego/pasywnego klienta, limity wyjścia/użytkowników/tokenów oraz
limity ruchu nadawcy i odbiorcy obowiązują każdą rolę. Administrator ich nie omija.
Czat zachowuje wspólną dotychczasową politykę i limity. Jawne zaufane zmiany
`set_search_permissions` / `set_transfer_permissions` obowiązują po gotowości do
końca połączenia; SUP, INF i zwykły ruch ich nie resetują. Odebranie prawa zapytań
nadal usuwa okna wyszukiwania. Nadanie roli nie odnawia kredytów ograniczeń ruchu.

## Operacje personelu i sprawdzanie bieżącej sesji

`LoginHub::role_allows(id, permission)` odczytuje uprawnienie roli konkretnej
gotowej sesji konta. Brak ID, zero, gość, oczekiwanie i zamykanie oznaczają odmowę.
Jest to wyłącznie odczyt polityki roli, a nie zamiennik kontroli faktycznej operacji.

`LoginHub::revoke_auth_session_as(actor, target, now)` jest zaufanym API adaptera
w wątku właściciela. Wywołujący musi pobrać ID wykonawcy z faktycznego kontekstu
uwierzytelnionego połączenia, nigdy z treści żądania ani dowolnego konta/SID.
Hub ponownie odczytuje obie bieżące sesje, sprawdza hierarchię, odrzuca własną,
równą i wyższą rolę oraz cofnięty czas, a następnie bez alokacji unieważnia
połączenie. Wykonawca musi nadal być gotowy. Powtórzone i stare ID nie mogą
wpłynąć na zastępcze połączenie. Kopie sesji i ról są danymi historycznymi,
a nie upoważnieniem. Zamykanie usuwa rolę razem z kontem i natychmiast blokuje
kolejne operacje wymagające roli.

Dotychczasowe `revoke_auth_session(target)` pozostaje uprzywilejowanym API kodu
zarządzającego hubem; nie jest publicznym API autoryzacji klienta. Etap nie dodaje
polecenia kick ADC, zarządzania rolami przez HTTP/WebAdmin, sesji przeglądarkowej,
bana całego konta ani trwałego edytora ról. Prywatny token API nie identyfikuje
konta ADC i nie nadaje jego roli.

## Nadawanie i zmiana roli

Role znajdują się w dotychczasowej tabeli kont. Zaufany kod administracyjny może
użyć `Account::with_access(role, status, changed_at)` i `AccountRepository::update`
z kontrolą rewizji; wywołujący musi autoryzować tę operację. SQL, scrypt i zapis
haseł pozostają poza reaktorem sieciowym. Rejestracja tworzy nadal wyłączone konta
registered i nie przyjmuje publicznego żądania nadania roli.

Rola, status, mapowanie i dane uwierzytelniające są migawką startową. Po zmianie
trwałej polityki dostępu zrestartuj hub. Dotychczasowe połączenie zachowuje rolę
do zamknięcia; nowo załadowany katalog odzwierciedla zmianę. Unieważnienie jednej
sesji nie blokuje świeżego poprawnego logowania wobec niezmienionego katalogu.
Role niestandardowe, edycja/przeładowanie uprawnień na żywo i trwały audyt
moderacji pozostają kolejnymi etapami.

## Weryfikacja

`security.roles` sprawdza całą macierz ról/uprawnień i hierarchię, nieznane wartości,
atomowe odrzucanie błędnej roli, publikację po ostatnim LF, izolację gości,
gotowość/zamykanie/stare ID, unieważnienie przy błędzie alokacji, rzeczywisty
routing LoginHub, nadrzędność wyłączeń globalnych i jawnych blokad sesji.
Dotychczasowe testy sesji pozostają włączone.

`accounts.adc_authentication.process` sprawdza role z prawdziwej MariaDB,
migawkę starego katalogu i nowy odczyt roli, odmowę dla wyłączonego personelu,
routing registered/moderatora przy wyłączonych operacjach gości, globalne
ograniczenia administratora przez TCP/TLS, odrzucanie deklaracji przywilejów
i izolację sekretów. Zachowuje testy prawdziwego ncdc: błędne/poprawne hasło,
reconnect, powitanie i czat przez ADC/ADCS. Bez danych i bazy produkcyjnej.

Wyniki lokalne opisują [Testowanie](TESTING.md) i [Changelog](../../CHANGELOG.md).
[GitHub CI](GITHUB_CI.md) uruchamia Debug/Release i WebAdmin na Ubuntu 24.04,
z pełną liczbą CPU przez `nproc`; nie czekamy na jego zakończenie.
