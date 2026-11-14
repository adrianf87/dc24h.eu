# Error Handling — alpha-0.6.9

**Weryfikacja lokalna alpha-0.6.9 (2026-11-14):** Przeszły wszystkie 165/165 testów CTest w Debug, Release i ASan/UBSan, w tym nowe awarie/statusy/rozłączenia, cztery scenariusze ncdc z resetem/błędami/dalszą komunikacją i wcześniejsze poprawne handshake zwykłe/TLS. Końcowe testy braku pamięci na terminalną diagnostykę CTM/RCM ponownie przeszły w trzech trybach. Brak ostrzeżeń kompilatora. Poprawne są build/typecheck WebAdmin, 12 testów Node, 10 testów Chromium i integracja API C++. Przeszły kontrole metadanych, UTF-8, zgodności języków/linków, składni zmienionych plików Python, przykładowego INI i białych znaków. Nie oczekiwano na GitHub Actions; pełna macierz systemów i obciążenie produkcyjne pozostają niesprawdzone.

Etap oparty bezpośrednio na main `9efb6020755362d4c9ea537014a36cfbfed145b3` po Connection Security. Gałąź numerowana zachowuje historię i jest publikowana osobno od main. Etap ujednolica diagnostykę połączeń i routing STA oraz usuwa luki izolacji błędów alokacji przy statusach i zamykaniu sesji.

## Diagnostyka lokalna i raporty klientów

`adc/connection_errors.hpp` definiuje `connection_failure(error, command)` bez alokacji i ograniczony opis ConnectionFailure. FailureAction rozróżnia odwracalną odpowiedź do źródła, terminalne odrzucenie źródła i natychmiastowe przerwanie wewnętrzne. Opisy są stałymi lokalnymi napisami; tokeny, nazwy protokołów, SID, IP i porty nie trafiają do tekstu diagnostycznego.

| Błąd połączenia | Działanie lokalne |
| --- | --- |
| Nieprawidłowy stan / podrobione źródło | Terminalne 244 / 240 |
| Brak odbiorcy | Odwracalne 140 z ogólnym opisem dla polecenia |
| Nieaktywne źródło CTM / błędne role RCM / błędna wiadomość | Odwracalne 100 |
| Wyłączenie / brak uprawnień | Odwracalne 125 |
| Obcy/zużyty plan, brak odbiorcy programowego, nieznany lub niespójny błąd/polecenie | Natychmiastowe przerwanie źródła bez kolejnej alokacji |

Pozostają wartości FC i kolejność błędów. Nieudane przygotowanie nie przekazuje oryginału ani echa E. Przyjęcie do kolejki nie daje domniemanego sukcesu. Błędy klientów 41 (nieobsługiwany protokół) i 42 (nieudane połączenie), zarówno odwracalne, jak i fatalne, zachowują sprawdzone PR/TO i dokładne bajty tokena. Inne/nieznane kody zachowują wcześniejszą walidację.

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html), rozdziały 3.3, 5.1, 5.3.1 i 5.3.8–5.3.12, oddziela kierowany kontekst klientów od statusu huba. Kierowany fatalny D/E STA jest przekazywany bez zamykania sesji huba. Poprawny fatalny HSTA dotyczy tylko jego nadawcy, bez pętli odpowiedzi. HSTA sukcesu/odwracalnego błędu nie kończy logowania ani nie nadaje uprawnień. Błędny STA nadal powoduje wcześniejsze terminalne odrzucenie protokołu.

## Niezmienny routing statusów

`adc/status_routing.hpp` dodaje StatusRouter, StatusDelivery, SharedStatusFrame, StatusRoutingError i StatusDispatchReport. Router pożycza SessionManager w wątku właściciela. Sprawdza gotowość/własność źródła B/D/E/F STA, stosuje dotychczasową listę publicznych pól, buduje jedną kanoniczną ramkę wyjściową i zachowuje ograniczoną niezmienną listę dokładnych ID gotowych sesji. Ramka posiada własny oczyszczony Message, kanoniczny zapis i typowany StatusCode. Kontrola spójności kodeka może również serializować wartości tymczasowe podczas przygotowania.

D wybiera dokładny cel, E cel i potem źródło; własny cel występuje raz. B wybiera gotowych użytkowników rosnąco według ID życia. F filtruje tę samą populację przez publiczne INF SU i wymagane/wykluczające selektory; SUP jest niezależny. Lista odbiorców jest ograniczona aktualną liczbą publicznych użytkowników i ustawionymi limitami. Limity treści i kodowanej ramki pozostają bez zmian.

Przed każdym wywołaniem odbiorcy ponownie sprawdzane są gotowość/własność źródła, gotowość celu i bieżące dopasowanie F. Nowe dopasowanie lub nowe logowanie nie rozszerza listy. Ponowne użycie SID odłączonego celu nie przekierowuje starego planu do nowej sesji. Utrata cechy F anuluje zachowaną kopię. Brak celu D/E przy dopuszczeniu daje wcześniejsze prywatne `ISTA 140 Status\srecipient\sunavailable`, bez oryginalnego echa.

