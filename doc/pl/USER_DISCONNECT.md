# User Disconnect — alpha-0.3.11

Etap powstał bezpośrednio z main `49b57749b346cb1638eae856c4b778a371616c47`, po scaleniu alpha-0.3.10 INF Updates. Gałąź alpha-0.3.11 zachowuje wcześniejszą historię i nie jest scalana automatycznie.

## Finalizacja i własność

`adc::UserDisconnect` pożycza SessionManager w pojedynczym wątku reaktora. Nie posiada gniazd, kontekstu TLS, kolejek, bazy, timerów ani drugiego rejestru użytkowników. Obie operacje nie alokują pamięci i mają noexcept:

1. `begin(id, cause, now)` znajduje model, kopiuje typowany publiczny SID i historyczną gotowość, a następnie wywołuje zarządzane finish_close. Zwracane Departure zawiera wartości, bez referencji do źródła. Połączenia accepted/negotiating nie mają publicznego odejścia. Użytkownik identified oczekujący na powitanie jest już publiczny i wymaga QUI, nawet jeśli nigdy nie osiągnął ready.
2. Właściciel odejmuje koszt kolejki, rozłącza ClientSession (zwalniając dzierżawę SID RAII i metadane) oraz niszczy Peer zawierający pożyczone referencje Session.
3. `release(id)` usuwa zamkniętą Session przez SessionManager, zwalniając rezerwacje SID/CID w UserRegistry i nicka w NickManager. Dopiero potem LoginHub ogłasza zapisany SID pozostałym publicznym odbiorcom.

Kolejność usuwa wcześniejszy problem: alokacja/rozsyłanie QUI przed sprzątaniem mogły rzucić wyjątek i pozostawić źródło. Zwraca też globalny budżet wyjściowy źródła przed obciążeniem odbiorców kosztem QUI. Rozłączony użytkownik nie jest celem routingu, a nowe połączenie może ponownie użyć CID/nicka i dostępnego SID bez odziedziczenia lokalnego SessionId poprzedniego życia.

begin zachowuje pierwszą przyczynę zamknięcia, pierwotny czas zamykania, historię gotowości i monotoniczne reguły czasu. Brak ID i cofnięty czas zwracają typowany RegistryError/SessionError bez mutacji. Kolejne begin dla closed zwraca first=false bez publicznego SID; kolejne release zwraca missing. Powtórzone LoginHub::disconnect dla nieobecnego Peer nic nie robi. Usunięcie żywego modelu zwraca not_closed. Błędny ogólny SID User daje invalid_public_sid, ale nie blokuje sprzątania; rzeczywiste przyjęcie ADC wymaga poprawnego, niezerowego SID.

Przed release wywołujący musi porzucić wszystkie pożyczone referencje Session. Moduł nie może sprawdzać zewnętrznych referencji ani automatycznie zabezpieczać dowolnych adapterów. W działającym serwerze Network nadal odpowiada za zdarzenie końcowe, usunięcie obserwacji epoll przed zamknięciem fd, zwolnienie limitów pending/per-IP z użyciem historycznej gotowości oraz zniszczenie połączenia gniazda/TLS/dekodera. Unikalny token transportu nie jest ponownie używany jako ID innej sesji. UserDisconnect nie przejmuje własności sieci.

## QUI, kolejki i obsługa błędów

