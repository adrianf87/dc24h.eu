# Logowanie gości ADC — alpha-0.2.8

**Aktualny etap — alpha-0.2.9:** [STA/QUI](ADC_STATUS.md) dodaje walidację semantyki statusów/rozłączeń, kontrolowany routing B/D/E/F, dokładniejsze błędy i ograniczone końcowe ISTA/własne IQUI. Opisy historyczne poniżej zachowują zachowanie pierwotnych wersji.

Hub C++ kończy teraz logowanie gościa przez rzeczywiste nieblokujące porty TCP i OpenSSL TLS. `LoginHub` łączy istniejące moduły SUP, SID, INF, Tiger i automatu stanów; jedna instancja w wątku reaktora obejmuje wszystkie porty ADC/ADCS. Zarządza publiczną listą, rezerwacjami nicków/CID i ograniczonym uporządkowanym wyjściem. Konta, hasła, bany/role, czat, wyszukiwanie i transfery pozostają dalszymi etapami.

Gałąź rozpoczyna się od main `b8978491fa8e146527eba818f9bc1ab0a31aa90d`, zawierającego alpha-0.2.1–alpha-0.2.7. Main i historyczne gałęzie zostają zachowane; ten etap nie scala się automatycznie.

## Kolejność komunikacji i gotowość

1. Klient wysyła `HSUP ADBASE ADTIGR`; dodatkowe poprawne funkcje są dozwolone.
2. Hub wysyła `ISUP ADBASE ADTIGR`. Dopiero pełne wysłanie umożliwia rezerwację SID.
3. Hub wysyła zarezerwowany `ISID xxxx`. Dopiero ukończenie ramki wraz z LF przeprowadza połączenie z PROTOCOL do IDENTIFY.
4. Klient wysyła `BINF xxxx` z NI, ID i PD. Walidator sprawdza Tiger(zdekodowany PID)==CID, źródłowy SID, pola oraz adres klienta z jądra. Publiczne metadane nigdy nie zawierają PD ani ignorowanych nieznanych pól.
5. Kontroler przyjęcia rezerwuje unikalny nick/CID i zatwierdza gościa, przechodząc wewnętrznie do NORMAL. To jawna polityka gości; poprawna para PID/CID nie nadaje konta, roli operatora ani uprawnień.
6. Hub kolejkuje IINF z CT32/nazwą/opisem huba, publiczne INF wcześniej przyjętych klientów, a na końcu własny publiczny INF nowego klienta. Pełne wysłanie ostatniej ramki oznacza gotowość i opcjonalne zdarzenie `login_ready`. Samo parsowanie INF lub wewnętrzne NORMAL nie oznacza jeszcze gotowości.
7. Dotychczasowi klienci otrzymują oczyszczony INF nowej osoby. Poprawne aktualizacje INF w NORMAL są odsyłane i rozsyłane, z zachowaniem anulowania pól opcjonalnych. Rozłączenie publikuje IQUI oraz zwalnia nick, CID, wyjście i SID.

Lista obejmuje także przyjęte sesje z nadal wysyłanym powitaniem. FIFO zapewnia wzajemną widoczność równoczesnych logowań oraz wysłanie własnego początkowego INF przed późniejszymi dołączeniami. Nieznane pola i PID nie są rozsyłane. Aktualizacja eksportuje zachowany publiczny snapshot oraz same nazwy usuniętych wcześniej zachowanych pól. Nicki porównuje się dokładnie jako bajty NFC UTF-8, z rozróżnianiem wielkości liter; jest to lokalna polityka alpha. CID porównuje się w kanonicznym zweryfikowanym zapisie. CID/SID są publicznymi identyfikatorami, nie sekretami uwierzytelniania.