Kopie dzielą jedną próbę. Obca domena lub brak odbiorcy programowego jej nie zużywa; poprawna wysyłka zużywa próbę przed ponownymi kontrolami i funkcjami odbiorców. Anulowanie jest wspólne i idempotentne. Zachowane dane/ramka/próba wytrzymują podmianę obiektu wywołującego w funkcji odbiorcy. Wyjątek odbiorcy propaguje się ze zużytą próbą, zapobiegając odtworzeniu częściowo zakolejkowanej operacji. Wysyłka i odczyt ramki nie alokują po przygotowaniu. Raporty liczą kopie zakolejkowane/odrzucone/anulowane, nie dostarczenie sieciowe ani udany transfer.

## Izolacja awarii i rozłączenia

LoginHub kolejkuje wspólne ramki STA z osobnymi pozycjami wysyłki i pełnym naliczeniem bajtów, w istniejących limitach klienta/huba. Odmowa kolejki lub błąd alokacji celu dotyczy tego odbiorcy; poprawne echo źródła E pozostaje niezależne. Błąd pamięci przygotowania/walidacji/diagnostyki dotyczy źródła. Źródło tracące gotowość w trakcie wysyłki anuluje pozostałe zachowane kopie.

`receive` obejmuje teraz dopuszczenie, walidację i terminalną diagnostykę STA/CTM/RCM granicą bad_alloc. Jawne `close` izoluje brak pamięci na komunikat zamknięcia. Walidacja STA przy zakończeniu wysyłki jest lokalna dla odbiorcy. Gdy diagnostyki nie da się bezpiecznie utworzyć, zamykana jest tylko odpowiednia sesja, bez rekurencyjnej alokacji błędów i automatycznego powtarzania. To izolacja wskazanych ścieżek, nie deklaracja obsługi każdej alokacji w całym serwerze.

Pozostaje kolejność terminalna: zachowanie częściowo wysłanego/przypiętego frontu TLS, anulowanie dalszego zwykłego wyjścia, ograniczona próba STA i własnego QUI oraz jednokrotne zwolnienie tożsamości/SID/kolejek przy rzeczywistym rozłączeniu. EOF, reset, niesprawny transport, pełna kolejka lub brak pamięci mogą uniemożliwić zawiadomienie. Wspólne ramki pozostają poprawne dla innych odbiorców po usunięciu jednej sesji. Łagodne TLS close_notify/reload nadal są przyszłymi pracami.

Uprawnienia połączeń nadal dotyczą CTM/RCM; raport błędu może wyjaśnić odmowę operacji. Hub nie wyznacza oczekującego transferu z TO, nie tworzy błędu klienta na podstawie zdarzenia jego gniazda, którego nie widzi, nie obniża ADCS i nie powtarza CTM/RCM. Nie obsługuje gniazd klientów ani bajtów plików. Nie dodano zależności, klucza INI, migracji SQL, endpointu, metryki ani sterowania WebAdmin. Treść błędów pozostaje poza logami, SQL, API/WebAdmin i licznikami Chat/Search.

## Weryfikacja

`transfers.errors` sprawdza działania/kody diagnostyki, zakresy i poziomy STA, własność/listę pól, własne echo, stałych odbiorców, obce/anulowane/skopiowane próby, brak alokacji wysyłki, zmiany F, wymuszone użycie starego SID, podmianę danych w funkcji odbiorcy i zużycie próby po wyjątku. Jednorazowe awarie alokacji w 400 przypadkach LoginHub obejmują przygotowanie źródła, kolejkę celu i sukces z izolacją obserwatora. Osobne przypadki sprawdzają wyjściową walidację STA, awarię diagnostyki zamknięcia, przypięty front, limity wyjścia i pełne zwolnienie zasobów.

`transfers.errors.process` obejmuje fragmentowane IPv4/IPv6, mieszane ADC/ADCS, CTM/RCM i D/E STA 141/142/241/242/nieznane kody, metadane, dalszy NORMAL, HSTA bez pętli odpowiedzi, własny fatalny status, błędne pola, ponowne logowanie/stary SID, reset TCP, dalszą komunikację i prywatność API/logów. Pozostają wcześniejsze szerokie testy STA/QUI, limitów czasu i backpressure TLS.

Cztery scenariusze rzeczywistego ncdc (`adc.transfererrors.ncdc`, `adc.transfererrors.adcs.ncdc`, `adcs.transfererrors.ncdc`, `adcs.transfererrors.adcs.ncdc`) przyjmują połączenie klienta, odczytują CSUP albo TLS ClientHello, resetują je, sprawdzają rzeczywiste 141 z tokenem dla nieobsługiwanego protokołu, przekazują 242 i potwierdzają działanie czatu/quit. ncdc 1.23.1 może obsłużyć błąd gniazda lokalnie bez DSTA do huba; test nie wymaga wymyślonego 142. Osobni surowi klienci sprawdzają przekazywanie 142/242. Poprawny TLS z weryfikacją certyfikatu pozostaje sprawdzany wcześniej; nowy przypadek TLS celowo kończy się przed poprawnym handshake.

Zobacz [STA/QUI](ADC_STATUS.md), [Connection Routing](CONNECTION_ROUTING.md), [Connection Security](CONNECTION_SECURITY.md), [Transfer Tokens](TRANSFER_TOKENS.md), [User Disconnect](USER_DISCONNECT.md) i [Testy](TESTING.md).
