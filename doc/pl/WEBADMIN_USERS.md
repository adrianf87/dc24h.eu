# Użytkownicy WebAdmin

alpha-0.8.12 dodaje katalog gotowych sesji huba tylko do odczytu oraz dwujęzyczną stronę WebAdmin z decyzjami moderacyjnymi. Gałąź powstaje z main. Nie jest scalana automatycznie.

`GET /api/v1/users` wymaga tylko tokenu bearer, tak jak status i health. Nie wymaga `X-DC24H-Admin-Grant`. Proces utrzymaniowy nie publikuje tej trasy. Schemat v9 pozostaje bez zmian.

Reaktor kopiuje co najwyżej 64 gotowe sesje do zwykłego katalogu, zanim wątek API odczyta tę kopię. Modele sesji zostają w wątku reaktora. Próbka powstaje w pętli reaktora, więc logowanie może pojawić się w następnym obiegu, a nie w tej samej chwili co powitanie. Późniejsza gotowa sesja, SID inny niż cztery znaki ADC albo nick poza 1..64 bajtami lub ze znakiem sterującym ustawia `truncated` i jest pomijany.

Każdy wiersz ma `session`, `sid`, `nickname`, `transport` (`adc` lub `adcs`), `role`, `public_muted` i `private_muted`. `session` to lokalny identyfikator życia procesu jako ciąg dziesiętny. Rola to `guest`, dopóki sesja nie jest uwierzytelniona. Katalog nie zawiera CID, adresu IP, identyfikatora konta, e-maila, opisu, rozmiaru udziału, hasła, dowodu ani powodu kick. Tych pól nie ma też w `GET /api/v1/status`.

Strona Użytkownicy pokazuje wiersze i ocenia kick, mute oraz unmute tą samą regułą niższej roli co Reguły moderacji, dla roli wybranej w przeglądarce. Decyzja nie wie, czy ta rola należy do wiersza. Strona pokazuje tekst polecenia klienta i go nie wysyła. `+kick`, `+mute` i `+unmute` nadal działają z uwierzytelnionej sesji Direct Connect. Bany konta, CID i IP pozostają na stronie banów. Starsze API bez tej trasy pokazuje katalog jako niedostępny, a nie jako pustą listę.

Ten etap nie obejmuje wdrożenia produkcyjnego, migracji schematu ani zmiany konta.
