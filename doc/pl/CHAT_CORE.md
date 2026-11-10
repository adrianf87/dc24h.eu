# Chat Core — alpha-0.4.2

**Bieżący etap — alpha-0.4.3:** [Public Chat](PUBLIC_CHAT.md) dodaje osobny adapter polityki i rozsyłania BMSG oraz jedną wspólną niemutowalną ramkę na publikację. Offsety i logiczne budżety odbiorców pozostają niezależne; zachowano bramki źródła/stanu/prywatności i FIFO.

Etap utworzony bezpośrednio z main `508b43df7316ff1a45a41374ca0a38ce2a0ab6f0`, zawierającego scalony kodek ADC MSG alpha-0.4.1. Dotychczasowy kod, historia i gałęzie są zachowane. Domyślny język serwera pozostaje EN/US UTF-8; dokumentacja i pomoc operatora wspierają też PL.

## Zakres i architektura

`dc24h_chat` jest osobną biblioteką C++20: `include/dc24h/chat/core.hpp`, `src/chat/core.cpp`. Zależy od Users i wspólnej walidacji tekstu; nie zależy od ADC, gniazd, TLS, logowania, SQL, zegarów ani trwałych kolejek. ADC zależy od Chat, bez cyklu bibliotek. LoginHub posiada silnik obok rejestru sesji i dostarcza adapter wyjścia.

