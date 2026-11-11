# Private Chat — alpha-0.4.4

**Bieżący etap — alpha-0.4.9:** [Anti-Flood](ANTI_FLOOD.md) dodaje włączone budżety liczby i odkodowanych bajtów B/D/E dla połączenia, konfigurowane w `[chat]`. Nadmiarowe wiadomości są odrzucane przed rozsyłaniem z ograniczoną odwracalną diagnostyką; walidacja i ogólne limity pozostają.

**Etap dostarczony w alpha-0.4.8:** [Chat Validation](CHAT_VALIDATION.md) centralizuje sprawdzanie tekstu, SID i gotowości/własności nadawcy w obu adapterach. Wybór życia odbiorcy i ponowna kontrola dispatch pozostają obowiązkowe; systemowe IMSG jest oddzielone.

**Etap dostarczony w alpha-0.4.7:** [Hub Messages](HUB_MESSAGES.md) dodaje zaufane systemowe IMSG powiązane z życiem sesji i stałe powitanie po własnym BINF. Klient nie może nadawać komunikatów systemowych; zachowano limity kolejek i gotowość przy końcowym LF.

**Etap dostarczony w alpha-0.4.6:** [Message Flags](MESSAGE_FLAGS.md) centralizuje dekodowanie PM/ME i kanoniczne wyjście kodeka oraz obu adapterów czatu. Brak ME, ME0 i ME1 pozostają rozróżniane; PM nie nadaje uprawnień do dostarczania.

**Etap dostarczony w alpha-0.4.5:** [Message Routing](MESSAGE_ROUTING.md) centralizuje wybór zakresu MSG i dostarczanie do ograniczonych kolejek. Plany są powiązane z domeną macierzystego Chat Core i ID życia sesji; prywatne wiadomości nigdy nie trafiają zastępczo na czat publiczny.

Etap pochodzi bezpośrednio z main `b7aff0bb6a886b96e31dbf63321271861b351d88`, po Public Chat. Wcześniejsze gałęzie/historia i publiczny czat są zachowane. Domyślny język serwera pozostaje EN/US UTF-8; dodano dokumentację PL i zaktualizowano pomoc US/PL.

## DMSG, EMSG i PM

