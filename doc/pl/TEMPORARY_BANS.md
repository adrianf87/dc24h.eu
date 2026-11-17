# Temporary Bans — alpha-0.8.6

Etap rozwija [Ban Core](BAN_CORE.md): termin bezwzględny i sprawdzanie wygaśnięcia
przy wejściu działały już w alpha-0.8.5. Dodaje czas trwania, wspólne funkcje stanu,
filtrowanie historii i odliczanie WebAdmin oparte na czasie serwera.
Gałąź powstaje z main `bdcda8a7254fc5c71276bec28b9b2b96a09dcb47`, wraz z poprawką
rezerwacji portów ncdc pomiędzy interfejsami. Nie następuje automatyczne scalenie.

## Czas i stan blokady

Zapisane znaczniki to całkowite sekundy Unix UTC z zakresu `0..253402300799`.
`duration_seconds` jest kanoniczną dodatnią liczbą całkowitą `1..31536000` (365 dni).
Admin Core oblicza `expires_at = command.time + duration_seconds`; błędny zakres,
przepełnienie i połączenie czasu względnego z bezwzględnym odrzuca przed dostępem
do SQL. Czas polecenia HTTP nadaje serwer; zegar przeglądarki nie wpływa na czas
trwania. Dotychczasowe `expires_at` pozostaje zgodne: przyszły termin UTC w
obsługiwanym zakresie nie podlega limitowi względnych 365 dni. Puste pole oznacza
blokadę stałą.

| Stan wyliczany | Warunek |
| --- | --- |
| revoked — cofnięta | Obecne `revoked_at`, niezależnie od wygaśnięcia |
| permanent — stała | Niecofnięta i bez terminu |
| temporary — czasowa | Niecofnięta i `expires_at > now` |
| expired — wygasła | Niecofnięta i `expires_at <= now` |

Równość oznacza wygaśnięcie. Reguły konta, CID i pojedynczego IP są sprawdzane przy
każdej nowej próbie wejścia według UTC serwera. Wygaśnięcie nie wymaga zadania cron,
okresowego zapisu SQL ani restartu. Niezmienny obraz startowy zawiera reguły, a nie
zapamiętaną flagę aktywności. Nakładające się reguły zachowują najdłuższy termin;
stała ma pierwszeństwo. Wygaśnięcie jednego wpisu nie omija drugiej aktywnej blokady.
Wygasły wpis zachowuje ID, autora, powód, rewizję i znaczniki. Nie jest usuwany ani
oznaczany jako cofnięty i nie tworzy fikcyjnej operacji administracyjnej. Nie trafia
do nowego aktywnego obrazu i nie zużywa limitu aktywnych wpisów przy zapisie.

Korekta zegara systemowego wpływa na kolejne decyzje wejścia; synchronizuj UTC hosta.
Wygaśnięcie nie kończy istniejących sesji. Odrzucony klient musi połączyć się ponownie.
Dodanie/cofnięcie nadal wymaga zatrzymania wszystkich instancji, serwisu administracji
i ponownego startu z nową polityką. Nie ma aktywnego polecenia `+ban`. Obowiązują
wszystkie reguły ról, hasła, hierarchii celu/autora, serializacji i nieznanego wyniku
zapisu z Ban Core. Schemat **v8**, SQL migracji 1–8 i manifest pozostają bez zmian;
migracja 9 nie jest potrzebna.

## Prywatne API administracyjne

Gniazdo Unix tego samego UID, Bearer, tryb administracji i ograniczone formularze
jak w [API](API.md). Odczyt zwraca maksymalnie 16 wpisów uporządkowanych po ID:

```http
GET /api/v1/bans?after=0&state=temporary
```

Opcjonalny `after` to kanoniczny nieujemny kursor signed BIGINT. Opcjonalny `state`
(domyślnie `all`) przyjmuje `all`, `active`, `permanent`, `temporary`, `expired`,
`revoked`. `active` łączy stałe i czasowe. Działa również `?state=expired`; przy obu
parametrach `after` poprzedza `state`. Nieznane, powtórzone, puste lub błędne parametry
są odrzucane. SQL filtruje przed LIMIT. `next_after` wskazuje ostatni pasujący wpis,
albo null dla krótkiej strony. Strony są bieżącymi obserwacjami, bez wspólnego obrazu
transakcji pomiędzy HTTP; upływ czasu lub zapis zmienia wyniki. Pełny aktualny zestaw
należy przeglądać po odświeżeniu od pierwszej strony.

