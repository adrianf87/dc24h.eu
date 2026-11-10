# User Core — alpha-0.3.3

Gałąź bazuje na main `ca00266992dd866bdd30ff68e911c981c6665a62`, po scaleniu Session Core. Zamówiony alpha-0.3.2 Session Manager jest osobną gałęzią z tego samego main, bez ukrytej zależności ani automatycznego scalenia. User Core rozszerza dc24h_users o samodzielny publiczny model gościa User i ograniczony, typowany Profile, zintegrowane z logowaniem i aktualizacjami LoginHub.

## Użytkownik, sesja i tożsamość

Session opisuje jeden czas życia transportu oraz gotowość i zamykanie. User opisuje przyjętego publicznego gościa na sesji: Identity (SID protokołu, CID klienta, nick) oraz Profile. Session posiada opcjonalny User, nieobecny przed przyjęciem. Model Users ma jedną żywą kopię tożsamości. Session::identity() zwraca pożyczony wskaźnik const do User; Session::user() referencję const do optional. Mutacje odbywają się wyłącznie przez metody cyklu życia. Kopie User/Profile posiadają własne teksty i listę funkcji.

Identity przeniesiono z session.hpp do user.hpp; dotychczasowy include session.hpp nadal działa. Limity magazynowania pozostają: SID 64 bajty, CID 128, nick 256, wartości niepuste i bez NUL. To ograniczone klucze publiczne, a nie dodatkowa walidacja ADC. Kodeki Sid/TigerCid, dowód Tiger, reguły NFC/nick oraz indeksy unikalności ADC pozostają autorytatywne. Bezpośrednie utworzenie Identity/User dowodzi wyłącznie ograniczeń pamięci, nie własności CID, konta ani unikalności. CID nie jest trwałym ID konta ani poświadczeniem. SID należy do połączenia i może być ponownie wydzierżawiony po sprzątaniu; lokalny SessionId rozróżnia czasy życia.

User jest kopiowalną wartością z dostępem tylko do odczytu. User::create odrzuca niepoprawną tożsamość, także opróżnioną przez przeniesienie. Klucze przyjętej sesji są niezmienne. Session::identify(User,time) wiąże użytkownika raz, podczas negotiating. Wariant Identity nadal istnieje i tworzy pusty profil. Session::update_user(User,time) zastępuje nick i cały profil jednocześnie, wyłącznie w identified/ready, przy niezmiennych SID/CID i niemalejącym czasie. Niepoprawna faza, cofnięty czas lub zmienione klucze pozostawiają model bez zmian. Dotychczasowe rename zmienia tę samą tożsamość User i zachowuje profil. Zamykanie utrzymuje ostatnią wartość publiczną do usunięcia sesji, bez tworzenia trwałego konta.

## Profil publiczny

ProfileData zawiera jawną listę pól, zamiast dowolnych pól INF:

| Pole | Projekcja ADC | Przechowywanie / znaczenie |
| --- | --- | --- |
| description | DE | Do 2048 bajtów |
| email | EM | Do 254 bajtów; publiczna deklaracja klienta, bez potwierdzania kontaktu |
| application / version | AP / VE | Do 128 bajtów każde |
| locale | LO | Do 64 bajtów; deklaracja tekstowa, nie wybór języka serwera |
| shared_bytes / shared_files / slots | SS / SF / SL | Opcjonalne nieujemne int64, również zero i INT64_MAX |
| features | SU | Do 128 unikalnych czteroznakowych tokenów wielkich liter/cyfr |

Profile::create sprawdza limity bajtów/liczby elementów, brak kontrolek ASCII/NUL, nieujemne liczby oraz format/unikalność funkcji. Suma treści tekstowej/tokenów nie przekracza 4096 bajtów; obecne limity pojedynczych pól też ograniczają ją poniżej tej wartości. Pusty tekst/lista i brak liczby oznaczają brak deklaracji, odróżniony od liczbowego zera. Model nie potwierdza zasobów, adresu email ani działania funkcji. Semantykę UTF-8/NFC sprawdza walidator ADC przed projekcją; fabryka magazynowania nie jest walidatorem Unicode/protokołu. Biblioteka pozostaje niezależna od ADC, OpenSSL, utf8proc, SQL, gniazd i logowania.

