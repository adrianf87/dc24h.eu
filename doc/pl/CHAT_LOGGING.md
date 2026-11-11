# Chat Logging — alpha-0.4.11

Gałąź utworzono bezpośrednio z main `2ad82a2beeb5b8b0963b8908657971c4d011e6dc` po Chat Configuration. Kod i diagnostyka protokołu pozostają EN/US UTF-8; komunikaty operatora, pomoc i dokumentacja mają wersje US i PL.

## Bezpieczne logowanie zdarzeń

Włącz stałe zdarzenia przez `[logging] chat=on` lub `--log-chat on`. Domyślnie logowanie czatu jest wyłączone. CLI ma pierwszeństwo przed INI; dozwolone są dokładnie `on` i `off`. Nieznane, powtórzone i błędne ustawienia blokują start. Poziomy `warning`, `error` i `off` wyłączają zdarzenia INFO oraz ich podsumowania. Konfiguracja jest startowa.

```ini
[logging]
level = info
file = stderr
connections = off
chat = on
```

Dozwolone wyniki to `public_dispatch`, `private_dispatch`, `rejected`, `flood_limited` i `internal_error`. Przykład US: `2026-11-11T00:00:00Z [INFO] Chat event=private_dispatch`; PL używa prefiksu `Czat zdarzenie=` i zachowuje stałe tokeny wyników.

Logger czatu nie otrzymuje treści wiadomości ani powitania, dowolnych pól ADC, nicków, SID/CID/PID, ID sesji, adresów IP, odbiorców, wartości PM ani sekretów. Nie tworzy transkrypcji, historii rozmów, audytu użytkownika ani zapisów w bazie. Domyślnym językiem jest EN/US; język operatora nie zmienia komunikatów protokołu.

Każdy Logger ma osobny budżet 32 zdarzeń czatu na jednosekundowe okno zegara monotonicznego, chroniony mutexem. Nadmiar zwiększa nasycany licznik bez formatowania rekordów. Pierwsze zdarzenie kolejnego okna lub destruktor zapisuje jeden WARN `Chat log events suppressed=N` (PL: `Pominięte zdarzenia logu czatu=N`). Logowanie połączeń zachowuje niezależny budżet 64/s. Podsumowanie może towarzyszyć pierwszemu rekordowi kolejnego okna; nie ma okresowego zegara dla bezczynnych logów.

Dotychczasowy logger zapewnia UTC, ograniczone poprawne UTF-8 w jednym wierszu i obsługę błędów best effort. Błędy alokacji/zapisu nie wychodzą z loggera czatu. Nieudany zapis nadal zużywa budżet; licznik pominięć dotyczy limitu tempa, nie sukcesu zapisu na dysk. Obowiązują zaufany katalog, zwykły plik, brak dowiązań symbolicznych i tryb 0600 dla nowych plików. Zapis pozostaje synchroniczny; etap nie zapewnia ograniczonego opóźnienia dysku ani rotacji/retencji logów.

## Znaczenie liczników

`LoginHub::chat_statistics()` kopiuje bez alokacji liczniki wątku reaktora. Obiekt RAII zapisuje dokładnie jeden wynik na odkodowane klienckie BMSG/DMSG/EMSG przekazane do `LoginHub::receive` po sprawdzeniu zamykania/natychmiastowego zamknięcia. Domyślny wynik to odmowa, co obejmuje wcześniejsze błędy stanu/źródła/flag/polityki. Nieudane przygotowanie oznacza błąd wewnętrzny. Fatalna odmowa jest liczona raz; dalsze wejście zamykanego połączenia jest pomijane. Anti-Flood liczy też odmowy bez powiadomienia. Dodawanie uint64 jest nasycane; nowy hub/serwer zaczyna od zera.

| Pole API `chat` | Znaczenie |
| --- | --- |
| `received` | Kwalifikujące się odkodowane próby B/D/E MSG |
| `public_dispatches` | Wywołania rozsyłania BMSG zakończone DispatchReport |
| `private_dispatches` | Wywołania DMSG/EMSG zakończone DispatchReport, także do siebie |
| `rejected` | Błędy stanu/źródła/składni/flag/polityki lub niedostępny cel przed poprawnym dispatch |
| `flood_limited` | Odmowy Anti-Flood, także bez powiadomienia |
| `errors` | Błędy alokacji przygotowania/diagnostyki lub nieoczekiwane błędy dispatch |
| `queued` | Suma przyjęć do kolejek odbiorców z poprawnych raportów dispatch |
| `unavailable` | Odbiorcy raportowani jako niedostępni przy dispatch |
| `refused` | Odmowy kolejek odbiorców, w tym limit wyjścia lub błąd alokacji |

Przed nasyceniem migawka wątku właściciela spełnia `received = public_dispatches + private_dispatches + rejected + flood_limited + errors`. Próba dispatch jest liczona nawet przy odmowie wszystkich kolejek; nie oznacza dostarczenia przez transport ani odczytu. EMSG zwykle dodaje dwa przyjęcia do kolejek, a do siebie jedno. Brak celu podczas przygotowania zwiększa `rejected`, zamiast `unavailable` z raportu dispatch. Błędy kolejek pozostają odizolowane; nie dodano powtórzeń ani wiadomości.

