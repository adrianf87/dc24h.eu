# Ban Core — alpha-0.8.5

**alpha-0.8.6 Temporary Bans:** [Czas trwania, automatyczne wygaśnięcie i czas serwera w UI](TEMPORARY_BANS.md) rozwijają Ban Core bez zmiany schematu v8 i migracji SQL 1–8. Nowy kontrakt czasu trwania i filtrów API opisano w podanym dokumencie.

Hub przechowuje i egzekwuje stałe lub czasowe blokady ID kont, kanonicznych CID
Tiger oraz pojedynczych adresów IPv4/IPv6. Prywatny serwis i dwujęzyczny WebAdmin
pozwalają dodawać/cofać wpisy. Hub ładuje niezmienny katalog przy starcie; wejście
nie wykonuje SQL, sprawdzania haseł ani alokacji. Etap dodaje schemat **v8**;
pliki SQL migracji 1–7 pozostają bez zmian.

## Zakres i identyfikatory

| Typ | Wartość | Moment sprawdzenia |
| --- | --- | --- |
| account | Dodatnie kanoniczne ID konta | Identyfikacja konta przed wysłaniem GPA |
| cid | Kanoniczny 39-znakowy CID Tiger | Sprawdzony INF przed obecnością i GPA |
| ip | Jeden liczbowy IPv4/IPv6 | Accept przed TLS, modelem sesji i SID |

Blokada konta dotyczy uwierzytelniania kont; gdy jest wyłączone, nie ma tożsamości
konta do porównania. CID/IP działają, gdy wczytano katalog z bazy. Hub z wyłączoną
bazą nie ma trwałego katalogu blokad. CID jest identyfikatorem kontrolowanym przez
klienta, nie dowodem konta. IP blokuje **każde** połączenie z adresu, również
personel i wspólny NAT. Rola nie omija blokady zasobu. Akceptujemy tylko pojedyncze
IP: bez DNS, CIDR, portów, nawiasów i identyfikatorów stref. IPv6 jest normalizowany,
a IPv4 zapisany jako mapped IPv6 staje się zwykłym IPv4, bez obejścia aliasem.

Prywatny powód to niepusty tekst NFC UTF-8 do 256 bajtów, bez znaków sterujących,
formatowania i separatorów wierszy. Nie trafia do protokołu publicznego ani logów.
`expires_at = null` oznacza blokadę stałą; inaczej wygaśnięcie musi być późniejsze
od utworzenia i mieścić się w zakresie UTC. Wygaśnięcie jest sprawdzane przy każdym
nowym wejściu bez restartu; odrzucony klient musi połączyć się ponownie. Nie zmieniamy
statusu kont, haseł, poświadczeń ADC ani mapowań. Uprawnienia hasłem do prywatnego
serwisu pozostają osobne, umożliwiając lokalne cofnięcie błędnej blokady IP.

## Uprawnienia serwisowe i baza

Przed zapisem zatrzymaj wszystkie huby, następnie uruchom je ponownie, aby wczytać
nowe/cofnięte wpisy. Nie modyfikujemy aktywnych sesji i nie dodajemy `+ban` na żywo.
Admin Core otrzymuje `bans.create` i `bans.revoke` — łącznie dziesięć poleceń
serwisowych. Nowe operacje są dostępne tylko przez prywatny serwis; CLI właściciela
zachowuje wyłącznie dwa polecenia własności.

Aktywny Operator lub wyższy musi potwierdzić aktualne hasło konta i mieć oba granty:
`users.ban` oraz `blocks.manage`. Konto celu i konto już powiązanego CID muszą mieć
ściśle niższą rangę. Siebie/równych/wyższych odrzucamy. Niepowiązany CID nie ma
rangi konta do porównania. Mapowanie jest sprawdzane ponownie pod blokadą.
Cofnięcie wymaga też, aby twórca był wykonawcą albo miał obecnie niższą rangę.
Przyszłe zaufane powiązanie tożsamości może zmienić użytkownika zablokowanego CID;
sprawdzaj blokady zasobów przy nadawaniu ról. Nie ma ukrytego wyjątku Ownera ani
wymuszonego cofnięcia. Konfliktowe stare dane wymagają zaufanej naprawy offline.

Zapis sprawdza dane/schemat, świeże metadane wykonawcy/celu i dokładny weryfikator.
KDF działa przed blokadami. Transakcja ponownie sprawdza konta w kolejności ID,
mapowanie CID i dowód hasła. Nieblokująca blokada nazwana bazy szereguje zapisy
oraz kontrolę pojemności. Powtórne przejęcie własnej blokady jest odrzucane bez
zwolnienia blokady wywołującego. Rewizja chroni cofnięcie przed starym żądaniem.
Dodanie tworzy wpis; cofnięcie podnosi rewizję i zapisuje czas/wykonawcę, bez DELETE.
Limit wynosi 4096 aktywnych rekordów. Duplikaty są dozwolone: dowolny aktywny wpis
blokuje, a blokada stała/najpóźniejsze wygaśnięcie wygrywa. Cofnij wszystkie
pasujące wpisy, aby odblokować zduplikowaną wartość.

