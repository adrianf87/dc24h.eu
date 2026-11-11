# Statystyki czatu WebAdmin — alpha-0.4.12

Gałąź utworzono bezpośrednio z main `be34e21e3ebad17b3f75cd5277a3dd6023c043d2` po Chat Logging. Etap korzysta z istniejących pól `/api/v1/status.chat`. Działanie serwera, definicje liczników, protokół i schemat API zachowują zakres opisany w [Chat Logging](CHAT_LOGGING.md).

## Widok operatora

Otwórz **Czat** w nawigacji monitoringu, wybierz **Zobacz wszystkie statystyki czatu** na Przeglądzie lub przejdź bezpośrednio do `#chat`. Odświeżenie strony oraz wstecz/dalej zachowują nawigację fragmentem URL. Wspólny wybór języka przełącza etykiety/opisy US i PL; liczby korzystają z `en-US` lub `pl-PL`. Menu mobilne zamyka się po nawigacji i pozwala przewijać zawartość na niskich ekranach.

Widok pokazuje dziewięć wartości bezpośrednio z walidowanej migawki API:

| Sekcja | Etykieta | Pole API | Znaczenie |
| --- | --- | --- | --- |
| Próby wysłania wiadomości | Odebrane próby | `received` | Kwalifikujące się odkodowane próby publicznego/bezpośredniego/zwrotnego MSG klienta przyjęte do obsługi czatu |
| Próby wysłania wiadomości | Czat publiczny | `public_dispatches` | Publiczne wywołania dispatch zakończone raportem, w tym akcje |
| Próby wysłania wiadomości | Czat prywatny | `private_dispatches` | Bezpośrednie/zwrotne wywołania dispatch zakończone raportem, także do siebie |
| Próby wysłania wiadomości | Odrzucony czat | `rejected` | Błędy walidacji/polityki lub brak celu przed poprawnym dispatch |
| Próby wysłania wiadomości | Limity tempa czatu | `flood_limited` | Odmowy Anti-Flood, także bez powiadomienia |
| Próby wysłania wiadomości | Błędy wewnętrzne | `errors` | Próby przerwane przez wewnętrzne błędy przygotowania/dispatch |
| Kolejki odbiorców | Kopie w kolejce | `queued` | Przyjęcia do kolejek wyjściowych odbiorców |
| Kolejki odbiorców | Niedostępni odbiorcy | `unavailable` | Odbiorcy z raportu dispatch, którzy stali się niedostępni przed przyjęciem kopii |
| Kolejki odbiorców | Odrzucone kopie | `refused` | Odmowy kolejek odbiorców, w tym limity pojemności/błędy alokacji |

Opis zakresu wyjaśnia liczenie od startu, zerowanie po restarcie i pomijanie powitań/wiadomości systemowych. Pomijane są też surowe błędy parsera i odmowy transportu/bezpieczeństwa przed obsługą czatu; dokładne granice opisuje [Chat Logging](CHAT_LOGGING.md). Jedna wiadomość może tworzyć wiele kopii dla odbiorców. Przyjęcie do kolejki nie oznacza dostarczenia ani odczytu. Dispatch może nie mieć żadnego przyjętego odbiorcy. Widok pokazuje sumy bez sztucznych wskaźników dostarczenia, tempa/sekundę, procentów i transkrypcji.

Przegląd zachowuje cztery liczniki podsumowania i prowadzi do pełnego widoku. Wspólny komponent Metric utrzymuje dotychczasowy wygląd. Osobna strona rozdziela sześć liczników prób od trzech liczników odbiorców, dopasowuje siatkę do komputerów/tabletów/telefonów i zawija duże wartości bez przepełnienia strony. Rzeczywiste zero jest pokazywane jako `0`; brak danych jako `—`.

## Odświeżanie, błędy i eksport

Wspólne odświeżanie ręczne i automatyczne co pięć sekund obejmuje Czat. Wyłączenie automatycznego pozostawia przycisk ręczny. Każda poprawna migawka zastępuje sumy, także po spadku/wyzerowaniu po restarcie. Nawigacja i zmiana języka nie zerują liczników serwera. Nie jest liczona lokalna historia różnic ani tempo czatu.

