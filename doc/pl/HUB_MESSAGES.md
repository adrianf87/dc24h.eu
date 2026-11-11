# Hub Messages — alpha-0.4.7

**Bieżący etap — alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

**Etap dostarczony w alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizuje sprawdzanie tekstu, SID i gotowości/własności nadawcy w obu adapterach. Wybór życia odbiorcy i ponowna kontrola dispatch pozostają obowiązkowe; systemowe IMSG jest oddzielone.

Etap bazuje bezpośrednio na main `8d0da2516b007466828b00aafd4e7bf7a857b9f3`, po scaleniu Message Flags. Wcześniejsze gałęzie i historia są zachowane. Kod i systemowe teksty protokołu używają EN/US UTF-8; pomoc operatora i dokumentacja obsługują też polski.

## Zakres

`adc::HubMessages` przygotowuje i rozsyła systemowe wiadomości przez zaufany interfejs wewnętrzny. Niemutowalny `HubMessageFrame` zawiera IMSG z jednym tekstem pozycyjnym, bez SID źródła/celu, selektorów cech i pól PM/ME. [Specyfikacja ADC](https://adc.sourceforge.io/ADC.html) określa typ I jako informację pochodzącą z huba i podaje przykład IMSG. Czat klientów pozostaje w MessageRouter; nigdy nie jest promowany do wyjścia systemowego.

`make_hub_message(text)` zwraca współdzielony niemutowalny pakiet albo typowany `HubMessageError`. Sprawdza niepusty, ograniczony tekst UTF-8/NFC, dopuszcza odkodowane LF, odrzuca pozostałe sterujące ASCII/DEL i serializuje przez istniejący kodek MSG. Spacje, LF i backslash są zapisywane przez escape. Cała zakodowana treść ramki ma limit 16384 bajtów plus końcowe LF; rozwinięcia escape są wliczane. Niepoprawna treść nie jest normalizowana ani obcinana. Literalne `BMSG`, `PM` lub `ME1` w tekście pozostają treścią.

## Wybór i rozsyłanie

`HubMessages::prepare(text, optional<SessionId> target)` przyjmuje zaufany tekst, nie komunikat klienta ani tożsamość źródła. Cel wybiera jedną gotową sesję według identyfikatora życia; brak celu wybiera uporządkowaną migawkę wszystkich gotowych sesji przez ograniczone strony rejestru. Oczekujące powitania, sesje anonimowe, zamykane i zamknięte są pomijane. Brakujący/niegotowy cel zwraca `recipient_unavailable` przed publikacją. Broadcast bez gotowych użytkowników jest poprawnym pustym planem.

`HubDelivery` posiada ramkę, ID odbiorców i prywatny token domeny usługi. Macierzysta usługa ponownie sprawdza każde życie sesji podczas dispatch. Usunięci odbiorcy są niedostępni; późniejsze logowania i ponownie użyte SID nie dziedziczą planów. Inna usługa, także nad tym samym rejestrem, nie może rozesłać planu. Zachowany token chroni też przed zaakceptowaniem planu przez nową usługę pod ponownie użytym adresem. Plan po przeniesieniu nie ma ramki i jest odrzucany.

Dispatch nie alokuje po utworzeniu callbacka. Zwraca liczniki queued, unavailable i refused przez `chat::DispatchReport`, bez tworzenia fikcyjnego nadawcy czatu. Zaufane callbacki mogą zamykać sesje, lecz nie mogą rzucać wyjątków ani ponawiać częściowego rozesłania. Przyjęcie do kolejki nie potwierdza odebrania/przeczytania; brak atomowości między odbiorcami.

`LoginHub::send_hub_message(text, optional<SessionId> target)` łączy usługę z istniejącymi ograniczonymi kolejkami. Jest to API C++ wątku reaktora, nie nowy endpoint HTTP ani komenda klienta. Błędy przygotowania nie zmieniają kolejek; alokacja przed dispatch propaguje wyjątek do zaufanego wywołującego. Błąd alokacji kolejki zamyka tylko odbiorcę przez `internal_error`; odmowa limitu używa `output_limit`. Pozostali wybrani odbiorcy są nadal obsługiwani. Pamięć pakietu jest współdzielona, ale każdy odbiorca zachowuje własne FIFO, offset i pełne logiczne obciążenie bajtami. Częściowe zapisy TCP/TLS i anulowanie ogona przy zamknięciu zachowują reguły niemutowalnego początku kolejki.

## Powitanie w działającym serwerze

Każdy LoginHub przygotowuje jeden współdzielony stały komunikat:

```text
Welcome to dc24h.eu. Guest access is enabled.
```

Po zatwierdzeniu poprawnej tożsamości gościa kolejność wyjścia to IINF huba, BINF obecnych publicznych użytkowników, własne BINF, następnie powitalne IMSG. Kolejne zdarzenia obecności/czatu trafiają dalej w tym samym FIFO. Powitanie jest kolejkowane przed zatwierdzeniem rejestru i publikacją; pełna długość ramki jest rezerwowana w łącznym budżecie powitania. Błąd lub przekroczenie budżetu nie publikuje użytkownika ani komunikatu.

Końcowe LF własnego BINF nadal oznacza gotowość i zwalnia limit oczekujących logowań dokładnie raz. Następujące IMSG nie opóźnia ani nie powtarza tego przejścia. Klient widzi więc własną tożsamość przed czatem systemowym. Zamknięcie przed ukończeniem anuluje niewysłany ogon według dotychczasowych reguł. Reconnect dostaje jedno nowe powitanie; dotychczasowi użytkownicy nie otrzymują powitania innej osoby.

Stały komunikat nie zawiera nicka, adresu, CID/PID, danych uwierzytelnienia ani wejścia klienta. Pozostaje angielskim tekstem protokołu niezależnie od języka operatora `--language`; nie dodano negocjacji języka klienta. Pomoc US/PL wyjaśnia IMSG. Usługa nie loguje, nie zapisuje w bazie ani nie eksportuje dowolnego tekstu systemowego. Zaufany wywołujący odpowiada za dobór treści bez sekretów; usługa nie jest mechanizmem usuwania poufnych danych.

## Zaufanie i ograniczenia

IMSG pochodzące od klienta nadal ma niedozwolony kierunek: fatalne ISTA 240 i końcowe własne IQUI, bez rozsyłania podanej treści. HMSG pozostaje nieobsługiwaną operacją klient–hub z odwracalnym 125; nie dodano interpretera komend ani echa jako komunikatu systemowego. IMSG nie zastępuje diagnostyki STA/QUI.

Nie dodano konfiguracji, zależności, migracji, pól JSON API ani funkcji WebAdmin. Zdalne wiadomości administratora, harmonogramy ogłoszeń, komunikaty przy wygaszaniu serwera, język klienta, historia i moderacja pozostają planem. Kod wywołujący musi działać w tym samym wątku reaktora; rejestr nie staje się bezpieczny wielowątkowo.

## Weryfikacja

`chat.hub` obejmuje kodowanie, granice tekstu/zakodowanej ramki, niemutowalne pakiety, stronicowanie 270 odbiorców, pomijanie oczekujących, typowany brak celu, izolację domen, przeniesione/nieaktualne plany, późniejsze logowania, mutację/odmowę callbacka, dispatch bez alokacji i rollback przygotowania. Sprawdzenia LoginHub obejmują kolejność powitania, dostarczanie systemowe do celu i wszystkich gotowych, współdzielenie adresów pakietów, częściowe przypięte wyjście, anulowanie ogona, izolację odmowy limitu i zwalnianie obciążeń. Dotychczasowe testy alokacji kolejek i transportów sprawdzają rozszerzony wariant ramek.

`chat.hub.process` sprawdza fragmentowane logowanie, dokładną kolejność własnego INF/IMSG, pojedyncze powitanie, reconnect, ADC/ADCS, odmowę podszywania się klienta pod hub i prywatność logów/API z niezwiązanym obserwatorem. Wspólne helpery procesowe jawnie sprawdzają powitanie. Rzeczywisty ncdc ADC/ADCS wyświetla komunikat systemowy przy zachowaniu testów czatu publicznego/prywatnego, akcji i reconnect; nagranie nieszyfrowanego transportu sprawdza rzeczywistą ramkę IMSG.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(chat.hub|adc.ncdc|adcs.ncdc)' --output-on-failure
```

Bieżące wyniki zapisano w [CHANGELOG.md](../../CHANGELOG.md). Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne wymagają osobnych dowodów.

Weryfikacja lokalna z 2026-11-11: **82/82 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganiem narzędzi integracyjnych i bez wyłączania wykrywania wycieków. Obejmuje to rzeczywisty ncdc ADC/ADCS, Hub Messages, regresje czatu/sesji/kolejek, tymczasową MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły kompilacja/typecheck WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych, lokalne odnośniki, składnię zmienionych testów Pythona, pomoc US/PL i białe znaki Git. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niezweryfikowane.
