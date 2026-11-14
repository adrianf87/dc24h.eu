# Transfer Limits — alpha-0.6.10

**Lokalna weryfikacja alpha-0.6.10 (2026-11-14):** Wszystkie 169/169 testów CTest przeszło w każdym z trybów Debug, Release i ASan/UBSan z wymaganymi narzędziami integracyjnymi, w tym nowe testy limitów native/process, dwa testy flood/odnowienie z prawdziwym ncdc, wcześniejsze połączenia peer plaintext/TLS, MariaDB i Apache HTTPS. Bez ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, 12 testów Node, 10 testów Chromium i integracja z rzeczywistym API C++. Poprawne metadane wersji, UTF-8/parytet języków/linki lokalne, składnia zmienionego Pythona, start/zatrzymanie z przykładem INI i białe znaki. Bez oczekiwania na zakończenie GitHub Actions; pełna macierz systemów i obciążenie produkcyjne pozostają niezweryfikowane.

Gałąź oparta bezpośrednio na main `76f3710905367d76ddd6791643446d14f152e6ec`. Zachowano wcześniejsze gałęzie alpha-0.6.1–alpha-0.6.9 oraz ich scalony kod. Ten etap ogranicza żądania zestawienia połączeń przekazywane przez hub; dane plików nadal płyną bezpośrednio pomiędzy klientami.

## Budżety i konfiguracja

`transfers/limits.hpp` definiuje walidowane `Limits`, `FloodLimits`, `Direction` i `TransferGuard`. Każde połączenie LoginHub posiada osobny strażnik z niezależnymi budżetami liczby i bajtów żądań wychodzących oraz przychodzących. Strażnik zajmuje stałą pamięć i nie przechowuje tokenów, SID, adresów, treści ani historii żądań. Przyjęcie i ograniczanie powiadomień nie alokują pamięci i używają przekazanego czasu zegara monotonicznego.

Wszystkie klucze należą do `[transfers]`:

| Klucz INI | Domyślnie | Dozwolony zakres |
| --- | ---: | --- |
| `request_rate` | 32 | 1..1000000 żądań/sekundę |
| `request_burst` | 64 | 1..1000000 żądań |
| `request_byte_rate` | 131072 | 1..16777216 zdekodowanych bajtów/sekundę |
| `request_byte_burst` | 262144 | 16384..16777216 zdekodowanych bajtów |
| `incoming_rate` | 128 | 1..1000000 żądań/sekundę |
| `incoming_burst` | 256 | 1..1000000 żądań |
| `incoming_byte_rate` | 524288 | 1..16777216 zdekodowanych bajtów/sekundę |
| `incoming_byte_burst` | 1048576 | 16384..16777216 zdekodowanych bajtów |

Brak klucza oznacza wartość domyślną. Zero, znak liczby, ułamki, błędne wartości, przepełnienia, duplikaty i wartości poza zakresem zatrzymują start. Opcje Network/LoginHub oraz bezpośredni konstruktor strażnika sprawdzają te same granice. Zmiana wymaga restartu. Uprawnienia nie omijają limitów; nie ma przełącznika wyłączającego ochronę.

Budżety początkowo są pełne. Ułamkowe odnowienie z dokładnością nanosekund nasyca się przy pojemności; bezczynność nie pozwala zgromadzić nieograniczonej serii. Cofnięty czas nie tworzy kredytu, a skrajne czasy min/max nie przepełniają obliczeń. Liczba i bajty są pobierane atomowo w ramach danego budżetu: odmowa nie zużywa żadnego z nich. Koszt obejmuje zdekodowany czteroznakowy nagłówek, źródło/cel, wszystkie parametry i separatory, bez końcowego LF. Bajty kodowania transportowego nadal podlegają dotychczasowym limitom ramek i ruchu. Minimalny byte_burst 16384 nie zwiększa istniejącego limitu pól CTM/RCM.

## Kolejność przyjmowania i dostarczanie

1. Najpierw wykonywane są dotychczasowe kontrole sesji, stanu i źródła. Wyczerpanie budżetu nie ukrywa terminalnej odmowy za podszycie się pod SID lub nieprawidłowy stan.
2. Próba CTM/RCM zużywa budżet wychodzący nadawcy przed przygotowaniem planu przez Connection Router, kontrolami możliwości/uprawnień i odpowiedzią o błędzie routingu. Nieistniejące cele, niezgodne role i odmowy polityki nie generują nieograniczonych odpowiedzi. Późniejsza odmowa lub błąd pamięci/kolejki nie zwraca pobranego kredytu.
3. Tylko poprawnie przygotowane żądanie może zużyć budżet przychodzący konkretnego czasu życia odbiorcy. Wszyscy nadawcy współdzielą ten budżet, co chroni odbiorcę przed sumą niezależnych limitów źródłowych. Nie powstaje stan dla nieistniejących SID. Odmowa anuluje plan przed dostarczeniem i echem E.
4. Obowiązują dotychczasowe jednorazowe wysyłanie, ponowne kontrole gotowości/adresu/uprawnień i ograniczone współdzielone kolejki. Przyjęte E kosztuje jedną próbę nadawcy i jedną odbiorcy; echo nie zużywa kolejnej próby przychodzącej. Przyjęcie do kolejki nie potwierdza zestawienia połączenia klientów.

