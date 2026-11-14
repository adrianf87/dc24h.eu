# Transfer Statistics — alpha-0.6.11

**Lokalna weryfikacja alpha-0.6.11 (2026-11-14, Debian 13):** Końcowe pełne przebiegi przeszły 171/171 testów CTest w każdym z trybów Debug, Release i ASan/UBSan z wymaganymi integracjami. Wcześniejszy timeout zmiany trybu ncdc w równoległym Debug przeszedł trzy osobne powtórzenia i końcowy pełny Debug; zachowano termin trzech sekund. Bez ostrzeżeń kompilatora. Przeszły build/typecheck WebAdmin, 14 testów Node, 11 testów Chromium i rzeczywista telemetria transferów C++ API/React. Poprawne actionlint 1.7.12, składnia bash kroków workflow, zgodność nazw liczników C++/TypeScript, metadane, UTF-8/parytet języków/linki lokalne, składnia zmienionego Pythona i białe znaki. Bez oczekiwania na koniec zdalnych Actions/własnego runnera; pozostałe systemy i obciążenie produkcyjne nie są zweryfikowane.

Etap oparty na main `122a682452cc02ee3589819beda5e70c3aca0914`, po Transfer Limits. Dodaje zbiorcze liczniki koordynacji połączeń w LoginHub, Network, zabezpieczonym lokalnym API statusu i istniejącej stronie Połączenia WebAdmina. Nie obserwuje gniazd klientów ani danych plików.

## Kontrakt liczników

`transfers/statistics.hpp` definiuje stałą listę 22 nazw, posiadający dane snapshot `Statistics` i bezalokacyjny zakres `Attempt`. LoginHub zapisuje jedną zdekodowaną próbę CTM/RCM po kontroli zamykania, przed walidacją i alokacjami. Błędy parsera pozostają w sieciowych licznikach niepoprawnych ramek ADC; dane ignorowane po rozpoczęciu zamykania nie dodają próby. Zaufane wywołania samodzielnych Core/Router i lokalnie generowane wyjście nie dodają prób klienta.

Dla każdego prefiksu `ctm_` i `rcm_` eksportowanych jest dziesięć pól:

| Sufiks | Znaczenie |
| --- | --- |
| `received` | Zdekodowane próby, także odrzucone przez stan/źródło/pola |
| `routed` | Zakończone poprawne kierowanie, także przy odmowie/anulowaniu kopii |
| `rejected` | Odmowa protokołu, nieobecnego SID, możliwości lub uprawnień |
| `rate_limited` | Odmowa budżetu liczby/bajtów nadawcy, także cicha |
| `recipient_limited` | Odmowa zbiorczego budżetu liczby/bajtów odbiorcy |
| `errors` | Błąd pamięci walidacji/przygotowania/diagnostyki źródła lub wewnętrzny błąd wysyłania |
| `queued` | Przyjęte niezmienne kopie wyjściowe; D może dodać jedną, E dwie |
| `refused` | Kopie nieprzyjęte przez limit kolejki lub błąd pamięci |
| `canceled` | Kopie anulowane przez ponowne kontrole wysyłania |
| `queue_errors` | Błędy alokacji kolejki odbiorcy; podzbiór nieprzyjętych kopii |

Każda zakończona logiczna próba ma dokładnie jeden wynik: `received = routed + rejected + rate_limited + recipient_limited + errors`, do osiągnięcia nasycenia. Błąd alokacji powiadomienia o limicie/odmowie zastępuje poprzedni wynik przez `errors`. Błędy kolejki pojedynczego odbiorcy pozostają częścią zakończonej próby `routed`, zwiększając `queue_errors` i `refused`; inni odbiorcy nadal mogą przyjąć swoje kopie. Przyjęcie do kolejki nie potwierdza wysłania, połączenia klientów, poprawnego transferu ani ukończenia pliku.

`peer_protocol_unsupported` liczy przyjęte raporty D/E STA klientów z błędem 41, a `peer_connection_failed` raporty z błędem 42. Raport jest liczony raz po poprawnej kontroli źródła/stanu/pól i przyjęciu celu, przed wysłaniem. Poziom recoverable/fatal i echo E nie mnożą wyniku. Brak celu, błędne lub podszyte raporty, B/F/H STA, diagnostyka huba i EOF/reset gniazda nie zwiększają tych pól. Są to niezweryfikowane deklaracje klientów; powtarzane tokeny nie są korelowane ani deduplikowane.

