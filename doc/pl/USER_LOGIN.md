# User Login — alpha-0.3.8

**alpha-0.7.10 Roles:** [sprawdzone role kont](ROLES.md) są teraz przypisane do gotowych sesji, nadają uprawnienia Search/Transfers i pozwalają personelowi unieważniać sesje niższych ról. Limity globalne i jawne blokady połączeń nadal obowiązują. Zmiany roli w bazie wymagają restartu; bez nowej migracji SQL i publicznego API administracji.

**alpha-0.7.9 Auth Sessions:** [sesje kont związane z połączeniem](AUTH_SESSIONS.md) dodają atomowe powiązanie przy gotowości, zaufane odczyty według konta i unieważnianie połączenia. Zamykanie natychmiast usuwa uwierzytelnienie. Bez nowej migracji SQL, automatycznych ról i sesji przeglądarkowych. CI korzysta z [runnerów GitHub](GITHUB_CI.md) bez czekania na wynik.

**Etap dostarczony w alpha-0.3.12:** [Limits](LIMITS.md) dodaje konfigurowalne limity sesji/użytkowników wspólne dla ADC/ADCS, ograniczone limitem połączeń. SessionManager transakcyjnie sprawdza pojemność; LoginHub odrzuca pełny rejestr przed przygotowaniem powitania/listy przez ISTA 211 i własne IQUI. Rezerwacje closing liczą się aż do sprzątania. Starsze opisy zachowują zakres historyczny.

Etap powstał bezpośrednio z main `cc2a52603d0e67a6de722c783d84922f204ca0db`, po scaleniu alpha-0.3.7 User Registry. Poprzednie etapy są już obecne. Gałąź alpha-0.3.8 zachowuje main/historię i nie jest scalana automatycznie.

## Odpowiedzialność i wymagania

`adc::UserLogin` jawnie łączy poprawną identyfikację ADC z zarządzaną sesją publiczną. Pożycza SessionManager w wątku reaktora; nie posiada gniazd, kolejek, poświadczeń, kopii User ani zegara. Nie można go kopiować/przenosić, a manager musi istnieć dłużej. Wywołujący podaje ClientSession należącą do danego lokalnego SessionId; LoginHub pobiera tę parę z tego samego Peer. Moduł nie uwierzytelnia samodzielnie identyfikatora transportu.

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html) rozdziela negocjację protokołu, identyfikację, opcjonalną weryfikację i normalną pracę. Ta wersja nadal przyjmuje wyłącznie gości. ClientSession najpierw sprawdza SUP/hash/SID, źródło początkowego INF, wymagane NI/ID/PD, Tiger(PID)==CID i metadane publiczne. LoginHub jawnie zatwierdza zweryfikowaną tożsamość jako gościa istniejącą bramką ClientSession. Protokołowy NORMAL nie oznacza jeszcze gotowości aplikacyjnej: powitanie może być w kolejce.

`UserLogin` nie parsuje surowego INF, nie haszuje ponownie PID i nie implementuje kont, haseł/PAS, ról ani trwałych danych kont. Przyjmuje wyłącznie złożoną ClientSession, nie dowolny User/ClientInfo/tekst. ClientInfo już odrzuciło PID; używany jest publiczny wynik VerifiedIdentity. Ogólne fabryki User i indeksy nie zastępują dowodu ADC.

## Kontrakt rejestracji

`register_guest(id, client, now)` wymaga istniejącego negocjującego modelu, NORMAL po jawnym zatwierdzeniu tożsamości, zwalidowanego ClientInfo, poprawnego niezerowego przydzielonego SID i publicznego CID zgodnego z wynikiem Tiger. Wspólny adapter `user_from_info` projektuje ograniczony User/profil. Wszystkie alokacje projekcji poprzedzają modyfikację. SessionManager następnie sprawdza monotoniczny czas, unikalność SID/CID/nicka i transakcyjnie przygotowuje UserRegistry/NickManager przed zatwierdzeniem fazy identified.

Sukces rejestruje User raz z publicznym wyszukiwaniem SID/CID/nicka, bez gotowości. Powtórzenie oraz model accepted, identified, ready, closing lub closed są odrzucane przez cykl życia. IDENTIFY z poprawnym INF, ale bez jawnego zatwierdzenia gościa, również jest odrzucane. Typowany błąd lub wyjątek alokacji zachowuje model/indeksy. Moduł nie modyfikuje ClientSession także przy błędzie i nie cofa jej wcześniejszego zatwierdzenia protokołu. Za odrzucenie/zamknięcie odpowiada wywołujący. Testy samodzielne mogą ponowić po błędzie alokacji z tą samą ClientSession; LoginHub przechwytuje bad_alloc wyłącznie wokół tej rejestracji i wymusza natychmiastowe internal_error bez alokowania ramki odrzucenia. Wcześniejsze alokacje protokołu/powitania i późniejsze rozsyłanie obecności są poza tą wąską ochroną; nie deklaruje się pełnego odtwarzania po wyczerpaniu pamięci systemu.

