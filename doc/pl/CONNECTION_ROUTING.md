# Connection Routing — alpha-0.6.4

**Lokalna walidacja alpha-0.6.4 (2026-11-13):** Wszystkie **134/134 testy CTest** przeszły osobno w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracji i domyślnym wykrywaniem wycieków, w tym wspólna macierz SID/błędy/czasy życia, równoległe pary mieszanych ADC/ADCS IPv4/IPv6 z tym samym tokenem, rzeczywisty ncdc CTM/RCM i handshake klientów, wcześniejsze Search/UDP, MariaDB i Apache HTTPS. Brak ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** i integracja API C++. Poprawne są metadane, UTF-8, zgodność języków/linków, składnia zmienionych plików Python, przykładowy INI i białe znaki. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne pozostają niesprawdzone.

Etap rozpoczyna się z main `79b7c1ffa82d6a5389ddc92bd13e3ffa61eb95a1` po alpha-0.6.3. Łączy istniejące implementacje routingu CTM i RCM. Gałąź funkcji zachowuje historię i jest publikowana oddzielnie od main.

## Przyjmowanie i odbiorcy

`adc::ConnectionRouter` jest wspólnym silnikiem sprawdzonych DCTM, ECTM, DRCM i ERCM na wątku właściciela. LoginHub posiada jeden router korzystający z Transfer Core i SessionManager. Używa jednego handlera komend i jednego wariantu współdzielonego pakietu połączenia w ograniczonej kolejce wyjściowej. Silnik odrzuca inne komendy ADC; routing czatu, wyszukiwań i statusu pozostaje oddzielny.

SID źródła musi należeć do gotowej sesji wysyłającej. Dokładny SID celu jest rozwiązywany w rejestrze na okres życia gotowej sesji. Wskazanie siebie jest błędem. Cel nieobecny, niegotowy lub zamykany nie może wybrać zastępcy. CID, nick, deskryptor gniazda i token nie wybierają odbiorców. Identyczne tokeny równoległych niezależnych par nie łączą ich odbiorców ani nie tworzą rejestru oczekujących połączeń.

| Zakres | Ustaleni odbiorcy w kolejności | Dostarczanie |
| --- | --- | --- |
| DCTM / DRCM | Okres życia celu | Tylko cel |
| ECTM / ERCM | Okres życia celu, okres życia źródła | Próba celu, następnie niezależnie sprawdzone echo źródła |

`RoutedConnection::recipients()` udostępnia ograniczony zbiór jako stały span identyfikatorów okresów życia. Dispatch może anulować nieaktualnego odbiorcę, ale nie rozszerzyć zbioru, rozgłosić żądania, wybrać innego użytkownika ani przypisać ponownie użytego SID do nowego logowania. Błąd przygotowania lub nieaktualny cel podczas dispatch nie daje echa. Odmowa kolejki celu nadal może pozwolić na niezależne echo źródła. Przyjęcie do kolejki potwierdza wyjście koordynacji, a nie odbiór przez klienta, zestawienie połączenia lub dostarczenie pliku.

## Walidacja i role

Istniejące własne kodeki CTM i RCM pozostają granicą walidacji protokołu. Zachowano układy D/E, SID źródła/celu, widoczne nazwy protokołu, port CTM, nieinterpretowany token NFC, rozszerzenia, budżety odkodowanych danych i limity ramek. Poprawny nieznany protokół pozostaje do negocjacji klientów. Hub nie zmienia tokenów ani nie obniża ADCS. RCM nie otrzymuje wymyślonego portu ani odpowiedzi CTM generowanej przez hub.

CTM sprawdza źródło aktywne TCP z użytecznym adresem, wybierając IPv4 przed IPv6, a następnie przygotowuje cel. RCM najpierw rozwiązuje inny gotowy cel, po czym wybiera IPv4 przed IPv6, dla którego źródło jest pasywne, a cel aktywny TCP. Własność publicznego adresu i dokładne TCP4/TCP6 nadal wynikają ze wspólnych reguł Users/Transfer Core; aktywność UDP nie daje aktywności TCP. Rodzina jest wewnętrzną metadaną bez nowego pola ADC. Zachowano pierwszeństwo fatalnych błędów źródła/stanu i dotychczasowe odwracalne komunikaty 100/140 właściwe dla komendy.