User/Profile nie przechowuje PID/PD, PAS/hasła, adresu transportowego, I4/I6, portów, dowolnych rozszerzeń, RF/referrera, uprawnień CT, konta, roli ani bana. Jawne pola C++ wykluczają dowolną mapę metadanych. Dotychczasowy ClientInfo nadal posiada zwalidowane dane protokołu potrzebne do przekazywania INF; etap nie zmienia serializacji publicznego INF ani nie usuwa pól wymaganych przez protokół.

## Przyjęcie, aktualizacje i kopie

LoginHub projektuje cały zwalidowany ClientInfo, nigdy surowe tokeny. User powstaje po walidacji tożsamości/unikalności i poprawnym zbudowaniu ograniczonego powitania. Identified pozostaje odrębne od ready do pełnego wysłania własnego INF. Każda przyjęta aktualizacja INF projektuje cały scalony ClientInfo: pominięte pola zachowują wartość, jawne kasowanie ją usuwa. Nick i profil zatwierdzane są razem przez update_user. Błędne INF i zajęty nick zachowują poprzedni model podczas zamykania według istniejącej polityki. Wyjątki alokacji i błędy wyjścia obsługuje dotychczasowa ścieżka; nie dodano gwarancji odporności na dowolny błąd alokacji.

Snapshot zachowuje opcjonalne identity i dodaje opcjonalny profile, kopiowane z tego samego User. Oba są nieobecne przed przyjęciem i obecne po nim, również przy pustym profilu. Nie powiela to tożsamości żywego modelu, zachowując dotychczasowy odczyt identity w kopii. Kopie przetrwają aktualizacje, zamknięcie i usunięcie źródła. Pożyczone referencje dotyczą wyłącznie wątku właściciela i wygasają przy zastąpieniu/zniszczeniu. Są to dane wewnętrzne C++, bez zmiany schematu JSON API ani udostępnienia profili przez HTTP.

Indeksy unikalności nick/CID/SID, rejestr obiektów protokołu, własność transportu, limity bezpieczeństwa i odpowiedzi protokołu pozostają bez zmian. Nie potrzeba migracji SQL, trwałych kont, konfiguracji, nowych komunikatów runtime ani zależności. Istniejące katalogi US/PL są nadal aktualne. Main i historyczne gałęzie są zachowane.

## Weryfikacja i ograniczenia

`users.core` sprawdza granice tekstu/funkcji/liczb, kontrolki/NUL, błędny format/powtórzenia funkcji, ujemne deklaracje, własność kopii, fazę/czas, niezmienne SID/CID i atomową wymianę profilu. Testy rzeczywistego LoginHub obejmują projekcję profilu, INT64_MAX bez obcięcia, aktualizacje/kasowanie, odrzucenie błędnego INF i zajętego nicka bez częściowej zmiany, niezależnego klienta, sprzątanie i ponowne logowanie. Dotychczasowe testy Session Core, procesów TCP/TLS i rzeczywistego ncdc pozostają wymagane.

To publiczny model gościa w pamięci, bez uwierzytelniania kont, globalnego katalogu, wielu sesji konta, ról, banów, czatu/wyszukiwania/transferów, trwałej historii i administracyjnych mutacji. Deklaracje klienta nie są zaufanymi faktami. Zobacz [testowanie](TESTING.md), [Session Core](SESSION_CORE.md), [INF](ADC_INF.md) i [walidację tożsamości](ADC_IDENTITY.md). Lokalne testy nie dowodzą działania innych systemów, zdalnego CI, obciążenia, fuzzingu ani audytu bezpieczeństwa.