`UserLoginResult` jest opcjonalnym wariantem UserLoginError, RegistryError lub SessionError. UserLoginError rozróżnia invalid_protocol_state, unverified_identity (także brak możliwości projekcji publicznego modelu) i identity_mismatch. Brak sesji, kolizje, faza i wsteczny czas zachowują typy managera. `user_login_status` mapuje je na stałe istniejące statusy ADC: CID 224, nick 222, limit 211, stan protokołu/cyklu życia 244, niezweryfikowana tożsamość 227, niezgodność tożsamości 240, błędny model 243 oraz naruszenia wewnętrzne/brak/czas/SID 210. Błędy nie zawierają danych wejściowych klienta.

## Koniec powitania i kolejność działania

`complete_welcome(id, client, now)` wymaga identified i NORMAL, ponownie sprawdza zweryfikowaną tożsamość oraz porównuje SID/CID/nick z rejestracją. Potem wywołuje mark_ready SessionManager z dostarczonym czasem. Brakujący, obcy, nieaktualny, zamykany lub gotowy model nie może zakończyć powitania. Sukces występuje raz; odrzucenie nie modyfikuje modelu. Zakończenie jest noexcept i nie alokuje, także przy sprawdzaniu CID przez parsowanie stałego rozmiaru zamiast formatowania tekstu.

Moduł nie liczy bajtów: właściciel wyjścia musi wywołać zakończenie dopiero po przyjęciu końcowego LF własnego INF przez transport. Bezpośrednie wywołanie jest zaufaną operacją integracji, nie niezależnym dowodem dostarczenia. Tę granicę wymusza LoginHub:

1. Sprawdza początkowy INF i odrzuca zajęty nick/CID dotychczasowymi bramkami.
2. Zatwierdza gościa i umieszcza w ograniczonym FIFO INF huba, listę przyjętych użytkowników oraz własny INF z oznaczeniem końca powitania.
3. Rejestruje przez UserLogin dopiero po poprawnym umieszczeniu wszystkich wymaganych ramek. Przepełnienie powitania nie publikuje użytkownika. Publiczny INF jest rozsyłany dopiero po rejestracji. Wpisy identified czekające na powitanie nadal są zarezerwowane i ujęte na listach, zachowując kolejność równoczesnych logowań.
4. Zatwierdza wychodzące komunikaty ADC po wysłaniu całej ramki. Po własnym INF wywołuje zakończenie UserLogin i zwraca pierwszy sygnał gotowości. Częściowe wyjście, zwykłe aktualizacje i dane końcowe nie dają gotowości.

Błędy rejestracji korzystają z istniejącego ograniczonego ISTA/własnego IQUI. Naruszenie niezmiennika przy końcu powitania wymusza zamknięcie internal_error bez gotowości. Limity kolejek, przypięty bufor TLS, terminy logowania/wyjścia i liczniki oczekujących pozostają w swoich modułach. Rezerwacje closing/closed trwają do usunięcia przez SessionManager. Rozłączenie finalizuje/usuwa model, indeksy i kolejkę dotychczasową ścieżką; reconnect dostaje nowy czas życia i może ponownie użyć CID/nicka.

Nie dodano zależności, ustawień, migracji SQL, pól JSON API, funkcji WebAdmin, komunikatów operatora ani tłumaczeń. Językiem tworzenia i domyślnym runtime pozostaje EN/US UTF-8, z istniejącymi languages/us i languages/pl. Dokumentacja User Login ma odpowiedniki w doc/us i doc/pl.

## Weryfikacja

`users.login` sprawdza brak sesji/negocjacji, błędny dowód i brak zatwierdzenia, wsteczny czas przyjęcia, pojedynczą rejestrację, publiczną projekcję i indeksy, kolizje SID/CID/nicka, obcy i nieaktualny nick przy końcu powitania, wsteczny czas zakończenia, pojedynczą gotowość, odrzucenie closing i sprzątanie. Ograniczone błędy alokacji C++ obejmują projekcję i wszystkie zarządzane indeksy z ponowieniem; zerowy budżet potwierdza brak alokacji przy zakończeniu. Osobna kampania alokacji LoginHub sprawdza chroniony błąd rejestracji: brak publicznego User/gotowości, natychmiastowe zamknięcie i pełne usunięcie kolejki/modelu. Wcześniejsze punkty mogą przekazać wyjątek; nie wstrzykuje się błędów alokatora C utf8proc ani systemu.

Testy LoginHub potwierdzają rejestrację identified przed gotowością, brak gotowości po INF huba/części własnego INF, gotowość dokładnie po końcowym LF i brak rejestracji przy przepełnieniu wymaganego powitania. Istniejące testy cyklu życia/danych końcowych obejmują anulowane powitanie i odrzucenie przy częściowym wyjściu.

`users.login.process` sprawdza rzeczywiste ADC/ADCS w obu kierunkach mieszanych transportów: brak routingu niezidentyfikowanego SID, brak publicznego join/quit po błędnym Tiger, rejestrację fragmentowanego poprawnego INF, brak przejęcia CID, routing gotowego SID, sprzątanie po fatalnym zamknięciu i reconnect z tym samym CID. Sprawdza liczbę zdarzeń gotowości/odrzuceń i brak prywatnych danych w logach. Istniejący rzeczywisty ncdc ADC/ADCS osiąga NORMAL i reconnect z polskimi nickami. Lokalne wyniki opisuje [TESTING.md](TESTING.md); nie oznaczają zdalnego CI, pełnej macierzy systemów, testów obciążenia ani pełnego huba.