Hub odrzuca nieprawidłową kolejność, podszywanie pod SID, zajęty nick (222), zajęty CID (224), błędną parę PID/CID (227), brak BASE (245) i wspólnego hash (247), wysyłając fatalne ISTA ze stałym opisem. Pozostałe błędy INF zachowują dotychczasowe mapowanie. Surowe parametry nie trafiają do opisów statusu ani logów operatora. Niefatalny status 125 informuje o poprawnych stanowo komendach bez zaimplementowanej obsługi czatu/wyszukiwania/transferów; wiadomości i zapytania nie są przekazywane.

Komenda wejściowa nie omija niewysłanej odpowiedzi. Zbyt wcześnie przesłane komendy zawodzą na aktualnej bramce stanu. Po zakolejkowaniu fatalnego odrzucenia dalsze dane są ignorowane przez przyjęcie, chociaż liczniki składni/limitów mogą nadal zliczać odebrane ramki. Sesja opróżnia ograniczone wcześniejsze wyjście i status, po czym się zamyka; błąd transportu, limitu lub terminu może zamknąć ją od razu. EOF anuluje wyjście. Fatalne HSTA klienta także usuwa publiczną obecność. Ponowne połączenie tworzy nową sesję; sprzątanie jest idempotentne.

## Serializacja i własność wyjścia

`serializer.hpp` koduje zdekodowane Message, escapując spację, LF i backslash oraz dodając końcowy LF. Sprawdza zgodność nagłówka, command/kind, parametry, selektory funkcji, UTF-8/NFC i składnię BASE przez wspólny parser. Odrzuca puste parametry, błędną składnię i treść ponad 16384 zakodowane bajty. Wynikiem jest własny string lub stały SerializeError. Serializacja nie nadaje uprawnienia do wysyłania; kontroler logowania sam tworzy ograniczony profil odpowiedzi.

`LoginHub::output(id)` pożycza niezmienny niewysłany początek kolejki. `consume(id, bytes)` przyjmuje wyłącznie bajty faktycznie wysłane i zatwierdza zdarzenia sesji po pełnej ramce z LF. Częściowa wysyłka nie zatwierdza SUP/SID/gotowości. Dodawanie ramek nie zmienia adresu ani pozostałej długości pierwszej. Nieudana wysyłka zamyka klienta bez fikcyjnego ukończenia.

TCP używa nieblokującego send, MSG_NOSIGNAL i dokładnego przesunięcia. ADCS używa SSL_write_ex, zachowuje identyczny adres/długość przy WANT_READ/WANT_WRITE i kończy zapis przed inną operacją SSL. Ponawiany odczyt wymagający zapisu także poprzedza zapisy aplikacyjne. Każde połączenie ma stabilny bufor odczytu. epoll obserwuje wymagany kierunek; EPOLLOUT tylko przy potrzebie. Tura wykonuje maksymalnie osiem ramek wyjściowych i osiem odczytów po 8192 bajty, sprawdzając stop. Buforowane dane OpenSSL są jawnie ponownie planowane.

Limity zakodowanego wyjścia wynoszą **256 KiB na połączenie i 16 MiB na hub**; częściowo wysłana ramka liczy się w całości do zakończenia. Kontrola następuje przed wstawieniem. To stałe domyślne transportu, bez opcji INI; moduł udostępnia mniejsze limity do testów/osadzania. Obiekty Message, węzły kolejki, snapshoty rejestru, wejście i bufory OpenSSL dodają ograniczony narzut ponad liczbę bajtów sieciowych. Przepełnienie oznacza natychmiastowe zamknięcie danego odbiorcy, bez zmiany przypiętego początku podczas oczekującego zapisu TLS. QUI jest kolejkowane dla pozostałych przyjętych sesji przed zwolnieniem starej własności; FIFO zachowuje kolejność starego INF, QUI i ewentualnej późniejszej nowej obecności. Przeciążenie globalne może zamknąć wielu odbiorców do odzyskania zasobów.