CTM/RCM, zakres D/E, IPv4/IPv6, nazwa protokołu, token i zmiana odbiorcy współdzielą budżet wychodzący połączenia. Kierunki są niezależne, więc odbieranie żądań nie zużywa możliwości odpowiedzi CTM. INF/SUP i zaufane zmiany uprawnień zachowują kredyt. Rozłączenie niszczy strażnika; nowe połączenie otrzymuje świeże budżety. Dotychczasowe limity akceptacji i połączeń na IP oraz ogólne limity transportu nadal ograniczają częste ponowne łączenie.

Przekroczenie budżetu źródła lub celu pomija żądanie bez dostarczenia i echa. Nadawca dostaje najwyżej jedno odzyskiwalne `ISTA 100 Connection\srequest\srate\slimit\sexceeded FC<FOURCC>` na sekundę, wspólnie dla wszystkich odmów limitów transferowych. Kolejne odmowy są ciche; powodzenie nie resetuje okna powiadomień. Stały opis nie ujawnia tokenu, protokołu, adresu, SID ani budżetu odbiorcy. Peer STA nadal może raportować błędy połączeń i podlega istniejącym limitom transportu/wyjścia. [Specyfikacja ADC](https://adc.sourceforge.io/ADC.html), sekcje 3.3 i 5.3.1, definiuje D/E oraz odzyskiwalne ogólne STA; wartości limitów i odstęp powiadomień są lokalną polityką.

Błąd alokacji powiadomienia zamyka tylko źródło zgodnie z istniejącą granicą obsługi błędów. Błąd alokacji kolejki celu pozostaje lokalny dla celu; semantyka echa przyjętego E nie zmienia się. Odrzucone żądania nie oczekują na późniejsze ponowienie. Wcześniej zakolejkowane ramki nie są wycofywane ani ponownie rozliczane przy częściowym zapisie.

## Weryfikacja i zakres

`transfers.limits` sprawdza atomowość liczby/bajtów, ułamkowe odnowienie, skrajny/cofnięty czas, błędne limity/kierunek, brak alokacji strażnika, wspólne budżety komend/zakresów/protokołów, zmiany trybu/uprawnień, sumowanie na odbiorcy, niezależność kierunków, fałszywe źródło przy wyczerpaniu i sprzątanie po rozłączeniu.

`transfers.limits.process` obejmuje fragmentowane połączenia ADC/ADCS przez IPv4 i IPv6, prywatną ograniczoną odmowę, odnowienie, sumowanie na celu, koszt bajtów rozszerzeń, błędne/zduplikowane klucze INI oraz prywatność API/logów. `adc.transferlimits.ncdc` i `adcs.transferlimits.ncdc` wysyłają serię ofert nieobsługiwanego protokołu do prawdziwego ncdc: dokładnie jedna przyjęta oferta daje odpowiedź peer 141, pozostałe skutkują jednym prywatnym powiadomieniem huba, odnowienie pozwala na kolejną odpowiedź, a klienci zachowują czat i poprawne zakończenie. Wcześniejsze pozytywne testy CTM/RCM z połączeniem peer plaintext/TLS pozostają w pełnej regresji.

Etap nie dodaje zależności, migracji, polityki kont, rejestru oczekujących tokenów, endpointu HTTP, statystyk transferowych ani kontrolek WebAdmin. API zachowuje poprzedni schemat; nowe limity ustawia się przez INI. Zaufani użytkownicy samodzielnych Core/ConnectionRouter zapewniają własne zasady przyjmowania; limity transportowe stosuje LoginHub. Przepustowość danych peer, liczba równoległych transferów, sloty, terminy, trwałe kary oraz pełna macierz systemów/testy obciążenia pozostają dalszymi etapami.

Zobacz [Konfigurację](CONFIGURATION.md), [Connection Security](CONNECTION_SECURITY.md), [Error Handling](ERROR_HANDLING.md), [Transfer Core](TRANSFER_CORE.md), [Testowanie](TESTING.md) i [przykład INI](../../config/dc24h.ini.example).