`0008_bans.sql` tworzy InnoDB, indeksy, ograniczenia typu/czasu/rewizji i restrykcyjne
klucze obce twórcy/cofającego do kont. Zachowuje wcześniejsze konta, role, hasła
i mapowania. Start nie migruje automatycznie. Zrób kopię, zatrzymaj hub, uruchom
jawne `--migrate-database` i sprawdź czyste v8 przed startem. Starsze narzędzia migracji/uwierzytelniania kont odrzucają nowszą historię.
Starszy hub gościnny może jej nie sprawdzać i nie egzekwuje tych blokad; downgrade
wymaga zgodnej kopii i weryfikacji zachowania huba. Użytkownik SQL huba potrzebuje
SELECT na blokadach i metadanych migracji. Serwis dodatkowo INSERT/UPDATE na
blokadach i dotychczasowego dostępu do kont/dowodów. Program nie nadaje grantów.

Katalog czyta aktywne rekordy stronami do 1024 w transakcji odczytu i buduje
niezmienne indeksy hash kont/CID/IP. Błędny aktywny rekord, przepełnienie limitu,
niezgodny/brudny schemat lub błąd SQL zatrzymują start przed portami. Wygasłe
/cofnięte wpisy są pomijane. Indeks nie zachowuje powodów/twórców/dowodów haseł.
Bezpośredni zapis SQL jest poza protokołem; musi być zatrzymany podczas serwisu/startu.

## Protokół i API

IP kończy przyjęte gniazdo przed TLS, więc nie ma wtedy komunikatu aplikacyjnego.
CID/konto dostaje ogólne `ISTA 231 Access blocked` dla blokady stałej lub
`ISTA 232 ... TL<sekundy>` dla czasowej, potem własne końcowe IQUI z tym samym
TL (-1 dla stałej). Nie publikuje się GPA ani publicznego dołączenia. Użyto
istniejących [reguł STA/QUI ADC](https://adc.sourceforge.io/ADC.html).
Stałe `event=banned` nie zawiera wartości/powodów/haseł. Odrzucenie IP używa
zbiorczych liczników przyjęć, bez logu treści/adresu dla każdej próby.

Zatrzymany serwis dodaje:

```text
GET  /api/v1/bans?after=0
POST /api/v1/bans
POST /api/v1/bans/{id}/revoke
```

Formularz dodania: `actor_id`, `proof`, `kind`, `subject`, `reason`, `expires_at`
(puste dla stałej, inaczej sekundy Unix UTC). Cofnięcie: `actor_id`, `proof`,
`revision`. Obowiązują prywatne gniazdo Unix z tym samym UID, token Bearer serwera,
ograniczone formularze adaptera z same-origin/nagłówkiem, limit zapisów i świeże
hasło. Nie powstaje publiczne logowanie administratora ani zapis w działającym hubie.
GET zwraca do 16 rekordów ze stanem wygasłym/cofniętym, po ID, z `next_after`.
ID/rewizje to dokładne ciągi dziesiętne. Odpowiedzi mają `maintenance: true`
i `restart_required: true`; zapis zwraca `ban`. Powody/wartości są widoczne
wyłącznie w prywatnym serwisie.

Nie ma ponownego połączenia ani ponawiania zapisu. Nieznany wynik SQL pozostaje
`outcome_unknown` i wymaga sprawdzenia przed kolejną operacją. Błąd budowania
odpowiedzi po udanym zapisie też oznacza niepewność dla klienta; licznik silnika
nadal opisuje zakończoną operację backendu. Test prywatnego proxy gubi prawdziwe
udane potwierdzenie COMMIT blokady i niezależnie potwierdza jeden zapis bez powtórki.

Strona Blokady ma typ/wartość, opcjonalne wygaśnięcie w czasie lokalnym, hasło,
potwierdzenie, ochronę rewizji i informację o restarcie. Hasło czyścimy przy wysłaniu;
nie trafia do storage, URL, stanu ani eksportu. Adapter/przeglądarka zachowują
BIGINT, sprawdzają DTO, tekst/czas/wartości i usuwają dodatkowe pola. Starsze API
lub działający hub wyłączają formularz. Rekordy opisują cykl blokady, nie pełny
trwały system zdarzeń audytu.

## Weryfikacja i dalszy zakres

Testy obejmują kanoniczne/mapped IP, CID, stałe/czasowe nakładanie, odczyt bez
alokacji, hierarchię, wyścig dowodu hasła, stare cofnięcie, błędne SQL, zachowanie
v7→v8 i migracje historyczne. Natywne wejście sprawdza konto/CID przed GPA/obecnością.
Prawdziwy hub z tymczasową bazą sprawdza IP/CID i wygaśnięcie bez restartu;
prywatne HTTP dodaje/cofa przez Admin Core. Node/Chromium i regresje rzeczywistego
C++/adapter/React sprawdzają DTO, formularze, rewizje, precyzję i polski widok mobilny.
Pozostają obowiązkowe testy logowania i kick z rzeczywistym ncdc ADC/ADCS.

CIDR/podsieci, polecenia ban na żywo/reload, publiczne sesje administracyjne,
pełny audyt i mute to kolejne etapy. Lokalnie polecenia używają ośmiu wątków.
CI zachowuje Ubuntu 24.04 i cztery logiczne CPU na runner z równoległością z nproc.
Nie czekamy na zdalny wynik. Nie wykonujemy produkcyjnej migracji ani wdrożenia.
