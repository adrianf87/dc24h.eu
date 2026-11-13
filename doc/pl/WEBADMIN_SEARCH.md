# Monitoring wyszukiwań WebAdmin — alpha-0.5.13

**Bieżący etap — alpha-0.6.3:** [ADC RCM](ADC_RCM.md) dodaje własną walidację D/E żądań odwrotnych i routing celu/echa przez Transfer Core, z pasywnym źródłem/aktywnym celem dla rodziny, kontrolą życia/adresu i wspólnymi kolejkami. CTM/RCM współdzielą walidację pól. Rzeczywisty ncdc sprawdza RCM→CTM i handshake obu ról; bez rejestru tokenów oczekujących i obsługi danych plików.

**Etap dostarczony w alpha-0.6.2:** [ADC CTM](ADC_CTM.md) dodaje własną walidację D/E CTM i rzeczywisty routing celu/echa przez Transfer Core, z kontrolą aktywności TCP źródła, życia/adresu, wspólnymi ograniczonymi kolejkami i handshake rzeczywistego ncdc. Protokół/port/token/rozszerzenia zachowują sprawdzony zapis i prywatność. RCM i dane plików pozostają kolejnymi etapami.

**Etap dostarczony w alpha-0.6.1:** [Transfer Core](TRANSFER_CORE.md) dodaje niezależny silnik C++20 koordynacji połączeń: ograniczone własne żądania, role aktywne/pasywne IPv4/IPv6 i niezmienne plany jednej próby związane z sesjami, anulowaniem i ponownymi kontrolami. Wspólna klasyfikacja adresów Users zachowuje Search. Routing CTM/RCM i dane plików pozostają kolejnymi etapami.

**Lokalna walidacja alpha-0.5.13 (2026-11-13):** Przeszły build serwera Debug i wszystkie **120/120 zestawów CTest** z wymaganymi narzędziami, w tym rzeczywistym ncdc ADC/ADCS/UDP, MariaDB i Apache HTTPS. Przeszły build/typecheck WebAdmin, **12 testów Node**, **10 testów Chromium** oraz integracja API C++ z React potwierdzająca dodatnie tempo wyszukiwań. Sprawdzono widoki desktop i telefon PL. Poprawne są metadane, UTF-8, zgodność języków/linków, składnia zmienionych plików Python, przykładowy INI i białe znaki; brak ostrzeżeń kompilatora. Etap zmienia monitoring w panelu i metadane wydania, bez zmiany algorytmu wyszukiwania/API C++. Zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne nie zostały zweryfikowane.

Etap powstał bezpośrednio z main `dbd5f74dacdb409c073c99df805ff419975894d0` po scaleniu alpha-0.5.12. Zachowano kod, historię i katalogi US/PL. Gałąź funkcji jest publikowana oddzielnie; main pozostaje bez zmian.

## Widok operatora

Otwórz **Wyszukiwanie** w WebAdmin, aby zobaczyć ostatnią zbiorczą aktywność huba ponad dotychczasowymi licznikami i konfiguracją startową. Strona obsługuje EN/US i PL, desktop/telefon, ręczne odświeżanie oraz wspólny przełącznik odpytywania co pięć sekund. Korzysta z istniejącego uwierzytelnionego [API stanu](API.md), proxy i prywatnego sekretu. Nie dodano drugiego żądania odpytywania, endpointu, operacji zapisu, zależności ani migracji.

Cztery karty pokazują tempo odebranych zapytań, wyników, odrzuconych prób i kopii przyjętych do kolejek w ostatnim przyjętym przedziale. Wykres dwóch serii przedstawia tempo zapytań/wyników na wspólnej skali i z pozycjami wynikającymi z upływu czasu. **Ostatnie przedziały** otwiera dostępną tabelę sześciu ostatnich przedziałów, od najstarszego. Wartości mają wybrany format językowy i najwyżej jedno miejsce po przecinku. Wykres ma dostępną nazwę; tabela pozwala odczytać dane bez rozróżniania kolorów.

Poniżej pozostaje 19 liczników łącznych, wskaźnik okien tokenów i 16 ustawień tylko do odczytu opisanych w [konfiguracji wyszukiwania](SEARCH_CONFIGURATION.md). Okna wygasają leniwie, więc wskaźnik nie jest dokładną liczbą żywych wyszukiwań. Kopie w kolejce nie potwierdzają dostarczenia. URES bezpośrednio między klientami nie trafia do API ani monitoringu. Nie wyświetla się treści zapytań, nazw plików, TTH, tokenów, SID/CID, listy użytkowników ani historii wyszukiwań.

## Zasady próbkowania