Początkowa lista użytkowników musi mieścić się w ograniczonym budżecie kolejki klienta/huba; logowanie, którego pełny snapshot się nie mieści, jest zamykane bez publikowania tożsamości. Strumieniowanie snapshotów nie jest zaimplementowane.

Zatrzymanie zamyka porty i klientów bez oczekiwania na kolejki lub close_notify. TLS close_notify/reload oraz produkcyjne testy obciążenia/fuzz/fault injection pozostają osobnymi zadaniami.

## Terminy i konfiguracja

INI `[security]` dodaje `login_timeout=30` i `output_timeout=10`, obie opcje w zakresie 1–3600 sekund. Brak nowych przełączników CLI; wartości ustawia INI lub biblioteczne Options. Nadal działają limity TLS, niedokończonej ramki, bezczynności, częstości i IP.

Bezwzględny termin logowania biegnie od przyjęcia TCP, obejmuje TLS i kończy się dopiero po pełnym wysłaniu własnego INF. Keepalive i częściowe dane go nie odnawiają. Termin wyjścia zaczyna się przy oczekującym wyjściu, odnawia po wysłaniu bajtów i znika po opróżnieniu kolejki. Odczyty go nie przedłużają. Reaktor sprawdza terminy okresowo (około 250 ms) oraz przed/po wejściowym I/O; planowanie może dodać opóźnienie.

Nowe opcjonalne zdarzenia INFO: `login_ready`, `login_rejected`, `login_timeout`, `output_timeout`, `output_limit`, `write_error`. Terminy/przepełnienia zwiększają security_closed; zwykłe odrzucenia protokołu nie. Budżet zdarzeń pozostaje ograniczony. Angielskie ostrzeżenia mają polskie tłumaczenia. Opisy ADC pozostają angielskie niezależnie od języka operatora. Schemat API v1 i WebAdmin pozostaje bez zmian: active_connections liczy gniazda, adc_frames składnię; żaden nie liczy zalogowanych osób.

## Testy i pozostały zakres

`adc.login` sprawdza escapowanie/limity, częściowe wysyłki, równoczesne przyjęcia, własny INF na końcu, prywatność pól, aktualizacje/anulowanie, duplikaty, podszywanie, sprzątanie, stabilność początku oraz limity indywidualne/globalne. `adc.login.process` uruchamia rzeczywisty C++ TCP/OpenSSL: wspólna obecność, fragmentacja, odrzucenia duplikatów/Tiger, reconnect, bezwzględny termin mimo keepalive i izolacja wolnego odbiorcy. `adcs.crypto` wymusza blokadę zapisu małym buforem gniazda i sprawdza brak utraty/powtórzeń zdekodowanych bajtów.

`adc.ncdc` i `adcs.ncdc` używają tymczasowych PTY ncdc, kończą logowanie i ponowne połączenie do C++. ncdc wysyła próbę czatu w NORMAL i wyświetla oczekiwany status braku implementacji, potwierdzając przyjęcie własnego INF. Relay ADCS przekazuje bajty bez zmian: TLS kończy się w C++, a HSUP/BINF/tekst nie występują jawnie w zapisie transmisji. Wcześniejsze testy modułów SUP/SID/INF/tożsamości zachowują historyczny węższy zakres.

Polityka kont/haseł nie jest włączona: wersja przyjmuje poprawnych gości, nie czyta wierszy kont i nie używa niskopoziomowych metod zatwierdzania hasła. Testy nie oznaczają kompletnego produkcyjnego huba, uwierzytelniania kont, gotowego czatu/wyszukiwania/transferów, zmierzonej skalowalności ani audytu bezpieczeństwa. Zobacz [TESTING.md](TESTING.md) i [ROADMAP.md](ROADMAP.md).

Podstawa: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), szczególnie 3.5, 5.1, SUP/SID/INF/STA/QUI; kontrakt ponawiania transportu: [OpenSSL SSL_write](https://docs.openssl.org/3.0/man3/SSL_write/).
