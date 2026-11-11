# Chat Validation — alpha-0.4.8

**Bieżący etap — alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

Etap bazuje bezpośrednio na main `b20cc83fa46a48642e6939a8ceb11a354339a4dd`, po scaleniu Hub Messages. Wcześniejsze gałęzie/historia są zachowane. Kod i teksty protokołu używają EN/US UTF-8; pomoc operatora i dokumentacja obsługują też polski.

## Wspólna kontrola przyjęcia

`adc::validate_chat(core, sender, message)` w `chat_validation.hpp` i `chat_validation.cpp` centralizuje walidację czatu klientów. PublicChat i PrivateChat wywołują ją przed przygotowaniem planu, także przy bezpośrednim użyciu bez MessageRouter. Router wybiera adapter przez porównanie zakresu enum; nie serializuje już osobno każdej wiadomości przed adapterem. Nie odczytuje niesprawdzonego opcjonalnego SID.

Funkcja nie zmienia sesji/kolejek i zwraca pożyczony `MsgView` albo typowany `ChatValidationError`. Wynik jest danymi, nie tokenem uprawnienia do ponownego użycia. Wejściowy Message musi istnieć dłużej niż widok i pozostać niezmieniony. Nie ma flagi zapamiętanego zatwierdzenia, asynchronicznego wyszukiwania ani nowego trwałego rejestru.

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html) definiuje UTF-8 NFC, czteroznakowe SID base32 i MSG w NORMAL. Implementacja łączy te reguły protokołu z dotychczasowymi limitami tekstu/kolejek i własnością gotowych sesji.

| Sprawdzenie | Wynik |
| --- | --- |
| Niezgodna komenda/kind, brak wektora tekstu, zła liczba pozycyjnych, za dużo parametrów | `invalid_message` |
| Pusty, zbyt długi odkodowany, niepoprawny UTF-8, nie-NFC lub zabroniony sterujący ASCII w tekście | `invalid_text` |
| Brakujący/niepoprawny SID źródła routingu lub lokalnie zarezerwowane AAAA | `invalid_sid` |
| Brakujący/niepoprawny SID celu D/E | `invalid_sid` |
| Niepoprawny pełny nagłówek, flagi, kodowanie nieznanych pól lub długość zakodowana | `invalid_message` |
| Brakujące lub niegotowe życie sesji nadawcy | `invalid_state` |
| Poprawny SID źródła należący do innego połączenia | `wrong_source` |
| Poprawny nieobsługiwany zakres F/H/I | `unsupported_type` |
| Obsługiwane B/D/E z gotowym zgodnym nadawcą | Pożyczony tekst i wartości PM/ME |

Kontrole struktury poprzedzają dostęp do tekstu/SID. Długość tekstu jest ograniczona przed normalizacją lub serializacją. Jawnie sprawdzana jest składnia źródła i celu D/E, następnie pełny kodek MSG. H/I nie może stanowić nadawcy klienta. Dla FMSG powiązanie źródła jest sprawdzane przed zwróceniem nieobsługiwanego zakresu. Wejściowe IMSG nadal jest odrzucane wcześniej przez maszynę stanów; zaufane systemowe IMSG pozostaje w [Hub Messages](HUB_MESSAGES.md).

## Polityka tekstu i SID

Tekst jest niepusty i ma najwyżej 16384 odkodowane bajty. Odkodowane LF jest dozwolone; pozostałe sterujące ASCII i DEL są odrzucane. Poprawny tekst zachowuje dokładne bajty, w tym początkowe/końcowe/powtórzone spacje, nowe linie, backslash i słowa przypominające flagi. NFC jest sprawdzane, nie narzucane przez modyfikację; błędne wejście nie jest naprawiane ani obcinane. Jest to walidacja protokołu, nie moderacja, filtrowanie prezentacji Unicode ani renderowanie znaczników.

Pełny kodek nadal ogranicza całą zakodowaną treść ramki do 16384 bajtów plus końcowe LF oraz 128 parametrów. Wliczane są nagłówki, flagi, separatory i rozwinięcia escape. Nieznane pola nadal wymagają poprawnej składni/UTF-8/NFC, mimo usuwania ich z routingu. Reguły wartości i duplikatów PM/ME pozostają we wspólnym kodeku flag.

SID jest zapisem base32 rozróżniającym wielkość liter i mającym dokładnie cztery znaki. AAAA jest zarezerwowane przez ten hub i nie może być źródłem klienta, także w ręcznie utworzonym obiekcie. Poprawny składniowo cel, np. niedostępne AAAA/ZZZZ, nie przyznaje członkostwa; przygotowanie prywatne zwraca dotychczasowe `recipient_unavailable`, bez zastępczego wyszukiwania po nicku lub deskryptorze.

Wywołujący podaje lokalne życie SessionId niezależne od SID na sieci. Core sprawdza istnienie, pełną gotowość i własność wskazanego źródła. Nadawcy accepted, negotiating, identified z oczekującym powitaniem, closing i closed są odrzucani. Zmiana nicka nie zmienia powiązania źródła.

## Odbiorcy i rozsyłanie