`lib/search-monitoring.ts` przechowuje do **30 obserwacji**, czyli najwyżej 29 przedziałów. Każda ma własną dozwoloną kopię 19 liczników łącznych, wersję/czas pracy serwera i monotoniczny czas przeglądarki. Pomija wskaźnik okien tokenów, konfigurację i nieznane pola API. Wspólny hook `useHub` zachowuje tę ograniczoną historię przy nawigacji i zmianie języka. Nie zapisuje jej w local/session storage, bazie ani historii serwera.

Tempo jest szacunkiem: `(nowy licznik - poprzedni licznik) / czas w sekundach`. `performance.now()` przeglądarki mierzy odstęp między poprawnie odczytanymi odpowiedziami; czas pracy serwera służy jedynie do wykrywania zerowania. Są to średnie obserwowane w przedziale, nie chwilowe obciążenie, skonfigurowane limity ani tempo dostarczania przez sieć. Liczniki API są próbkowane niezależnie; nie wymaga się zgodności sum odebrań i wyników końcowych.

| Widok | Różnice liczników |
| --- | --- |
| Zapytania / s | queries_received |
| Wyniki / s | results_received |
| Odrzucone próby / s | queries_rejected + queries_rate_limited + results_rejected + results_rate_limited + results_recipient_limited |
| Kopie w kolejce / s | query_queued + result_queued |

Odrzucone próby obejmują walidację/stan/źródło/politykę i limity nadawcy/odbiorcy. Nie dodaje się do nich odrzuconych kopii kolejek, ignorowanych zapytań ani błędów wewnętrznych. Jedno zapytanie może zakolejkować wiele kopii. Szczegóły łączne pozostają dostępne do rozróżniania wyników.

Pierwsza migawka stanowi punkt początkowy: cztery karty pokazują kreski, a wykres oczekuje na dwie obserwacje. Równe liczniki w następnym przedziale poprawnie dają zero. Odczyty częstsze niż **sekunda** aktualizują zwykłą migawkę, ale nie dopisują obserwacji; ich przyrosty zostają później uwzględnione względem zachowanego punktu początkowego. Przyjmowane są odstępy do **15 sekund**. Dłuższa przerwa, cofnięcie czasu, zmiana wersji, spadek czasu pracy lub któregokolwiek z **19** liczników rozpoczyna nowe okno bez ujemnych wartości i linii łączącej zerowanie. Spadek wskaźnika okien tokenów nie zeruje historii tempa.

Błąd HTTP, timeout, błędna odpowiedź, niebezpieczna liczba lub brak rozszerzenia wyszukiwania czyści obserwacje. Starsze API bez `search` zachowuje stan niedostępnych danych. Powrót połączenia zaczyna nowe okno. Anulowane poprzednie żądanie nie może wyczyścić stanu nowszego. Brakujące przedziały nie są zastępowane zerami ani łączone ponad awarią.

Wyłączenie automatycznego odpytywania zachowuje ostatni przedział i jawnie pokazuje wstrzymanie. Odświeżanie ręczne nadal działa. **Wyczyść lokalne obserwacje** usuwa tylko historię tej przeglądarki, bez żądania zapisu i bez zmiany liczników/ustawień serwera. Następny odczyt ustala nowy punkt początkowy. Przeładowanie strony też usuwa obserwacje. Eksport pozostaje oczyszczoną migawką łączną API, bez czasów przeglądarki i wyliczonej historii.

Wykrywanie restartu używa dostępnych sygnałów wersji/czasu pracy/liczników. Restart, po którym wszystkie liczniki i czas pracy zdążyły dogonić poprzednie wartości, jest nierozróżnialny w tym kontrakcie. Nie dodano identyfikatora procesu/epoki ani trwałej usługi monitoringu.

## Weryfikacja

Testy jednostkowe obejmują dokładne różnice, zero przy bezczynności, spadek każdego licznika, zmiany wskaźnika, zerowanie czasu pracy/wersji, błędne zegary, granice jednej/piętnastu sekund, zachowanie po szybkim odczycie, własne dozwolone kopie, limit 30 obserwacji i granicę bezpiecznych liczb JavaScript. Testy przeglądarki sprawdzają wspólne odpytywanie, wykres/tabelę, pauzę/odświeżanie ręczne, lokalne czyszczenie, niezmieniony eksport, telefon PL, brak/błąd danych i powrót po restarcie/przerwie.

Integracja z rzeczywistym API C++ najpierw ustala punkt początkowy w przeglądarce, następnie wysyła dwie dodatkowe pary zapytanie/wynik ADC przez prawdziwe sockety huba i potwierdza dodatnie tempo oraz liczniki w React. Dotychczasowa zgodność ncdc ADC/ADCS/UDP pozostaje objęta testami serwera. Wyniki zapisano w [CHANGELOG](../../CHANGELOG.md); zdalne CI, pełna macierz systemów/klientów i obciążenie produkcyjne wymagają osobnej weryfikacji.