Nie są liczone surowe ramki odrzucone przez parser, odmowy transportu/bezpieczeństwa przed LoginHub, wejście ignorowane podczas zamykania, zakresy inne niż B/D/E, INF/STA/keepalive, powitalne IMSG ani zaufane `send_hub_message`. Te ścieżki mają dotychczasowe liczniki protokołu/sieci/bezpieczeństwa. Nie dodano statystyk wiadomości systemowych i nie zmieniono dopuszczania, routingu, Anti-Flood, kolejek, gotowości ani odpowiedzi ADC. Znaczenie zakresów opisuje [specyfikacja ADC](https://adc.sourceforge.io/ADC.html).

## Network, API i WebAdmin

Network publikuje dziewięć sum przez niezależne liczniki atomowe w dotychczasowym cyklu i przy wyjściu. Wątki API nie odczytują zmiennych modeli sesji/czatu. `/api/v1/status` otrzymuje stały obiekt liczbowy `chat`; nadal wymagane są prywatne gniazdo Unix i token bearer. Pola API są próbkowane niezależnie i mogą przejściowo pochodzić z różnych aktualizacji; podana równość nie jest gwarancją transakcyjnej migawki HTTP. Rozłączenia nie zerują sum procesu; restart je zeruje.

WebAdmin waliduje i wybiera dziewięć pól, odrzuca brakujące/ujemne/ułamkowe liczby oraz całkowite poza bezpiecznym zakresem JavaScript, usuwa pola dowolne i eksportuje bezpieczne liczniki. Podgląd pokazuje próby publiczne/prywatne, odmowy i Anti-Flood w US/PL. Przyjęcie do kolejki nie oznacza dostarczenia. Serwer i WebAdmin należy aktualizować razem, ponieważ parser klienta wymaga nowych pól; nie ma nowego endpointu ani zapisu przez API.

Statystyki działają również przy wyłączonym lub niesprawnym logowaniu. Network wysyła stałe zdarzenie tylko po wzroście właściwego licznika wyniku; licznik nasycony na maksimum uint64 nie emituje kolejnych zdarzeń. Hub/Core nie wykonują I/O loggera, plików ani bazy.

## Weryfikacja

Weryfikacja lokalna z 2026-11-11: wszystkie **90/90 zestawów CTest** przeszły w Debug, Release i ASan/UBSan z wymaganymi narzędziami integracyjnymi i domyślnym wykrywaniem wycieków, w tym rzeczywisty ncdc ADC/ADCS, tymczasowa MariaDB i Apache HTTPS. Początkowa kontrola metadanych wykryła stare wersje we wstępach README; po aktualizacji dokumentacji przeszła we wszystkich trzech konfiguracjach. Kompilacje nie emitowały ostrzeżeń kompilatora. Przeszły typecheck/budowanie produkcyjne WebAdmin, 4 testy Node, 3 testy Chromium (także dwujęzyczne metryki czatu) i integracja z rzeczywistym API C++. Sprawdzono przykładowy INI, pomoc US/PL, metadane, UTF-8 i lokalne linki dokumentacji. Są to wyniki lokalne; nie potwierdzają zdalnego CI, pełnej macierzy systemów/klientów ani gwarancji obciążenia produkcyjnego.

`chat.logging` obejmuje brak alokacji, nasycenie, dokładny fanout B/D/E/do siebie, brak celu, politykę i ciche odmowy Anti-Flood, fałszywe źródło, ignorowanie zamykanego wejścia, pominięcie systemowego IMSG, sprzątanie/izolację restartów, błąd alokacji i limity kolejek. `chat.logging.process` sprawdza mieszane ADC/ADCS, izolację prywatną przy uporządkowanych barierach INF, dokładne liczniki uwierzytelnionego API, konfigurację i CLI, fałszywe źródło, ciche odmowy, budżet logów i brak treści/identyfikatorów/sekretów.

Testy loggera obejmują domyślne wyłączenie i poziomy, błędne enum, niezależne budżety, współbieżnych autorów, PL, zmianę okna/podsumowanie przy zamykaniu oraz błąd zapisu. Rzeczywisty ncdc ADC/ADCS włącza logi, sprawdza publiczne/prywatne liczniki i zdarzenia oraz zachowuje kontrole prywatności. WebAdmin sprawdza kontrakt, usuwanie pól z eksportu, dwujęzyczne metryki i integrację z rzeczywistym C++ API. Wyniki i ograniczenia opisują [TESTING.md](TESTING.md) oraz [CHANGELOG.md](../../CHANGELOG.md).

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DDC24H_REQUIRE_INTEGRATION_TESTS=ON
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure -j 4
ctest --test-dir build-debug -R 'chat.logging|logging|configuration|adc.ncdc|adcs.ncdc' --output-on-failure
```