Dotychczasowy ograniczony czytnik API i ścisły `parseSnapshot` sprawdzają dziewięć liczników. Brakujące pola, wartości nieliczbowe/ujemne/ułamkowe/poza bezpiecznym zakresem całkowitym JavaScript oraz nieudane żądanie API odrzucają całą migawkę. Dotychczasowy baner pokazuje łączenie/brak konfiguracji/utratę połączenia; wcześniejsze wartości zastępowane są kreskami, a eksport jest wyłączany do otrzymania poprawnych danych. Poprawne same zera zachowują stan połączenia i możliwość eksportu. Pola atomowe API są próbkowane niezależnie, więc UI nie wymusza zgodności sum w jednej odpowiedzi HTTP. Liczby większe od bezpiecznego zakresu JavaScript są odrzucane zamiast zaokrąglania.

**Eksportuj migawkę** korzysta z bezpiecznego eksportu: znacznik czasu i pełny znormalizowany stan, w tym dziewięć nazwanych liczników `chat`. Nie jest to eksport rozmów. Parser wybiera wyłącznie dozwolone pola i usuwa treść/odbiorców/tożsamości przed wyświetleniem lub pobraniem. Gniazdo/token pozostają po stronie serwerowej adaptera Vite; token i dowolny tekst backendu nie trafiają do przeglądarki. Widok nie dodaje komend czatu, zmiany konfiguracji, historii użytkowników, endpointów ani pól poświadczeń. Wymagania produkcyjnej bramy/sesji opisuje [WEBADMIN.md](WEBADMIN.md).

## Weryfikacja i odtworzenie

Weryfikacja lokalna **alpha-0.4.12** z 2026-11-11: wszystkie **90/90 zestawów CTest** przeszły w Debug, w tym rzeczywisty ncdc ADC/ADCS, tymczasowa MariaDB i Apache HTTPS. Przeszły typecheck/budowanie produkcyjne WebAdmin, 4 testy Node, 6 testów Chromium i rzeczywista integracja C++ API → adapter → React. Obejrzano zrzuty desktop/mobile; test nawigacji/eksportu czatu ponownie przeszedł po poprawce przewijania panelu bocznego. Sprawdzono metadane, UTF-8, lokalne linki dokumentacji, przykładowy INI i białe znaki. Wyniki są lokalne; nie powtarzano Release/ASan/UBSan dla tego etapu UI, a zdalne CI, pozostałe przeglądarki/systemy i obciążenie produkcyjne są niezweryfikowane.

Testy przeglądarkowe obejmują dokładne wartości każdego licznika, lokalizację i zawijanie największej bezpiecznej liczby, link z Przeglądu, bezpośredni `#chat`, aktywną nawigację, przeładowanie, zamykanie menu mobilnego, US/PL, odświeżanie, restart do zera, brak/błąd/brakujące dane API, odzyskanie i wybór pól eksportu. Testy parsera/adaptera zachowują walidację każdego pola i izolację sekretów.

Rzeczywista integracja uruchamia C++, loguje dwa klienty ADC, wysyła BMSG/DMSG/EMSG i wywołuje odmowy przez brak celu oraz limit tekstu. Sprawdza niezerowe liczniki przez Unix API → adapter Vite → React, dziewięć wartości na komputerze/telefonie oraz brak prywatnych znaczników i sekretów w API/HTML/logach. Dotychczasowe zestawy CTest ncdc ADC/ADCS nadal sprawdzają rzeczywistego klienta; ten etap UI nie dodaje funkcji protokołu.

```sh
cd webadmin
npm run build
npm test
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:e2e
PLAYWRIGHT_CHROMIUM_EXECUTABLE=/usr/bin/chromium npm run test:integration -- ../build-debug/dc24h
```

Pomiń `PLAYWRIGHT_CHROMIUM_EXECUTABLE` przy Chromium zainstalowanym przez Playwright. Wyniki opisują [TESTING.md](TESTING.md) i [CHANGELOG.md](../../CHANGELOG.md). Zdalne CI, pozostałe przeglądarki, pełna macierz systemów i obciążenie produkcyjne wymagają osobnych dowodów.