Liczniki nasycają się przy `UINT64_MAX`, nie alokują pamięci, nie zachowują treści żądań i sumują jeden czas życia huba. INF/SUP, zmiana uprawnień, ukończenie wyjścia i rozłączenie klienta nie resetują ich. Nowy start huba zeruje liczniki. Reaktor publikuje stałe pola atomowe przez istniejącą granicę Network/API, także przy wyjściu. Pola odczytywane równolegle nie tworzą transakcji; chwilowy odczyt API może nie spełniać równania między polami.

## API i WebAdmin

Zabezpieczona odpowiedź `GET /api/v1/status` dodaje kompatybilne rozszerzenie v1:

```json
{"transfers":{"statistics":{"ctm_received":0,"ctm_routed":0}}}
```

Skrócony przykład pomija pozostałe dwadzieścia wymaganych pól. Serwer zawsze zwraca wszystkie 22 nazwy z nieujemnymi wartościami całkowitymi. Uwierzytelnianie, prywatne gniazdo Unix, ramkowanie HTTP, limity odpowiedzi i ścieżki endpointów pozostają bez zmian. Rozszerzenie nie zawiera użytkowników, SID, IP/portów, protokołów/tokenów, rekordów pojedynczych użytkowników, ścieżek ani sekretów. Nie eksportuje konfiguracji i nie dodaje operacji administracyjnych zapisu.

Strona Połączenia WebAdmina pokazuje wyniki/kopie CTM i RCM oraz raporty klientów w EN/US lub PL. Odświeżanie i eksport korzystają z istniejących snapshotów. `transfer-status.ts` sprawdza wszystkie obecne liczniki jako nieujemne bezpieczne liczby całkowite i tworzy własny obiekt wyłącznie z dozwolonych pól. Nieznane pola są usuwane. Starsze API bez rozszerzenia działa i pokazuje niedostępność zamiast zer. Brakujące, błędne lub niedokładne pola obecnego rozszerzenia unieważniają snapshot; awarie/restart obsługuje istniejący cykl statusu. API zachowuje pełny zakres uint64, a przeglądarka odrzuca wartości poza dokładnym zakresem JavaScript.

## Weryfikacja

`transfers.statistics` sprawdza dokładne sumy D/E CTM/RCM, odmowy źródła/możliwości/uprawnień, ciche odmowy i oba limity, bezalokacyjne kopie/nasycenie, pojedyncze raporty, zamykanie/rozłączenie/restart i niezależność od Chat/Search. Przebieg 128 jednorazowych awarii alokacji obejmuje błędy źródła, kolejek i powodzenie, zachowując niezwiązanego użytkownika i jeden wynik próby.

`transfers.statistics.process` sprawdza dokładne pola/wartości API dla ADC/ADCS przez IPv4/IPv6, fragmentowane dane, powtarzane tokeny, odmowy limitów, raporty 141/242, nieobecne cele, sumy po rozłączeniu, zerowanie restartu i prywatność API/logów. Istniejące rzeczywiste scenariusze ncdc dla transfer-core, CTM/RCM, możliwości/tokenów/uprawnień/błędów/limitów sprawdzają też podział prób na wyniki; scenariusze limitów i błędów sprawdzają odpowiednie odmowy i raporty klientów.

Testy frontendowe obejmują wszystkie liczniki, brakujące/błędne/niebezpieczne dane, własne kopie i listy dozwolone, prywatność eksportu, starsze API i mobilny widok EN/PL. Integracja z rzeczywistym API C++ sprawdza przekazanie statystyk do adaptera i interfejsu. Zdalne CI i macierz własnego runnera Debiana 13 opisuje [Debian 13 CI](CI_DEBIAN13.md); publikacja gałęzi nie czeka na jej zakończenie.

Zobacz [Transfer Limits](TRANSFER_LIMITS.md), [Error Handling](ERROR_HANDLING.md), [API](API.md), [WebAdmin](WEBADMIN.md) i [Testowanie](TESTING.md). Nie dodano zależności, migracji, klucza konfiguracji, rejestru oczekujących tokenów, pomiaru przepustowości/równoległych transferów ani trwałej telemetrii.