[Oficjalna specyfikacja routingu/MSG ADC](https://adc.sourceforge.io/ADC.html) definiuje direct do celu oraz echo do celu i źródła. Obsługę realizują `adc::PrivateChat` oraz niezależne od protokołu plany Chat Core.

| Wejście | Odbiorcy |
| --- | --- |
| `DMSG source target text` | Tylko cel |
| `EMSG source target text` | Cel, następnie źródło |
| DMSG/EMSG do siebie | Ta sesja raz |

Brak przejścia awaryjnego do broadcast. Nadawca i cel muszą być istniejącymi, w pełni ready sesjami. Cel anonimowy/czekający na powitanie/closing/closed/brakujący jest niedostępny. BMSG bez PM nadal obsługuje PublicChat. BMSG z PM, FMSG/HMSG i prywatne grupy pozostają nieobsługiwane. AW nie blokuje prywatnego ruchu do ready.

PM jest opcjonalne dla kierowanego routingu. Jeśli występuje, musi wskazywać SID źródła zwykłej prywatnej rozmowy i jest zachowywane jako PM<source>. Inna poprawna grupa PM zwraca recoverable ISTA 125, bez wysyłania treści. Brak PM pozostaje brakiem; serwer nie dodaje fikcyjnych metadanych rozmowy. Wiadomość bez PM nadal trafia tylko do wybranych sesji, niezależnie od sposobu wyświetlania przez klienta. Błędne/duplikowane PM/ME zachowują istniejącą ścieżkę fatal.

Tekst, Unicode/NFC, spacje, zakodowane LF/backslash i opcjonalne akcje zachowują politykę kodeka. Prywatne `/me` działa. Nowa ramka zawiera tylko typ, źródło, cel, tekst i opcjonalne PM/ME; nieznane pola, także podobne do PD, są usuwane. ME jest kanoniczne ME0/ME1, z rozróżnieniem braku i false. Tekst nie jest obcinany, przycinany ani interpretowany jako komenda huba.

## Cel i Core

`chat::Directed` jest samodzielnym niemutowalnym planem: ID/klucz nadawcy, ID życia/klucz celu, tekst, akcja i stała tablica dwóch ID. Widok odbiorców zawiera jeden lub dwa wpisy, bez duplikatu dla wiadomości do siebie. `Core::prepare_direct` sprawdza gotowość/powiązanie źródła, tekst oraz gotowość celu wyszukanego dokładnie po SID. Wyszukiwanie ma ograniczone O(log n), bez skanowania całej listy.

`Core::dispatch(Directed, DirectedSink)` ponownie sprawdza źródło oraz oryginalne ID życia i klucz celu przed kolejkami. Nowa sesja z tym samym SID nie przejmuje starego planu: recipient_unavailable oznacza brak treści i echa. Rename celu zachowujący SID/ID pozostaje prawidłowy. Każda wybrana sesja jest ponownie sprawdzana przed callbackiem. Core nie zależy od ADC, transportu, TLS, SQL, loggera ani historii.

DispatchReport liczy queued, unavailable i refused. Echo jest kopią dla źródła, nie potwierdzeniem dostarczenia/przeczytania. Po rozpoczęciu poprawnego dispatch odmowa kolejki celu nie anuluje niezależnego echa, a odmowa nadawcy nie cofa już zakolejkowanego celu. Brak atomowości między gniazdami i automatycznych ponowień. Callback musi izolować wyjątki; ponowienie częściowej publikacji może duplikować wyjście.

## Adapter i błędy

`include/dc24h/adc/private_chat.hpp` i `src/adc/private_chat.cpp` definiują PrivateChatError, PrivateDelivery i niemutowalny PrivateChatFrame. Prepare sprawdza także ręczne obiekty Message, politykę D/E/PM, uzyskuje Directed, wybiera dozwolone metadane i serializuje pełną ramkę przed zmianą kolejek. Cel i echo współdzielą const pakiet. Plan po przeniesieniu jest odrzucany. Błąd przygotowania nie mutuje ani częściowo nie rejestruje użytkowników.

| Warunek | Wynik runtime |
| --- | --- |
| Brak/niegotowy cel przy przyjęciu lub przed dispatch | Recoverable ISTA 140 ze stałym opisem i FCDMSG/FCEMSG; bez treści/echa |
| Poprawna nieobsługiwana grupa PM | Recoverable ISTA 125 z FOURCC; bez treści |
| Podszycie źródła/błędne znane pola | Fatal ISTA 240, własne IQUI, ograniczone sprzątanie |
| Nadawca przed NORMAL lub ukończeniem powitania | Fatal ISTA 244 |
| Awaria alokacji przygotowania | Internal_error nadawcy i natychmiastowe sprzątanie bez publikacji |
| Limit/alokacja kolejki odbiorcy | Zamknięcie tego odbiorcy; drugi wybrany odbiorca może kontynuować |

Błędy niedostępności nie odbijają tekstu, adresu, klucza celu, grupy ani sekretów. Brak skrzynki offline, grup, moderacji/historii i uprawnień kont rejestrowanych w tym etapie.

## Kolejki, limity i prywatność

LoginHub posiada zwykłe ramki, wspólne publiczne lub wspólne prywatne pakiety. Jeden typowany helper stosuje dotychczasowe limity klienta/globalne, FIFO i reguły przypiętego frontu do obu trybów. Każdy odbiorca obciąża pełny rozmiar ramki, także przy częściowym wysłaniu. DMSG liczy jeden wpis; EMSG jeden lub dwa. Ukończenie, anulowanie ogona i rozłączenie zwalniają obciążenia raz, a pamięć fizyczna trwa do ostatniego właściciela.

Każda kolejka ma własny offset. Rozłączenie źródła nie unieważnia pakietu celu, ponowny SID nie kieruje starej treści do nowego tokenu transportu. Identified/pending nie dostaje zaległych wiadomości prywatnych. Prywatne/publiczne/status/lista zachowują FIFO odbiorcy. Pozostaje kontrola stanu przy ukończeniu wyjścia. Limit treści ramki to 16384 zakodowane bajty bez LF, z nagłówkami/flagami/separatorami/escaping; D/E z PM zostawia mniej miejsca na tekst niż BMSG. Bez nowych limitów, opcji, zależności, migracji SQL, pól JSON API i funkcji WebAdmin.

Prywatność oznacza routing do wybranych sesji przez hub. Hub przetwarza tekst w pamięci do przekazania. Treść i nieznane pola nie są logowane, utrwalane, kopiowane do obcych kolejek ani eksportowane przez API/WebAdmin. Działają dotychczasowe statystyki liczbowe. Nie deklaruje to pełnej odporności procesu na OOM, zdalnego CI ani wydajności produkcyjnej.

## Weryfikacja

`chat.private` sprawdza odbiorców/kolejność D/E, wspólny pakiet, pojedyncze wysłanie do siebie, opcjonalne PM, grupy/źródło/stan, dokładne granice, przenoszenie planu, ponowny SID, rename, stare źródło, dispatch bez alokacji z istniejącym sink i wycofanie przygotowania. Testy kolejek obejmują offsety, sprzątanie źródła, limit wolnego celu/przypięty front, niezależne echo, wykluczenie powitania i awarie pojedynczych kolejek bez wpływu na obserwatora.

`chat.private.process` używa prawdziwych mieszanych ADC/ADCS, obcego obserwatora i pending klienta. Sprawdza D tylko do celu przez znaczniki publicznego FIFO, E, wiadomości do siebie, Unicode/LF/akcje, sanitizację, niedostępność/grupy, direct bez PM, brak zaległości, dokładne 16 KiB, reconnect, stary SID, podszycie i prywatność logów/API.

Prawdziwy ncdc ADC/ADCS wysyła `/msg` do obserwatora, odbiera prywatną odpowiedź na ekranie i wysyła prywatne `/me`. Zachowano testy publiczne, listy/INF/reconnect/statystyk oraz roundtrip plaintext. Przed publikacją wymagane są pełne regresje serwera i WebAdmin. Inne systemy/klienty, pomiary obciążenia, grupy/offline/role/moderacja/historia pozostają osobnymi etapami.