Po wspólnej walidacji adaptery zachowują swoje zakresy i politykę rozmów PM. Broadcast z PM i nieobsługiwane grupy nigdy nie stają się publiczną treścią. Core::prepare_direct wybiera wyłącznie gotowy, zidentyfikowany cel po sprawdzeniu prywatnego PM; zachowuje to kolejność odmowy grup i powiązanie życia sesji. Brakujący/oczekujący/zamykany/zamknięty odbiorca zachowuje odwracalne ISTA 140 bez prywatnej kopii lub echa.

Pożyczony widok sam nie wybiera celu ani nie zatwierdza dostarczenia. Przygotowanie Core ponownie sprawdza stan przy tworzeniu własnego planu; dispatch nadal sprawdza domenę silnika, gotowość/źródło nadawcy, tożsamość celu i każde wybrane życie bezpośrednio przed callbackiem. Zamknięcie/usunięcie źródła po poprawnej walidacji uniemożliwia późniejszą publikację. Stare plany nie trafiają do nowych właścicieli SID/fd, późniejszych logowań ani innej domeny Core. Nadal obowiązuje jeden wątek właściciela reaktora.

PublicChat/PrivateChat zachowują publiczne enumy błędów. Błędy tekstu/SID/obiektu mapują się na invalid_message, invalid_state i wrong_source zachowują odpowiadające wyniki, a poprawne nieobsługiwane zakresy pozostają unsupported_type. MessageRouter zachowuje błędy, a LoginHub stałe oczyszczone opisy diagnostyczne.

## Działający serwer i błędy

Wejście najpierw przechodzi parser ogólny i dotychczasową maszynę stanów połączenia. Błędna składnia, UTF-8/NFC, escape lub zbyt długa ramka są odrzucane w dotychczasowych limitach; następna legalna wiadomość może przywrócić obsługę. NORMAL/źródło i błędy znanych pól zachowują fatalne ISTA 244/240 z ograniczonym zamknięciem przez własne IQUI. Nieobsługiwane zakresy/grupy pozostają odwracalnym 125. Nowa warstwa nie osłabia wcześniejszych kontroli i nie umieszcza treści w diagnostyce.

Alokacje walidacji mogą rzucić wyjątek przed publikacją. Dotychczasowa obsługa LoginHub zamyka tylko źródło przy błędzie przygotowania; błędy kolejek odbiorców pozostają izolowane w dispatch. Walidator nie ma skutków ubocznych. Nie dodano zależności, konfiguracji, migracji SQL, endpointu HTTP, pola API, historii ani moderacji. Treść pozostaje poza logami, bazą i eksportami administracyjnymi. Zaktualizowano pomoc US/PL i metadane wydania.

## Weryfikacja

`chat.validation` obejmuje sterujące/UTF-8/NFC tekstu, dokładne limity zakodowane, literalne białe znaki/komendy, wadliwe pełne obiekty, składnię źródła/celu i brakujące optional, zarezerwowane źródło, podszywanie się, wszystkie fazy nadawcy i niegotowe fazy celu. Sprawdza też bezpośrednie adaptery/router, kolejność odmowy zakresu, zmianę nicka, rollback alokacji i walidację poprzedzającą zamknięcie źródła/odmowę dispatch. Dotychczasowe testy życia, domen, PM/ME, awarii kolejek i przypiętego wyjścia pozostają aktywne.

`chat.validation.process` używa obu transportów źródła: odzyskiwanie po błędnym tekście/SID z uporządkowanymi znacznikami obserwatora, zachowanie poprawnego Unicode/spacji/nowych linii, ramki graniczne/nadmierne, odmowa B/D/E przed NORMAL, fałszowanie źródła i prywatność logów/API. Rzeczywisty ncdc ADC/ADCS odbiera dodatkowe Unicode/nową linię/ME0 przy zachowaniu sprawdzeń czatu publicznego/prywatnego, akcji, powitania i reconnect.

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --parallel 3
ctest --test-dir build-debug -R '^(chat.validation|adc.ncdc|adcs.ncdc)' --output-on-failure
```

Bieżące wyniki zapisano w [CHANGELOG.md](../../CHANGELOG.md). Zdalne CI, pełna macierz systemów/klientów, obciążenie produkcyjne i długotrwały fuzzing wymagają osobnych dowodów.

Weryfikacja lokalna z 2026-11-11: **84/84 zestawów CTest przeszło w Debug, Release i ASan/UBSan**, z wymaganiem narzędzi integracyjnych i bez wyłączania wykrywania wycieków. Obejmuje to rzeczywisty ncdc ADC/ADCS, Chat Validation, regresje czatu/sesji/kolejek, tymczasową MariaDB i Apache HTTPS. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły kompilacja/typecheck WebAdmin, 4 testy Node, 3 testy Chromium i integracja z rzeczywistym API C++. Sprawdzono metadane, UTF-8, zgodność plików językowych, lokalne odnośniki, składnię zmienionych testów Pythona, pomoc US/PL i białe znaki Git. Zdalne CI, pełna macierz systemów/klientów, obciążenie produkcyjne i długotrwały fuzzing pozostają niezweryfikowane.