Pierwszy handler dostarcza publiczne BMSG wszystkim aktualnie ready, także nadawcy, oraz zachowuje opcjonalne `/me`. Jest to zgodne z [regułami broadcast i MSG ADC](https://adc.sourceforge.io/ADC.html). Rozmowy prywatne/grupowe, D/E/F/H, historia, moderacja, konta/uprawnienia i osobne limity szybkości czatu pozostają kolejnymi etapami. Działają dotychczasowe limity wejścia, ramek, bezpieczeństwa i wyjścia. Opcjonalnego pomijania AW2 nie zastosowano: publiczny czat otrzymują wszyscy ready.

## Przyjęcie i plany

`Core::prepare(sender, source, text, third_person)` zwraca samodzielny Broadcast lub typowany Error. Wymaga istniejącej sesji ready z publiczną tożsamością i dokładnym dopasowaniem źródła. Accepted, negotiating, identified/czekający na powitanie, closing, closed i brakujący nadawca nie mogą publikować. Kontrola źródła jest powtarzana przy dispatch; stary plan nie uprawnia do publikacji po zamknięciu/usunięciu sesji.

Tekst jest niepustym UTF-8/NFC. Odkodowany LF jest dozwolony; inne surowe ASCII controls i DEL są odrzucane. Tekst nie jest przycinany, normalizowany po cichu ani interpretowany jako polecenia. Neutralny limit wynosi 16384 bajty po odkodowaniu; ADC nadal ogranicza całą zakodowaną ramkę z nagłówkiem/separatorami/escaping do 16384 bajtów. `text::valid_chat_text` zachowuje politykę znaków kodeka ADC. LF i backslash są treścią, nie dodatkowymi ramkami.

Broadcast posiada ID życia nadawcy, klucz źródła, tekst, opcjonalny ME i uporządkowaną listę ID życia odbiorców. Nie kopiuje CID, nicka, profilu, adresu, sekretów ani nieznanych pól ADC. Odbiorcy pochodzą ze stron SessionManager po 256 ID, są filtrowani do ready i ograniczeni pojemnością rejestru. Publiczny interfejs planu jest niemutowalny. Dane przetrwają zmianę tekstów wywołującego/usunięcie modeli. Plan jest kopią chwili przyjęcia, nie historią ani zgodą na publikację po zamknięciu źródła.

## Dispatch i cykl życia

`Core::dispatch(plan, sink)` ponownie sprawdza nadawcę, a następnie rozwiązuje każde zapisane ID bezpośrednio przed wywołaniem sink. Usunięci/zamykani/niegotowi odbiorcy są pomijani; późniejsze połączenia nie są dodawane wstecz. Ponowne użycie SID protokołu nie przejmuje starego ID życia. Zmiana nicka zachowująca SID nie unieważnia aktywnego źródła.

Sink zwraca informację o przyjęciu jednej wiadomości do kolejki. DispatchReport rozróżnia queued, unavailable i refused. Queued nie oznacza dostarczenia ani przeczytania przez klienta. Odmowa jednego odbiorcy nie zatrzymuje pozostałych. Sam dispatch nie alokuje po skonstruowaniu sink. Wyjątek callback propaguje się po możliwej częściowej publikacji; adaptery nie mogą rzucać ani ponawiać częściowo wysłanego planu. Brak atomowości całej sieci, retransmisji, potwierdzeń, trwałych ID wiadomości i gwarancji dokładnie raz przy ponowieniach wywołującego.

Silnik działa wyłącznie w wątku reaktora. Modele rejestru są odczytywane const. Sink może zamykać odbiorców: żaden iterator kontenera ani pożyczony model nie jest przechowywany między callbackami. Gdy dispatch już rozpoczęto, zamknięcie nadawcy przez awarię jego kolejki nie anuluje bieżącej przyjętej publikacji dla pozostałych.

## Adapter ADC i prywatność

Po kontrolach kierunku/NORMAL/źródła LoginHub przekazuje MSG do Chat. Publiczna dostawa obsługuje wyłącznie BMSG bez PM. DMSG, EMSG, FMSG, HMSG i BMSG z prawidłowym PM otrzymują recoverable ISTA 125 z FOURCC. Nigdy nie są zamieniane na publiczne wiadomości. Treść nieobsługiwanych rozmów prywatnych nie trafia do innych użytkowników. Błędne PM/ME, podszycie źródła i błędny stan zachowują istniejącą ścieżkę fatal STA/własne QUI.

Adapter waliduje MSG kodekiem alpha-0.4.1, przygotowuje plan Chat i buduje nowe publiczne ADC wyłącznie ze źródła, tekstu i opcjonalnego ME. Nieznane pola, także podobne do PD, są usuwane. ME jest zapisywane jako ME0/ME1 z zachowaniem różnicy między brakiem a false; tekst użytkownika pozostaje niezmieniony. Projekcja/serializacja kończy się przed publikacją; każde enqueue nadal kontroluje zakodowany budżet klienta i całego huba.

Wiadomości trafiają wyłącznie do ready. Oczekujący na powitanie otrzymuje normalną listę, bez zaległego czatu. FIFO zachowuje INF przed późniejszym czatem w kolejności reaktora. Odbiorca closing nie dostaje zwykłego czatu po rozpoczęciu ścieżki terminalnej. Przepełnienie jego wyjścia wymusza output_limit bez zmiany przypiętego bufora; inni odbiorcy nadal otrzymują wiadomość. Awaria alokacji walidacji stanu MSG/przygotowania zamyka nadawcę z internal_error. Awaria kopii/kolejki pojedynczego odbiorcy zamyka tylko jego i nie przerywa rozgłaszania. Dotychczasowe sprzątanie zwalnia budżety, SID/CID/nick i transport. Nie deklaruje to pełnej odporności parsera/procesu na OOM.

Treść nie jest logowana, zapisywana w bazie, eksportowana w API ani wyświetlana przez WebAdmin. Działają istniejące statystyki liczbowe. Bez nowego JSON API, konfiguracji, SQL i zależności. Pomoc US/PL opisuje publiczny czat; metadane serwera/WebAdmin mają alpha-0.4.2.

## Weryfikacja

`chat.core` sprawdza niezależną od protokołu walidację źródła/tekstu, własność planu, 270 użytkowników i stronicowanie, fazy niegotowe, późniejsze połączenia/usunięcia, odmowę sink, ID życia, rename, stare źródło, wycofanie alokacji i dispatch bez alokacji z istniejącym sink.

`chat.adc` obejmuje projekcję/prywatność, powitanie i closing, odmowę BMSG z PM, limit wolnego odbiorcy, przypięte wyjście, zwalnianie budżetu oraz awarie alokacji walidacji/przygotowania/pojedynczych kolejek.

`chat.process` używa prawdziwych mieszanych ADC/ADCS: Unicode i /me w obu kierunkach, sanitizacja pól, INF/czat FIFO, odmowy prywatne/grupowe/feature/hub, odzyskiwanie składni, brak zaległego czatu dla niegotowych, reconnect, podszycie i prywatność logów/API. ncdc ADC/ADCS sprawdza przychodzący tekst na ekranie, dostarczenie do obserwatora, /me, kopię dla nadawcy, reconnect i odmowę prywatnej rozmowy. Zachowano roundtrip przechwyconych ramek plaintext.

Starsze testy kolejek/rozłączeń używające nieobsługiwanego BMSG jako wypełniacza używają teraz HMSG, zachowując cel. Oczekiwania braku czatu zastąpiono asercjami publicznego dostarczenia. Wymagane są pełne regresje serwera/WebAdmin. Obciążenie produkcyjne i wszystkie systemy/klienty wymagają osobnej weryfikacji.
