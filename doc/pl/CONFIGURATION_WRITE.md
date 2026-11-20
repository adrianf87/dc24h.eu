# Atomowy zapis konfiguracji — beta-0.1.3

Etap rozwojowy rozszerza **oddzielny lokalny proces administracyjny**, a nie działający hub ADC. Przed uruchomieniem `dc24h-account-admin --config /bezwzględna/prywatna/hub.ini` należy zatrzymać hub. Przed startem procesu trzeba wykonać jawną migrację bazy do v10, po uprzednim backupie bazy. INI pozostaje źródłem konfiguracji startowej. Od beta-0.1.4 uwierzytelniony adapter WebAdmin przekazuje żądania zapisu i przywracania do tego lokalnego API; sam nie zapisuje plików.

## API i uprawnienia

Wszystkie żądania przechodzą przez istniejące lokalne gniazdo Unix. Wymagają tokenu procesu, krótkotrwałego grantu administratora, zgodnej sesji i sekretu CSRF. Bramka C++ ponownie sprawdza `configuration.manage` (Administrator lub Owner). ID wykonawcy pochodzi z zatwierdzonego grantu, nigdy z formularza.

- `GET /api/v1/management/configuration`: podgląd wartości zapisanych i startowych procesu administracyjnego oraz rewizji.
- `POST /api/v1/management/configuration`: walidacja propozycji wyłącznie w pamięci.
- `POST /api/v1/management/configuration/write`: formularz URL-encoded zawiera `revision` i od jednego do czterech pól: `network_max_connections`, `network_idle_timeout`, `logging_level`, `general_language`. Sukces zwraca `written:true`, `restart_required:true`, nową `revision` i `backup_available:true`.
- `POST /api/v1/management/configuration/restore`: tylko `revision`; przywraca kopię wskazaną przez serwer, wcześniej zabezpieczając aktualne INI jako kopię cofnięcia.

Żadna trasa nie przyjmuje ścieżki, polecenia systemowego, dowolnego klucza INI ani sekretu. Błędne formularze są odrzucane, niepoprawne wartości dają `422`, konflikty rewizji lub próba ponownego przywrócenia już aktywnej kopii `409`, a niejednoznaczny wynik zapisu `503 outcome_unknown`. Takiego zapisu nie wolno powtarzać bez sprawdzenia plików i audytu. Dotychczasowy kontrakt `management/authorize` nadal nie wykonuje operacji; edytor WebAdmin wprowadzono w beta-0.1.4.

## Bezpieczeństwo plików i odzyskiwanie

Używana jest wyłącznie kanoniczna ścieżka INI ustalona przy uruchomieniu procesu. Katalog nadrzędny i plik muszą należeć do efektywnego UID usługi i nie mogą być zapisywalne przez grupę ani innych. Plik musi być zwykłym plikiem bez dodatkowych dowiązań twardych. Dowiązania symboliczne i niebezpieczne pliki pomocnicze są odrzucane. Prywatny `.dc24h.lock` blokuje równoczesne operacje współpracujących procesów; rewizja i sprawdzenie metadanych wykrywają przestarzały odczyt. Zewnętrzne edytory nieużywające blokady wymagają odrębnej koordynacji.

Przed zapisem pełna poprzednia zawartość INI jest kopiowana i synchronizowana do `<ini>.dc24h.bak` (0600). Nowa, zwalidowana zawartość powstaje w prywatnym pliku tymczasowym w tym samym katalogu; po synchronizacji następuje atomowa zamiana nazwy i synchronizacja katalogu. Komentarze oraz pozostałe klucze pozostają zachowane. Przywracanie sprawdza backup, zapisuje stan sprzed przywrócenia w `<ini>.dc24h.undo` (0600), a następnie atomowo zastępuje główny plik. Gdy główny plik ma już taką samą zawartość jak backup, operacja jest odrzucana bez nadpisania kopii cofnięcia, aby nie utracić poprzedniego stanu. Działający hub otrzymuje zmiany dopiero po kontrolowanym restarcie.

W razie `outcome_unknown` lub awarii procesu należy wstrzymać kolejne zapisy, sprawdzić i zwalidować INI oraz pliki pomocnicze jako właściciel usługi, przejrzeć audyt i dopiero potem przywracać działanie. W tym etapie backup obejmuje jedną poprzednią generację; niezależne kopie operatora pozostają konieczne.

## Baza danych i audyt

Nowa migracja `0010_configuration_audit.sql` poszerza ograniczenia tabeli audytu v9 bez zmiany wcześniej zastosowanego SQL ani rekordów kont i banów. Autoryzowana operacja najpierw zapisuje trwałe zdarzenie `configuration.write` lub `configuration.restore` z `target_kind=configuration`, `target_id=1`, `outcome=started`. Po wykonaniu wynik zmienia się na `succeeded` albo `failed`. Awaria/nieznany rezultat pozostawia `started` do wyjaśnienia. Audyt nie zapisuje treści plików, tokenów, kluczy, haseł ani proponowanych wartości. Backup bazy ze schematu v9 pozostaje możliwy do przywrócenia do pustego schematu v10 po weryfikacji prefiksu historii migracji.

## Ograniczenia etapu

To nie jest zdalne administrowanie, edytor WWW, odczyt aktywnego huba, przeładowanie bez restartu ani kontroler systemd. Cykl życia usługi pozostaje oddzielny. Jest to wersja rozwojowa; nie wolno scalać bez zielonych kontroli CodeQL i C++/WebAdmin.
