# Public Chat — alpha-0.4.3

**Bieżący etap — alpha-0.4.10:** [Chat Configuration](CHAT_CONFIGURATION.md) grupuje startowe ustawienia czatu i dodaje limit odkodowanego tekstu, niezależne przełączniki publicznego/prywatnego/akcji i opcjonalne własne powitanie. Domyślne wartości zachowują działanie; odwracalne odmowy nie omijają walidacji ani Anti-Flood.

**Etap dostarczony w alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

**Etap dostarczony w alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizuje sprawdzanie tekstu, SID i gotowości/własności nadawcy w obu adapterach. Wybór życia odbiorcy i ponowna kontrola dispatch pozostają obowiązkowe; systemowe IMSG jest oddzielone.

**Etap dostarczony w alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) dodaje zaufane systemowe IMSG powiązane z życiem sesji i stałe powitanie po własnym BINF. Klient nie może nadawać komunikatów systemowych; zachowano limity kolejek i gotowość przy końcowym LF.

**Etap dostarczony w alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizuje dekodowanie PM/ME i kanoniczne wyjście kodeka oraz obu adapterów czatu. Brak ME, ME0 i ME1 pozostają rozróżniane; PM nie nadaje uprawnień do dostarczania.

**Etap dostarczony w alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizuje wybór zakresu MSG i dostarczanie do ograniczonych kolejek. Plany są powiązane z domeną macierzystego Chat Core i ID życia sesji; prywatne wiadomości nigdy nie trafiają zastępczo na czat publiczny.

**Etap dostarczony w alpha-0.4.4:** [Private Chat](PRIVATE_CHAT.md) dodaje DMSG tylko do celu i EMSG do celu oraz nadawcy, pojedyncze wiadomości do siebie, powiązanie celu z ID życia i kontrolę PM. Czat publiczny pozostaje oddzielony; grupy/historia/moderacja to kolejne etapy.

Gałąź pochodzi bezpośrednio z main `165c3596d7b2488599bbe6b8297f16cd35825f28`, po scaleniu Chat Core. Kodek MSG i podstawowe publiczne dostarczanie już istniały; ten etap wyodrębnia adapter BMSG i usuwa kopiowanie/serializowanie kolejkowanej ramki osobno dla każdego odbiorcy. Historia i gałęzie są zachowane. Domyślny język serwera to EN/US UTF-8; dostępna jest dokumentacja PL.

## Kontrakt BMSG

`adc::PublicChat` łączy kodek alpha-0.4.1 z niezależnym od protokołu chat::Core. Interfejs prepare/dispatch znajduje się w `include/dc24h/adc/public_chat.hpp`, implementacja w `src/adc/public_chat.cpp`. Moduł nie posiada gniazd, TLS, SQL, loggera ani trwałej kolejki. LoginHub dostarcza ograniczone kolejki transportowe. Core nadal nie zależy od ADC, bez cyklu bibliotek.

Czat główny przyjmuje BMSG bez PM. Każdy ready, także nadawca, jest wybierany raz według ID życia. AW2 pozostaje deklaracją klienta; opcjonalne pomijanie nie jest stosowane. `/me` zachowuje kanoniczne ME0/ME1 oraz różnicę między brakiem a false. Niepusty tekst ze spacji, Unicode/NFC, zakodowane LF i backslash pozostają treścią. Serwer nie dodaje nicka ani nie interpretuje komend; klient rozwiązuje SID przez aktualną listę użytkowników.

