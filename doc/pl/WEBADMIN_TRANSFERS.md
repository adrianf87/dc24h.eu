# WebAdmin — monitoring transferów — alpha-0.6.12

**Lokalna weryfikacja alpha-0.6.12 (2026-11-14, Debian 13):** Przeszły wszystkie 173 zestawy CTest w Debug (z ponownym sprawdzeniem metadanych/wersji) oraz pełny przebieg 173/173 w Release, z wymaganymi integracjami i prawdziwym ncdc. Obie kompilacje C++ bez ostrzeżeń. Poprawne build/typecheck WebAdmin, 20 testów Node, 13 testów Chromium i rzeczywista integracja tempa CTM/RCM C++ API → React. Obejrzano zrzuty desktop/mobilny PL; poprawne metadane, UTF-8/parytet języków/linki lokalne, składnia zmienionego Pythona i białe znaki. Pozostałe systemy, obciążenie produkcyjne i nowy zdalny przebieg CI pozostają niezweryfikowane.

Etap oparty bezpośrednio na main `d24df98b9903ff259d01c87f5e1412bd0a87f6d5`, po poprawce zakresu bind ncdc w alpha-0.6.11. Implementacje i gałęzie alpha-0.6.1–alpha-0.6.11 już istnieją; ten etap dodaje brakujący monitoring, zachowując wcześniejsze prace.

## Widok operatora

Otwórz **Połączenia**, aby zobaczyć ostatnią aktywność koordynacji ponad łącznymi [statystykami transferów](TRANSFER_STATISTICS.md). Cztery karty pokazują odebrane żądania CTM/s, RCM/s, odrzucone próby/s i kopie przyjęte do kolejek/s. Wykres CTM/RCM używa wspólnej skali i rzeczywistych chwil obserwacji. **Ostatnie przedziały** otwierają dostępną tabelę sześciu ostatnich przedziałów, od najstarszego; liczby stosują format EN/US lub polski, z najwyżej jednym miejscem po przecinku.

Nadal widoczne są 22 liczniki łączne: osobne wyniki CTM/RCM, kopie/odmowy/anulowania/błędy alokacji kolejek oraz raporty STA 41/42 klientów. Przyjęcie do kolejki nie potwierdza wysłania przez gniazdo, nawiązania połączenia klientów, postępu pliku ani ukończenia transferu. Raporty błędów są niezweryfikowanymi deklaracjami klientów. Panel nie wyznacza przepustowości, liczby przesyłanych plików ani stanu transferów poszczególnych użytkowników.

Wspólny przełącznik odpytywania co pięć sekund i ręczne odświeżanie obsługują wszystkie strony jednym żądaniem statusu. Wstrzymanie zachowuje ostatni przedział z odpowiednią etykietą. **Wyczyść lokalne obserwacje** usuwa tylko obserwacje transferów w tej przeglądarce; bez zapisu API, zmiany liczników serwera i usuwania obserwacji wyszukiwania. Nawigacja i zmiana języka zachowują historię, przeładowanie strony ją usuwa. Starsze API bez statystyk transferów wyświetla niedostępne wartości i komunikat zamiast wymyślonych zer.

## Zasady pomiaru

`lib/transfer-monitoring.ts` przechowuje najwyżej **30 obserwacji** (29 przedziałów): własne kopie 22 dozwolonych liczników łącznych, wersję/czas działania serwera i czas monotoniczny przeglądarki. Ograniczonym stanem zarządza istniejący `useHub`. Nie ma local/session storage, bazy, trwałej historii, dodatkowego endpointu ani drugiego zegara odpytywania.

Tempo jest średnią w obserwowanym przedziale: różnica każdego licznika zostaje podzielona przez liczbę sekund `performance.now()`, a następnie sumowane są powiązane wartości. Odejmowanie przed sumowaniem zapobiega zaokrągleniu dużych sum liczników przy granicy dokładnych liczb całkowitych JavaScript. Adapter odrzuca wartości ujemne, ułamkowe, brakujące i niedokładne w obecnym rozszerzeniu statystyk. Niezależny odczyt liczników API nie wymaga chwilowej zgodności sum wyników.

