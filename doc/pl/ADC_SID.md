# SID — alpha-0.2.5

## Zakres i wartości protokołu

`adc/sid.hpp` udostępnia Sid, SidPool i SidLease. `adc/sid_session.hpp` dodaje SidSession, łączący wspólną pulę huba z [SUP](ADC_SUP.md) i [ADC State](ADC_STATE.md). To niezależne komponenty C++20 w dc24h_adc, bez gniazd, logowania, bazy, Core i nowych zależności. Produkcyjny Network nadal używa samego parsera; etap nie uruchamia rzeczywistego handshake.

Źródło: [ADC 1.0.4](https://adc.sourceforge.io/ADC.html), rozdziały 3.2, 3.5.1, 5.1 i 5.3.3. SID jest wartością 20-bitową zapisaną czterema znakami base32. Koder używa alfabetu ABCDEFGHIJKLMNOPQRSTUVWXYZ234567, od najbardziej znaczącej grupy. Przykłady: 0 → AAAA, 1 → AAAB, 31 → AAA7, 32 → AABA, maksimum 1048575 → 7777. Parser i automat współdzielą koder bez zmiany akceptowanej składni SID.

`Sid::from_value(uint32_t)` przyjmuje 0..1048575; `Sid::parse(string_view)` wymaga dokładnie czterech znaków alfabetu, odrzucając małe litery, padding, spacje, kontrolne, NUL, Unicode i złą długość. Obie metody zwracają optional Sid bez alokacji. `value()` podaje liczbę, `encode()` cztery własne znaki bez terminatora, `str()` własny string. Kopia Sid jest publiczną wartością routingu, nie uchwytem własności, poświadczeniem ani potwierdzeniem tożsamości.

Pula rezerwuje zero/AAAA jako **politykę lokalną**, nie ograniczenie składni protokołu. Koder/parser nadal przyjmują AAAA. Przydział klientom obejmuje 1..capacity; domyślna pojemność wynosi 1048575. Mniejsza pojemność wybiera mniejszą przestrzeń dla polityki aplikacji/testów; brak nowej opcji INI i limitu Network.

## Algorytm puli i własność

Utworzyć dokładnie jedną wspólną SidPool na hub i używać jej we wszystkich przyszłych sesjach ADC/ADCS na wątku reaktora. Osobne instancje huba mają osobne przestrzenie i mogą wydawać taki sam SID. Konstruktor odrzuca zero/nadmiar względem 20 bitów przez invalid_argument ze stałym tekstem, następnie alokuje stan współdzielony i bitmapę. Przy pełnej pojemności bitmapa ma 16384 słowa uint64_t, dokładnie 128 KiB, plus stałe metadane/narzut kontenera. Aktywna rezerwacja zawiera tylko uchwyt stanu i opcjonalny Sid; bitmapa nie przechowuje osób ani parametrów.

`acquire() noexcept` zwraca aktywny SidLease albo pusty uchwyt po wyczerpaniu. Przydział rozpoczyna się przy obracanym kursorze, sprawdza wolne bity słowami, zawija raz i nie wybiera zajętego slotu ani nieużywanego bitu końcowego. Jest deterministyczny, bez losowych pętli prób. Pełna pula wykrywana jest w czasie stałym; przy wolnym miejscu przegląd obejmuje najwyżej słowa bitmapy i powtórną granicę podzielonego słowa. Po udanej konstrukcji acquire/reset nie alokują dynamicznie. Nie deklarujemy losowości, nieprzewidywalności ani benchmarku.

`size()`, `capacity()` i `in_use(Sid)` opisują rezerwacje na wątku właściciela. Zarezerwowany, jeszcze niewysłany SID jest zajęty. in_use zwraca false dla zera i wartości spoza zakresu tej puli; nie wyszukuje osoby. Pola nie są atomową telemetrią HTTP. Kopiowanie/przenoszenie puli jest wyłączone, aby przypadkowa kopia nie tworzyła pozornego drugiego właściciela. Udostępniać tę samą pulę jawnie przez shared_ptr.

SidLease jest domyślnie pusty, niekopiowalny i przenoszalny. Prywatny konstruktor uniemożliwia dowolne tworzenie rezerwacji, a `sid()` oddaje kopię opcjonalnej wartości. Move przenosi odpowiedzialność zwolnienia i opróżnia źródło. Przypisanie najpierw zwalnia cel; self-move jest bezpieczny. `reset()` i destruktor zwalniają raz; kolejne reset nic nie robią. Celowo brak API zwalniania przez sam SID: stara skopiowana wartość lub przeniesiony uchwyt nie może zwolnić nowego właściciela. Współdzielony stan bitmapy przeżywa zniszczenie obiektu puli, dopóki istnieje rezerwacja, unikając wiszącego wskaźnika przy sprzątaniu.

Wszystkie zmiany puli/sesji, reset/zniszczenie rezerwacji i zniszczenie ostatniego współwłaściciela muszą należeć do jednego wątku. Zarządzanie czasem życia przez shared_ptr nie czyni bitmapy bezpieczną współbieżnie. Przyszła obsługa wielu reaktorów wymaga jawnego projektu synchronizacji/własności.

## Zarządzana sesja SID

SidSession wymaga niepustego shared_ptr<SidPool> i opcjonalnej SupportPolicy. Utrzymuje życie alokatora, jest niekopiowalny/nieprzenoszalny i udostępnia tylko stałe funkcje protokołu. Nie daje modyfikowalnego automatu ani zwalnianego z zewnątrz uchwytu. Zniszczenie sesji zwalnia rezerwację, także gdy ISID przygotowano, ale nie wysłano.

| Krok | Skutek |
| --- | --- |
| Poprawny przychodzący HSUP | Negocjacja funkcji; PROTOCOL, brak SID |
| Obserwacja ukończonego wygenerowanego ISUP | Zakończenie odpowiedzi lokalnej; nadal PROTOCOL |
| `prepare_sid()` | Rezerwacja wartości; nadal PROTOCOL i pusty powiązany sid() |
| `sid_reply()` / `sid_reply_line()` | Własny Message ISID / linia LF dla rezerwacji; bez przejścia |
| Obserwacja ukończonego zgodnego ISID | Powiązanie SID i wejście do IDENTIFY |
| Decyzje aplikacji o tożsamości/haśle | Rezerwacja trwa przez IDENTIFY/VERIFY/NORMAL |
| IQUI własnego SID, fatalny STA huba/klienta, disconnect lub destrukcja | Zwolnienie rezerwacji dokładnie raz |

`prepare_sid() noexcept` zwraca opcjonalny SidError. Przed ukończonym ISUP lub z istniejącą rezerwacją daje invalid_sequence; poza PROTOCOL — invalid_state. Wyczerpanie daje exhausted bez zmian stanu/puli/sesji. Właściciel może zamknąć po ograniczonej odpowiedzi hub-full lub ponowić zgodnie z własną ograniczoną polityką. Zwolnienie przez inną sesję może udostępnić slot. Brak automatycznej kolejki oczekiwania i pętli ponowień.

`reserved_sid()` pokazuje opcjonalną rezerwację; `sid()` powiązany string protokołu, pusty do ukończenia ISID. Terminalny automat może zachować historyczny string po zwolnieniu, ale odrzuca wszystkie dalsze zdarzenia. Nowe połączenie wymaga nowej sesji. Odpowiedzi SID są kopiami; alokacja string/Message może rzucić bez utraty posiadanej rezerwacji. Pierwszy ISID ma dokładnie I/SID, jeden zarezerwowany parametr pozycyjny i brak routingu/funkcji/dodatkowych parametrów. Linia sieciowa ma dokładnie dziesięć bajtów z LF. Kopie odpowiedzi są dostępne podczas życia rezerwacji, lecz powtórne wysłanie po powiązaniu jest odrzucane.

`observe` zwraca opcjonalny `variant<StateError, SupportError, SidError>`. Błędny/niezarezerwowany ISID nie zmienia stanu ani nie zużywa kolejnego slotu. Podszyte źródła przychodzących B/D/E/F nadal odrzuca automat. IQUI innej osoby i przekazywany status nie zwalniają tej sesji. `disconnect()` jest terminalny i idempotentny. `sid_status_code()` proponuje fatalne 211 dla exhausted i 240 dla pozostałych błędów; nie wysyła bajtów i nie zawiera wejścia klienta. Tożsamość/hasło nadal wymagają jawnej zewnętrznej usługi; posiadanie SID ich nie zatwierdza.

## Wymagana dalsza integracja transportu

Zdarzenie to_client oznacza ukończoną uporządkowaną wysyłkę, nigdy dodanie do kolejki. Przyszły transport musi wysłać/zatwierdzić ISUP, przygotować SID, wysłać/zatwierdzić dokładnie ten ISID i utrzymać rezerwację podczas oczekiwania wyjścia. Rollback startu, częściowa/błędna wysyłka, EOF i terminy muszą rozłączać/niszczyć sesję. Przed zwolnieniem i ponownym użyciem anulować/opróżnić stare wyjście, usunąć referencje rejestru i określić kolejność listy osób/QUIT. Przenoszalny uchwyt zapobiega błędnemu zwolnieniu, ale nie zabezpiecza starych zakolejkowanych wiadomości. SID może być użyty ponownie i nie ma numeru generacji na drucie.

Rzeczywisty odbiorczy Network nie jest jeszcze podłączony do SidSession. Ograniczone wyjście i preflight, rejestr/dispatcher, obliczanie Tiger, walidacja PID/CID/nicku, terminy logowania i pełne logowanie ncdc są planowane. Brak zmian API HTTP, migracji SQL, TLS i katalogów komunikatów. SID jest publiczny i przewidywalny; nie używać go jako sekretu lub uprawnienia.

## Weryfikacja

`adc.sid` używa sprawdzeń aktywnych w Release dla wszystkich 1048576 wartości kodera i wszystkich 1048575 przydzielanych wartości puli. Sprawdza pełne wyczerpanie, 7777/maskę końca, zawijanie/użycie ponowne, małe pojemności przy granicach 64 bitów, deterministyczny churn, złe wejście, move/reset/self-move, życie puli/rezerwacji, osobne huby, unikalne wspólne sesje, wyczerpanie przed wysyłką, stare disconnect, zgodność ISID, podszywanie, trwałość VERIFY/NORMAL, własne/obce QUI, fatalny status i destrukcję przed wysłaniem.

`adc.sid.ncdc` używa izolowanego fixture ncdc z --sid. Python wysyła ISUP modułu C++, zgłasza ukończenie, odbiera/wysyła ISID wygenerowany z puli, zgłasza ukończenie i przekazuje rzeczywisty BINF ncdc z przydzielonym źródłem. Sterownik C++ pozostaje w niezatwierdzonym IDENTIFY i sprawdza sprzątanie po disconnect. Test obejmuje TCP loopback i TLS Pythona; sterownik SID nie ma stałego SID. Starszy fixture SUP zachowuje swój historyczny stały SID. Dotychczasowe adc.ncdc/adcs.ncdc osobno sprawdzają rzeczywisty Network/OpenSSL C++. To różne granice, nie pełne logowanie huba. Zobacz [TESTING.md](TESTING.md).
