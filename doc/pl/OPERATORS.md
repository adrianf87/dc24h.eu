# Operatorzy i własność huba — alpha-0.8.2

**alpha-0.8.5 Ban Core:** [Trwałe blokady wejścia kont/CID/IP](BAN_CORE.md) dodają jawny schemat v8, dodawanie/cofanie z hasłem, egzekwowanie ze startu i wygaśnięcie. Admin Core ma dziesięć poleceń; users.ban/blocks.manage działają. Zastępuje wcześniejsze opisy v7/planowanych banów.

**alpha-0.8.4 Kick:** [Wyrzucanie aktywnych sesji](KICK.md) dodaje prywatne polecenie +kick, hierarchię, komunikaty końcowe, natychmiastowe odcięcie routingu i liczniki. Operacje serwisowe i schemat v7 pozostają.

**alpha-0.8.3 Admin Core:** [Centralny silnik poleceń serwisowych](ADMIN_CORE.md) obsługuje API kont i CLI Ownera, izolację punktów dostępu, wyniki, prywatny katalog i liczniki procesu. Dowody haseł, transakcje i schemat v7 pozostają.

Etap rozwija [Uprawnienia](PERMISSIONS.md) z main. Do zespołu należą Moderator,
Operator, Administrator i Owner. Uprawnienia pochodzą ze sprawdzonej roli konta
i uwierzytelnienia, nigdy z INF klienta ani wartości wybranej w przeglądarce.
Registered/VIP pozostają poza spisem. Wyłączone konta zespołu są widoczne, lecz
nie mogą się logować ani zatwierdzać operacji. Spis nie oznacza obecności online.

## Cykl życia zespołu

Prywatne serwisowe API/WebAdmin pozwala utworzyć wyłączone konto Registered,
zmienić rolę i po weryfikacji aktywować konto. Administrator nadaje Moderator,
Operator i niższe role; Owner może dodatkowo nadać Administrator. Cel i nadawana
rola muszą być ściśle niższe od wykonawcy. Zmiana roli wyłącza konto i usuwa
poświadczenie ADC. Po aktywacji nadaj nowe, osobne hasło ADC i uruchom hub ponownie.

Strona Operatorzy wywołuje `GET /api/v1/accounts?staff_after=0`, następnie korzysta
z kursora `next_after`. SQL filtruje role przed ograniczeniem strony do 25 kont,
więc zwykłe konta nie ukrywają późniejszych operatorów. Odpowiedź ma
`maintenance: true`, `staff_only: true` i dotychczasowe bezpieczne metadane.
Adapter i przeglądarka odrzucają odpowiedzi starszego API bez filtra, duplikaty,
nieuporządkowane ID i niewłaściwe role. Nie eksportują poświadczeń. Odczyt nadal
wymaga prywatnego tokena Bearer i gniazda Unix z tym samym UID. Nie powstaje
publiczny spis zespołu ani zdalne logowanie kont w panelu.

Wybór operatora udostępnia dotychczasowe operacje roli, aktywacji, wyłączenia,
resetu i własnego hasła. Serwer ponownie sprawdza uprawnienia niezależnie od UI.
Degradacja poza zespół usuwa wiersz z tego widoku. Awans osoby spoza zespołu
wykonaj w Kontach. Formularz nie nadaje ani nie odbiera Ownera.

## Lokalne narzędzie właściciela

`dc24h-owner` instaluje się obok `dc24h` i `dc24h-account-admin`. Nie otwiera portów
huba/API, nie wykonuje migracji i wypisuje wyłącznie stałe komunikaty oraz
ID/rewizje. Czyta istniejącą prywatną konfigurację INI: baza włączona,
uwierzytelnianie ADC wyłączone, aktualny czysty schemat v7. Ten etap nie zmienia
plików migracji, manifestu ani schematu kont.

Zatrzymaj **wszystkie** instancje huba i procesy zapisujące dane serwisowe.
`--confirm-stopped` potwierdza wykonanie tej czynności; nie zatrzymuje ani nie
wykrywa innych procesów. Katalogi uwierzytelniania są niezmienne od startu,
więc istniejące sesje nie otrzymują nowych ról. Konfigurację, hasła i katalogi
utrzymuj pod zaufanym UID usługi. Plik hasła musi być zwykły, należeć do tego
samego UID, mieć prywatne prawa 0600/0400, jedno dowiązanie i nie być symlinkiem.
Hasła nie są wartościami argumentów procesu. Narzędzie czyści własne bufory
haseł i weryfikatorów. Nie wypisuje haseł, nazw kont, ścieżek ani diagnostyki SQL.

Minimalna konfiguracja serwisowa (dostosuj prywatne ścieżki lokalnie):

```ini
[general]
language = us
[database]
enabled = on
socket = /run/mysqld/mysqld.sock
name = dc24h
user = dc24h
password_file = /etc/dc24h/database.password
[authentication]
enabled = off
[api]
enabled = off
```

Istniejący użytkownik SQL potrzebuje SELECT, INSERT, UPDATE i DELETE na właściwych
tabelach kont/poświadczeń. Narzędzie nie nadaje grantów. Hasła kont umieść
w osobnych prywatnych plikach; obowiązuje minimum 15 znaków i dotychczasowa
polityka kont. Końcowe LF/CRLF jest usuwane. `[general] language = pl` włącza
polskie komunikaty wyniku; klucze pól i opcje pozostają EN/US. `--help` opisuje
oba polecenia, a `--version` nie czyta konfiguracji ani nie łączy się z SQL.