| Karta | Różnice liczników |
| --- | --- |
| Żądania CTM / s | `ctm_received` |
| Żądania RCM / s | `rcm_received` |
| Odrzucone próby / s | `rejected`, `rate_limited`, `recipient_limited` dla CTM i RCM |
| Kopie w kolejce / s | `ctm_queued` + `rcm_queued` |

Tempo odmów obejmuje walidację protokołu/stanu/źródła/możliwości/uprawnień i limity źródła/odbiorcy. Błędy wewnętrzne, odmowy kolejek, anulowane kopie i raporty klientów pozostają odrębnymi szczegółami łącznymi. Żądanie z echem może umieścić dwie kopie w kolejkach; liczba kopii nie oznacza liczby unikalnych prób ani połączeń.

Pierwsza poprawna odpowiedź ustala punkt odniesienia; karty pokazują pauzy do drugiej obserwacji co najmniej **sekundę** później. Równe liczniki oznaczają wtedy zerową aktywność. Szybsze ręczne odświeżenia aktualizują zwykłą migawkę, zachowując punkt odniesienia. Akceptowane są przedziały do **15 sekund**; dłuższa przerwa zaczyna nowy pomiar. Cofnięcie zegara przeglądarki, zmiana wersji, spadek czasu działania lub **dowolnego** z 22 liczników również zeruje okno. Wykres i tempo nie łączą danych przez wykryty restart.

Błędy HTTP, timeouty, niepoprawne odpowiedzi/liczby oraz brak rozszerzenia transferów usuwają obserwacje. Powrót rozpoczyna się nowym punktem odniesienia. Zastąpione anulowane żądanie nie nadpisuje nowszych obserwacji. Braków nie wypełnia się zerami. Restart wykrywany jest przez obecną wersję/czas działania/liczniki: jeżeli wszystkie wartości zdążyły dogonić poprzednie, odróżnienie restartu wymagałoby przyszłego identyfikatora uruchomienia serwera.

## Prywatność i uruchamianie

Wykorzystywane są istniejące zabezpieczone [API statusu](API.md), prywatne gniazdo Unix i poświadczenia po stronie serwera WWW. Monitoring nie zawiera SID, CID, nicków, adresów/portów, nazw protokołów/tokenów, nazw plików ani treści żądań. Eksport migawki zachowuje stały format dozwolonych liczników łącznych; bez czasów przeglądarki i wyliczonych przedziałów. Etap nie dodaje zapisu API, migracji SQL, zależności, klucza INI ani aktywacji usługi produkcyjnej. Obowiązują istniejące zasady [WebAdmina](WEBADMIN.md) i [Apache HTTPS](APACHE2_HTTPS.md).

## Weryfikacja

`transfer-monitoring.test.ts` sprawdza dokładne różnice CTM/RCM/odmów/kopii, zerową aktywność, spadki wszystkich 22 liczników, restart wersji/czasu/zegara, brak rozszerzenia, granice jednej/piętnastu sekund, szybkie odświeżenia, własność dozwolonych danych, limit historii i dokładność dużych liczb. Testy przeglądarki obejmują odpytywanie, wykres/tabelę, zachowanie historii przy nawigacji, pauzę/ręczne odświeżanie, czyszczenie lokalne, eksport łączny, polski widok mobilny oraz błędne/brakujące dane i powrót po przerwie.

Integracja rzeczywisty C++ → prywatne API → adapter Vite → Chromium ustala punkt odniesienia przed wysłaniem kolejnych CTM i RCM przez prawdziwe gniazda huba. Sprawdza dodatnie tempo i liczniki w React, zachowany monitoring wyszukiwania, izolację poświadczeń/treści i układ mobilny. Istniejące testy CTest używają prawdziwego **ncdc** dla ADC/ADCS, CTM/RCM, negocjacji TLS klientów, trybów, tokenów, uprawnień, błędów i limitów. Końcowe wyniki i ograniczenia platform zapisano w [historii zmian](../../CHANGELOG.md) i [testowaniu](TESTING.md).