[Specyfikacja ADC](https://adc.sourceforge.io/ADC.html) definiuje nagłówki direct/echo i negocjację CTM/RCM. Szczegóły implementacji i limity tego dokumentu opisują podzbiór obsługiwany w repozytorium.

## Czas życia, callbacki i kolejki

Plan zachowuje domenę routera i Core, niezmienne żądanie, wybrany endpoint oraz oryginalne identyfikatory życia źródła/celu. Kopie współdzielą jedną próbę Core. Anulowanie, ponowienie, rekurencyjny dispatch, obcy router i odtworzenie routera pod tym samym adresem pamięci nie dają kolejnej próby. Pusty sink i obcy plan są odrzucane przed zużyciem poprawnego planu. Dispatch ponownie sprawdza gotowość, własność SID, łączność i zapisany adres nasłuchu.

Jeden niezmienny `ConnectionFrame` posiada sprawdzoną wiadomość i zapisane bajty protokołu. Kolejki celu i opcjonalnego źródła współdzielą dokładnie ten pakiet, zachowując osobne offsety FIFO i logiczne naliczanie bajtów. Obowiązują istniejące przypięcie częściowego wyjścia TCP/TLS i limity kolejek. Dispatch zachowuje pakiet i metadane echa przed callbackiem celu; nawet zastąpienie planu wywołującego inną komendą wewnątrz callbacka nie przekieruje oryginalnego echa. Zamknięte lub zastąpione źródło anuluje echo; anuluje je również zmiana endpointu CTM lub aktywacja źródła RCM. Wyjątek callbacka zużywa próbę i propaguje bez ponowienia. Dispatch/cancel nie alokują po utworzeniu sinka.

Błąd alokacji przygotowania izoluje źródło. Błąd alokacji przy dodawaniu do kolejki lub przekroczenie limitu wyjścia izoluje danego odbiorcę; obowiązują istniejące zasady zamykania/opróżniania. Przygotowanie routingu nie wymaga zmiany rejestru, a błąd alokacji nie publikuje częściowego zbioru odbiorców.

## Przejście publicznego API C++

`CtmRouter` i `RcmRouter` pozostają fasadami ograniczonymi do właściwej komendy z metodami prepare/dispatch/cancel i oddzielnymi domenami. Delegują do ConnectionRouter. Dotychczasowe nazwy pakietów, planów, raportów i błędów Ctm/Rcm są aliasami wspólnych typów połączeń. Zwykłe wywołania pozostają dostępne, a wcześniejsze testy natywne kompilują się bez zmian.

Tożsamości dawnych typów zostały połączone: zewnętrzne przeciążenia lub warianty zawierające oba wcześniejsze typy pakietów/planów/błędów mogą wymagać pojedynczego wspólnego typu. Należy przebudować zależny kod C++; stabilne binarne ABI nie jest obiecane. Kod obsługujący obie komendy powinien używać bezpośrednio ConnectionRouter.

## Zakres i weryfikacja

Etap nie dodaje klucza konfiguracji, zależności, migracji bazy, endpointu, strony WebAdmin ani licznika transferów. Treść, tokeny i historia połączeń użytkowników pozostają poza logami, SQL i API/WebAdmin. Liczniki Chat/Search nie naliczają żądań połączeń; nadal obowiązują limity bajtów/ramek/wyjścia transportu. Rejestracja/korelacja/terminy tokenów oczekujących, nowa polityka bezpieczeństwa transferów, gniazda klientów i dane plików pozostają poza etapem.

`transfers.routing` sprawdza każdą parę różnych źródeł/celów w macierzy czterech SID dla obu komend i zakresów, dokładnych odbiorców, tożsamość pakietu, dozwolone komendy, kolejność błędów, odtworzenie domeny, kopie/anulowanie/ponowienie planów, rzeczywiste wymuszone ponowne użycie SID i zastąpienie planu między CTM/RCM w callbacku. Wstrzykiwanie błędów sprawdza wycofanie przygotowania i dispatch bez alokacji, także dla maksymalnych protokołów/tokenów, odmów i wyjątków. Wcześniejsze testy ról, czasów życia i kolejek CTM/RCM pozostają włączone.

`transfers.routing.process` używa IPv4 i IPv6 oraz mieszanych klientów ADC/ADCS: niezależne pary CTM i RCM celowo współdzielą token, odpowiedzi klientów trafiają tylko do celu, osoby postronne nic nie otrzymują, stare SID zawodzą po ponownym logowaniu, podszycie źródła zamyka sesję prywatnie, a czat działa dalej. Asercje API/logów sprawdzają prywatność i niezmienione liczniki Chat/Search. Istniejące testy rzeczywistego ncdc ADC/ADCS CTM i RCM nadal wykonują bezpośredni handshake CSUP/CINF/CGET obu ról. Zobacz [Kompilacja](BUILD.md), [Testy](TESTING.md), [Transfer Core](TRANSFER_CORE.md), [ADC CTM](ADC_CTM.md) i [ADC RCM](ADC_RCM.md).