### Pierwszy Owner

```sh
dc24h-owner --config /etc/dc24h/owner-maintenance.ini \
  --bootstrap HubOwner --password-file /etc/dc24h/owner-account.password \
  --confirm-stopped
```

Bootstrap tworzy **nowego aktywnego Ownera** z weryfikatorem scrypt. Odrzuca
istnienie dowolnego Ownera, również wyłączonego. Nie awansuje istniejącego konta,
nie resetuje zapomnianego hasła i odrzuca zajętą nazwę. Nieblokująca blokada
nazwana MariaDB zależna od bazy szereguje bootstrap/przekazanie własności.
Dwie równoczesne próby nie mogą utworzyć dwóch pierwszych Ownerów. Zaufane
bezpośrednie zapisy SQL pozostają poza tym protokołem i muszą być zatrzymane.

Konto i weryfikator powstają atomowo. Nowy Owner nie otrzymuje tożsamości ADC,
hasła ADC ani sesji. Przed logowaniem do huba jawnie powiąż sprawdzoną tożsamość
i nadaj osobne hasło ADC przez istniejące zaufane interfejsy `IdentityRepository`
i `AdcCredentialService`, następnie uruchom hub. Nadawanie ADC w przeglądarce
i ogólne narzędzie kont dla użytkownika pozostają planowane. Hasło konta może
od razu zatwierdzać serwisowe zarządzanie kontami/zespołem. Bootstrap nie jest
udostępniony przez HTTP, WebAdmin ani polecenia klienta ADC.

### Przekazanie własności

Przygotuj aktywnego Administratora jako następcę przez sprawdzony cykl zespołu,
pobierz aktualne ID/rewizje i zatrzymaj serwis. Aktywny Owner oraz aktywny
Administrator potwierdzają własne hasła kont z osobnych prywatnych plików:

```sh
dc24h-owner --config /etc/dc24h/owner-maintenance.ini \
  --transfer 1 --revision 3 --to 2 --to-revision 5 \
  --password-file /etc/dc24h/owner-account.password \
  --target-password-file /etc/dc24h/successor-account.password --confirm-stopped
```

Transakcja ustanawia następcę **aktywnym Ownerem**, a poprzedniego właściciela
**wyłączonym Administratorem**, podnosi obie rewizje i usuwa oba poświadczenia
ADC. Zachowuje hasła kont, metadane utworzenia/logowania i mapowania tożsamości.
Przed logowaniem następcy jawnie nadaj nowe hasło ADC. Po sukcesie pozostaje
aktywny Owner; nie ma pośredniego zatwierdzonego stanu bez właściciela.
Wielu wcześniejszych Ownerów utworzonych zaufanym SQL pozostaje zachowanych;
operacja zastępuje tylko potwierdzonego wykonawcę i nie zmienia liczby Ownerów.
Nie scala takich kont ani nie odzyskuje do nich dostępu.

Hasła są sprawdzane przed blokadami; kompletne metadane i dokładne rekordy
poświadczeń są weryfikowane ponownie pod blokadami kont w kolejności ID.
Błędne hasła, stare rewizje, wskazanie siebie, wyłączony wykonawca, wykonawca
inny niż Owner oraz następca inny niż aktywny Administrator oznaczają odmowę.
Zwykłe operacje nadal chronią Ownera przed degradacją, wyłączeniem i resetem.
Zmiana własnego hasła aktywnego konta zachowuje osobną ścieżkę dowodu.
Nie ma wymuszonego przekazania ani obejścia odzyskiwania.

Błąd SQL wycofuje obie zmiany ról i oba usunięcia ADC. Narzędzie nie łączy się
ponownie ani nie ponawia zapisów. Nieznany wynik potwierdzenia SQL daje kod 3
i komunikat nakazujący inspekcję przed ponowieniem; zwykła odmowa daje 1, sukces 0.
Po nieznanym wyniku najpierw sprawdź aktualne metadane/bazę. Migracje, wdrożenia,
utworzenie produkcyjnego Ownera i zmiany kont produkcyjnych są jawnymi czynnościami
operatora, a nie krokami publikacji wydania.

## Weryfikacja i dalszy zakres

Testy natywne obejmują równoczesny bootstrap, wyłączonego istniejącego Ownera,
oba hasła, hierarchię, stare dane i wyścig poświadczeń, wycofanie obu zmian przy
braku DELETE, rewizje, zachowanie sekretów, unieważnienie ADC i stronicowanie
zespołu mimo luk zwykłych kont. Rzeczywisty CLI działa w testach z tymczasową
MariaDB: bootstrap, przekazanie, stare żądanie, prawa plików i redakcja wyjścia.
Prywatne HTTP, adapter Node i Chromium sprawdzają filtrowany spis, starsze API,
degradację i polski widok mobilny. Pozostają testy rzeczywistego ncdc ADC/ADCS.

Mute, bany/blokady, przeglądanie audytu, polecenia własności na żywo i zdalne
przekazanie własności w przeglądarce pozostają planowane. Same uprawnienia
alpha-0.8.1 nie implementują tych silników. Lokalnie testy używają ośmiu wątków.
CI GitHub zachowuje Ubuntu 24.04 i cztery logiczne CPU na runner z równoległością
wynikającą z nproc. Nie oczekujemy na zakończenie zdalnego CI.