Udane odczyty i zapisy dodają liczbowe `observed_at` (UTC serwera). Lista używa tego
samego momentu do warunków SQL i odpowiedzi. Pola DTO wpisu nie zmieniają się;
stan i pozostały czas są wyliczane. Czas odpowiedzi zapisu jest pobierany po wykonaniu
polecenia: bardzo krótka blokada może wygasnąć jeszcze przed otrzymaniem odpowiedzi.

```text
POST /api/v1/bans
actor_id=42&proof=...&kind=ip&subject=192.0.2.10&reason=Policy&duration_seconds=3600
```

Wymagane jest dokładnie sześć pól: `actor_id`, `proof`, `kind`, `subject`, `reason`
i jedno z `duration_seconds` lub `expires_at`. Oba pola jednocześnie, nawet z pustym
`expires_at`, są odrzucane. Zero, znaki, zera wiodące, ułamki i zbyt długi czas są
odrzucane. POST `/api/v1/bans/ID/revoke` nadal przyjmuje dokładnie `actor_id`, `proof`,
`revision`. Cofnięcie i inne akcje Admin Core nie przyjmują czasu trwania. Zapisy są
limitowane i nigdy ponawiane po nieznanym wyniku. Hasło nie trafia do odpowiedzi,
logów ani stanu/pamięci trwałej UI.

## WebAdmin

Formularz EN/US i PL wybiera blokadę stałą, całkowitą liczbę sekund/minut/godzin/dni
lub lokalną datę przeliczaną na UTC. Łączny względny czas nie przekracza 365 dni.
Zmiana filtra wraca na pierwszą stronę. Cofnięty wpis opuszcza niepasujący filtr,
a zaznaczenie i rewizja wybranego wpisu zostają zachowane.

Odliczanie wiąże `observed_at` z `performance.now()`, aktualizuje się co sekundę
i nie używa `Date.now()`. Błędny zegar komputera nie zmienia wyliczanego wygaśnięcia.
To przybliżenie od ostatniej obserwacji: opóźnienie sieci i późniejsza korekta zegara
serwera wymagają odświeżenia. Wyniki filtra odświeżają się na żądanie, więc widoczny
wpis czasowy może zmienić stan na wygasły przed następnym odczytem. Timer kończy się
po opuszczeniu strony; niedostępna lista usuwa jego źródło. Starsze API bez
`observed_at` nadal pozwala czytać wpisy, bez wymyślonego odliczania i orzekania
czasowa/wygasła. Stan stały i cofnięty pozostaje jednoznaczny. Filtry i tworzenie
względne wymagają nowego API.

## Weryfikacja

Testy modelu i dyspozytora sprawdzają granice sekund, zero/maksimum/przepełnienie,
pierwszeństwo cofnięcia/stałej blokady, nakładanie reguł i odmowę przed SQL. MariaDB
w izolowanych bazach sprawdza tworzenie względne, niezmienną historię/rewizję,
filtrowanie przed paginacją, limit aktywnych i kolejną blokadę po wygaśnięciu. Prywatne
HTTP obejmuje czas trwania, czas serwera, wygaśnięcie bez zapisu, ścisłe parametry
i cofnięcie. Pełny zestaw zachowuje rzeczywiste IP/CID, konto przed GPA, ncdc ADC/ADCS
i utratę potwierdzenia COMMIT. Node/Chromium obejmują redakcję DTO, jednostki, błędny
zegar, odliczanie, filtry, czyszczenie hasła i mobilny PL. Lokalnie używane jest osiem
wątków CPU; GitHub CI zachowuje Ubuntu 24.04 i cztery logiczne CPU na runner z
równoległością `nproc`. Nie czekamy na zakończenie zdalnego CI.
