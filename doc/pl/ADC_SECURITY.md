# Bezpieczeństwo ADC — alpha-0.9.2

Walidacja parsera, fuzzing i kontrola protokołu. Ten etap nazywa profil poleceń huba, który automat połączenia już wymuszał, i wywołuje tę jedną funkcję przed sprawdzeniem stanu i sekwencji. Gałąź powstaje z main `6c26839`. Nazwa gałęzi to `alpha-0.9.2`.

Zbiór ramek przyjmowanych na łączu pozostaje ten sam. Nieznane rozszerzenia nadal są parsowane. GET, GFI i SND pozostają nieobsługiwanymi komendami, a nie błędami składni. To nie jest nowa klasa odrzuceń, fuzzing sterowany pokryciem ani limit pasma.

## Profil poleceń

`control_message` w `dc24h_adc` zwraca `ok`, `unsupported_command` albo `invalid_type`. Czyta kierunek, typ wiadomości i komendę. Nie czyta parametrów, cech ani tekstu i niczego nie alokuje.

`ConnectionStateMachine::observe` wywołuje je, gdy nazwa komendy zgadza się z rodzajem, a przed sprawdzeniem stanu, źródła i sekwencji. Rozłączona sesja nadal zwraca najpierw `disconnected`. Ręcznie złożona wiadomość, której rodzaj nie zgadza się z nazwą komendy, nadal zwraca `invalid_message` przed profilem.

| Komenda | Profil |
| --- | --- |
| Nieznana, GET, GFI, SND | `unsupported_command` dla każdego typu i obu kierunków, przed sprawdzeniem typu |
| SUP | Przychodzące hub albo wychodzące info |
| PAS | Tylko przychodzące hub. Każde wychodzące PAS to `invalid_type` |
| SID, GPA, QUI | Tylko wychodzące info |
| INF | Przychodzące broadcast. Wychodzące info albo broadcast |
| STA, MSG | Broadcast, direct, echo, feature oraz przychodzące hub albo wychodzące info |
| SCH, RES, CTM, RCM | Broadcast, feature, direct albo echo |

Składnia argumentów i tekstu zostaje w [parserze](ADC_PROTOCOL.md). Parser nadal przyjmuje nieznane rozszerzenia i ramki legalne dla huba, w tym ISID i GET. Serializator nadal parsuje ponownie ramki huba. Nieznana komenda nadal trafia do dotychczasowej ścieżki logowania i może dać dotychczasowy status nieobsługiwanej komendy. Nie jest przeklasyfikowana na niepoprawną ramkę.

Wychodzące PAS już było odrzucane. Profil zgłasza teraz `invalid_type` przed sprawdzeniem stanu, także gdy stan też byłby niepoprawny. Przyjmowane ramki się nie zmieniają. Status kierowany poza NORMAL pozostaje `invalid_type` automatu, po profilu. Sprawdzenia stanu, sekwencji i źródła zostają w [automacie](ADC_STATE.md).

## Kampania fuzzingu

`adc.security` jest deterministycznym CTestem w procesie. Nie używa libFuzzer i nie mierzy pokrycia.

- Generator: `std::mt19937`, ziarno `0x0ADC0922`. To samo ziarno wyznacza rozmiary kawałków. Test sprawdza ziarno.
- Dokładnie 4096 mutacji. Test sprawdza tę liczbę, więc kampania nie może się skurczyć bez nieudanego buildu.
- Każda mutacja startuje ze stałej ramki BASE: HSUP, ISUP, ISID, BINF, HMSG, BMSG, DMSG, EMSG, FSCH, ISTA, IQUI, HPAS, IGPA, BSTA, BSCH, DRES, DCTM, DRCM, HZ99 albo HGET. Edycja odwraca bajt, wstawia bajt, usuwa bajt, powiela prefiks albo dokleja LF. Bufor kończy się na `max_frame_bytes + 64`.
- Żaden wyjątek nie wychodzi na zewnątrz. Po każdym kawałku bufor dekodera strumienia ma co najwyżej `max_frame_bytes`.
- Wyrocznia podziału linii zgadza się z dekoderem, także dla zbyt długiej ramki bez LF. Takie wejście już zgłosiło `frame_too_long`, a `finish()` jest puste, a nie `truncated_frame`. Krótka ramka bez LF to `truncated_frame`.
- Każdy `Message` z `parse_line` wraca przez `serialize` do tych samych pól.
- Na świeżym automacie `control_message` i `observe` zgadzają się co do `unsupported_command` i profilowego `invalid_type`.

Osiem typów wiadomości, szesnaście komend i oba kierunki są sprawdzane bezpośrednio. Parametry nie zmieniają wyniku profilu.

Kampania nie dowodzi braku błędów parsera. Nie zamyka zagrożeń szczątkowych T01–T16 z alpha-0.9.1. T01 pozostaje podszytą tożsamością poza tą bramą. T13 pozostaje wyczerpaniem zasobów. Dotychczasowe testy `adc.parser` i `adc.syntax` zostają. Zobacz [Testy](TESTING.md) i [Bezpieczeństwo](SECURITY.md).

## Bez zmian

Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, strony WebAdmin, napisu językowego ani ustawienia Apache. Check status zostaje na runnerach GitHub-hosted Ubuntu 24.04. HTTP/2, OCSP, automatyczne odnawianie i publiczna uwierzytelniona brama pozostają niezaimplementowane. `configuration.manage`, `hub.manage` oraz kick i mute w przeglądarce pozostają niezaimplementowane.