Obowiązują [oficjalne reguły broadcast/MSG ADC](https://adc.sourceforge.io/ADC.html). Rozmowy prywatne/direct/echo/feature/hub, grupy, historia, moderacja, uprawnienia, offline, potwierdzenia i automatyczne ponowienia pozostają kolejnymi etapami. DMSG/EMSG/FMSG/HMSG i BMSG z PM otrzymują recoverable ISTA 125. Zamiar prywatny nigdy nie jest zamieniany na publiczny. Podszycie, błędny stan/PM/ME i składnia zachowują dotychczasowe STA/QUI/odrzucanie.

## Przygotowanie i błędy

`PublicChat::prepare(sender, message)` sprawdza całe wejście, typ B, brak PM i przyjęcie przez Core. PublicChatError rozróżnia invalid_message, unsupported_type, private_message, invalid_state i wrong_source. Odczytany Message nie dowodzi własności źródła ani gotowości. Sprawdzane są też obiekty zbudowane ręcznie; nieznane pola nie omijają limitów.

Adapter tworzy nowy Message wyłącznie ze źródła, tekstu i opcjonalnego kanonicznego ME. Nieznane pola, także podobne do PD, są usuwane. Wyjście jest serializowane raz podczas przygotowania SharedPublicChatFrame. Do ukończenia przygotowania żadna kolejka odbiorcy się nie zmienia. PublicBroadcast posiada plan Core i wspólny pakiet. Błąd walidacji/alokacji nie publikuje wiadomości ani nie mutuje rejestru.

Pozostają limity aplikacji: 16384 zakodowane bajty treści ramki bez LF, 128 parametrów i 128 selektorów. Nagłówek/separatory i rozwinięte sekwencje escaping wchodzą do budżetu. Kolejkowana ramka ma najwyżej 16385 bajtów z LF. Bez obcinania, przycinania i cichej normalizacji tekstu. Nie dodano nowych limitów czatu/flood, kluczy konfiguracji ani zależności.

## Wspólna ramka i kolejki

PublicChatFrame tworzy wyłącznie PublicChat; klasa jest niekopiowalna i nieprzenoszalna. Posiada sprawdzony Message i tekst wire z LF, udostępniając wyłącznie const. PublicBroadcast przechowuje `shared_ptr<const PublicChatFrame>` i plan życia Core. Kopie planu współdzielą pakiet; plan po przeniesieniu zwraca invalid_text przy dispatch. Pakiet nie posiada wskaźników modeli/sesji, nicka, CID, adresu ani sekretów.

`PublicChat::dispatch(plan, sink)` korzysta z Core do ponownego sprawdzenia źródła i każdego ID odbiorcy. Każdy otrzymuje ten sam obiekt i pamięć wire. Usunięty/zamykany odbiorca jest pomijany, późniejszy klient nie dostaje starej wiadomości, a ponownie użyty SID nie przejmuje starego ID życia. Brak sink daje missing_sink. False zwiększa refused, queued oznacza przyjęcie do wyjścia, nie odbiór/przeczytanie. Wyjątki mają kontrakt częściowej publikacji Core; LoginHub izoluje błędy alokacji pojedynczych kolejek i nie ponawia częściowej publikacji.

Ramki kolejki LoginHub są wariantem zwykłego posiadanego protokołu i wspólnego pakietu publicznego. Powitanie, INF, status i QUI zachowują zwykłą własność i zakończenia. Publiczne enqueue zapisuje shared pointer bez kopiowania Message/tekstu i bez tworzenia kolejnego wire. Każdy odbiorca ma własny offset, miejsce FIFO i pełne logiczne obciążenie bajtowe. Kontrola protokołu/stanu przy zakończeniu wysyłania pozostaje.

Wspólna pamięć nie zwiększa pojemności: rozmiar całej ramki każdego odbiorcy obciąża jego limit i globalny logiczny limit huba. Częściowo wysłana ramka nadal zajmuje pełny budżet do zakończenia. Zakończenie, anulowanie ogona i rozłączenie zwalniają każdy budżet raz. Pamięć fizyczna znika po zwolnieniu ostatniego właściciela. Metadane kolejek i chwilowy plan nadal wymagają ograniczonych alokacji; nie deklaruje się benchmarku ani dokładnego fizycznego budżetu pamięci.

Przypięty front TCP/TLS zachowuje adres przy dopisywaniu innych kolejek, częściowych wysłaniach, rozłączeniu nadawcy i ukończeniu innych odbiorców. Closing zachowuje front, usuwa późniejsze zwykłe ramki, następnie dodaje ISTA/własne IQUI w kolejności. Offset jednego odbiorcy nie zmienia wire ani offsetów innych. Po terminalnym QUI nie dodaje się publicznej wiadomości. Odmowa limitu/awaria kolejki zamyka tylko danego odbiorcę; inni kontynuują. Awaria przed publikacją zamyka nadawcę. Nie oznacza to pełnej odporności procesu na OOM.

## Runtime i prywatność

Czat dostają wyłącznie ready; identified/czekający na powitanie, anonimowy i closing nie otrzymują zaległości. INF i późniejsze BMSG zachowują kolejność reaktora/FIFO. Klient renderuje tekst; treść i nieznane pola nie są logowane, zapisywane ani eksportowane w WebAdmin/API. Działają dotychczasowe statystyki liczbowe. Bez nowego JSON API, SQL, listenera, klucza komunikatu operatora i funkcji WebAdmin. Wersja to alpha-0.4.3; dotychczasowe katalogi US/PL nadal obowiązują.

## Weryfikacja

`chat.public` obejmuje typowane odmowy, 270 odbiorców z tym samym pakietem, dispatch bez alokacji z istniejącym sink, własność/przenoszenie planu, granice zwykłych i zakodowanych ramek, wycofanie alokacji oraz równość wskaźników kolejek. Sprawdza niezależne offsety/budżety, sprzątanie nadawcy, przypięty sufiks przed fatal STA/QUI i końcowe zwolnienie obciążeń.

`chat.public.process` używa mieszanych prawdziwych ADC/ADCS: uporządkowane serie, INF/BMSG, Unicode/akcje, sanitizacja, AW2, dokładnie 16 KiB, odzyskanie po przekroczeniu, odmowa prywatnego zamiaru, publiczne znaczniki kolejności, zakodowane spacje i prywatność logów. Istniejące testy chat.core/chat.adc/chat.process zachowują kontrolę życia, wolnych odbiorców, PM, awarii i reconnect. Kampania alokacji zmienia pozycje bloków deque, ponieważ wspólne enqueue często nie alokuje.

Prawdziwy ncdc ADC/ADCS dodaje publiczny tekst Unicode/backslash/spacje do testów przychodzących/wychodzących, obserwatora, /me, reconnect i roundtrip plaintext. Wymagane są pełne regresje serwera i WebAdmin. Zdalne CI, pełna macierz systemów/klientów, produkcyjne obciążenie i pomiary wydajności wymagają osobnych dowodów.