[Specyfikacja ADC QUI](https://adc.sourceforge.io/ADC.html#_qui) definiuje IQUI z SID odchodzącego użytkownika. Publiczne powiadomienie ma dokładnie postać `IQUI <SID>\n`, bez przyczyny dostarczonej przez klienta, PID ani prywatnych metadanych. Odbiorcy identified/ready dostają je raz po wcześniej zakolejkowanych ramkach. Odbiorcy anonimowi, closing i natychmiast zamykani są pomijani. Nowa ramka nie może trafić za własne końcowe QUI odbiorcy. Własny INF kończący powitanie identified pozostaje przed późniejszymi zdarzeniami obecności.

Zachowano niezmienne bufory wyjścia i rozliczanie pełnych ramek. Sprzątanie źródła nie zmienia wskaźnika/widoku częściowo wysłanego albo przypiętego przez TLS początku innego odbiorcy. Nadal obowiązują limity kolejki klienta i huba: odbiorca bez miejsca na powiadomienie jest natychmiast zamykany z output_limit. Jego zasoby zwalnia zwykłe sprzątanie reaktora.

Po zwolnieniu źródła LoginHub przechwytuje bad_alloc przy tworzeniu publicznego QUI i dodawaniu go do kolejki. Błąd utworzenia powiadomienia natychmiast zamyka wszystkich pozostałych publicznych odbiorców z internal_error bez kolejnej alokacji. Błąd pojedynczej kolejki zamyka tego odbiorcę, a rozsyłanie do pozostałych trwa dalej. Klient nie może działać z używalną nieaktualną listą. Rezerwacje tych odbiorców pozostają do ich indywidualnego sprzątania. Końcowe ścieżki nie gwarantują dostarczenia do już uszkodzonego połączenia; nie obiecujemy pełnej odporności systemu na OOM ani atomowego dostarczenia wszystkim.

Dotychczasowy protokół końcowy pozostaje: ograniczone własne ISTA → własne IQUI → zamknięcie przy odpowiednich odrzuceniach, z zachowaniem częściowego początku kolejki i dotychczasowego terminu opróżnienia. Po zakończeniu źródła publiczne odejście podlega opisanej sekwencji. EOF, reset, błąd TLS, odrzucenie protokołu/statusu, timeout i limity zasobów używają wspólnego sprzątania. Wyłączenie serwera jawnie wyłącza publiczne ogłoszenia podczas zamykania wszystkich połączeń, unikając zbędnego rozsyłania QUI. Nie dodano wymiany TLS close_notify ani nieograniczonego oczekiwania na wyjście.

Nie dodano zależności, ustawień konfiguracji, kontraktu SQL/API, funkcji WebAdmin ani klucza tłumaczeń. Domyślny pozostaje EN/US UTF-8 z istniejącymi katalogami languages/us i languages/pl. Trwałe konta, czat, wyszukiwanie i transfery pozostają planem.

## Weryfikacja

`users.disconnect` obejmuje sześć faz cyklu życia, finalizację/usunięcie bez alokacji, pierwszą przyczynę/czas, cofnięty czas, brak/powtórzenie operacji, błędny ogólny SID, rezerwacje aż do release i rzeczywiste ponowne użycie jednoelementowej puli SID/CID/nicka. Testy LoginHub sprawdzają anulowanie powitania przed ready, anonimowe ADCS, pojedyncze bare QUI, FIFO, zwrot globalnego budżetu przed rozsyłaniem i niezmieniony przypięty początek odbiorcy.

Kontrolowana kampania alokacji C++ dowodzi zwolnienia źródła także przy błędzie powiadomienia. Błędy trwałe i jednorazowe obejmują utworzenie powiadomienia, izolację pojedynczego odbiorcy i dalsze dostarczanie do zdrowych klientów. Wstrzykujemy błędy alokacji C++ na badanych ścieżkach, bez awarii alokatora C, jądra i niezwiązanych operacji protokołu.

`users.disconnect.process` powtarza EOF, RST, fatalne HSTA klienta i końcowe odrzucenie błędnego INF przez ADC i ADCS, z obserwatorem na drugim transporcie. Każdy cykl sprawdza jedno bare publiczne QUI, reconnect z tym samym CID/nickiem, odzyskanie jedynego miejsca pending-login po błędnym anonimowym TLS, brak anonimowego odejścia, unikalne ID transportu i dokładnie jedno końcowe zdarzenie logu na przyjęte połączenie. Wyłączenie obejmuje gotowych użytkowników i niedokończone TLS bez publicznych danych aplikacji i prywatnej treści klienta w logach.

Rzeczywiste testy ncdc ADC/ADCS wymagają również statusu niedostępnego odbiorcy przy routingu do starego SID ncdc. W pełnej regresji pozostają wcześniejsze testy kolejności końcowej, timeoutów, limitów wyjścia, list, aktualizacji, bezpieczeństwa, TLS i prywatności. [TESTING.md](TESTING.md) opisuje wyniki lokalne i polecenia.
