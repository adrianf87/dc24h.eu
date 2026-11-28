# WebAdmin — plan bezpiecznej integracji API

Status: **DRAFT / CZĘŚCIOWO ZAIMPLEMENTOWANE (odczyt)**. Punkt wyjścia: chroniony `main` po PR #29 Stage IV (`bbf240d4f5303209c52614b62a877d4c915ad9aa`). Dokument ustala zakres gałęzi `feature/webadmin-secure-api-integration`.

## Pierwszy etap implementacji — odczyt po zalogowaniu

Na gałęzi funkcjonalnej dodano dokładną trasę `GET /stage4/v1/identity-summary`. Node wywołuje nowy endpoint C++ `GET /api/v1/stage4/identity-summary` przez prywatny most Unix z kontrolą UID i ścisłą listą tras. Usługa kont C++ zwraca wyłącznie zagregowane liczby kont i zweryfikowanych e-maili z MariaDB, a adapter Node waliduje i ogranicza DTO. Brama wymaga aktywnej sesji po haśle i OTP, sprawdza rzeczywistą rolę i rewizję konta także po asynchronicznym odczycie, limituje liczbę żądań i zezwala wyłącznie roli Owner/Administrator. React wyświetla liczniki kont w dotychczasowej stronie logowania; przeglądarka nie otrzymuje tokenu usługi ani grantu C++.

**Granica źródła danych:** usługa C++ kont Stage IV nie jest działającym hubem ADC/ADCS. Jej `/api/v1/status` opisuje syntetyczny stan procesu kont, a `/api/v1/users` nie jest tam dostępne. Dlatego w tej zmianie **nie** udostępniamy statystyk huba ani listy połączonych użytkowników. Wymagają osobnego, ocenionego pod kątem bezpieczeństwa, prywatnego mostu wyłącznie do odczytu. Dotychczasowe prywatne React `/admin-api` pozostaje prywatne.

Operacje zapisu kont, banów i audytu, przekazywanie grantów administratora, zapis konfiguracji i sterowanie usługą **nie są jeszcze zaimplementowane w publicznej bramie**. Żadna zmiana na serwerze produkcyjnym nie została autoryzowana.

## Cel i granice bezpieczeństwa

Połączyć istniejące możliwości administracyjnego API C++ z uwierzytelnionym WebAdmin za pomocą jawnych, typowanych adapterów. Uwzględnić wszystkie obszary WebAdmin, dla których istnieje obsługiwany kontrakt backendu; operacje niezaimplementowane pozostać mają oznaczone jako niedostępne. **Nie** publikować automatycznie całego prywatnego API. Nie udostępniać `/remote.html`, lokalnego `/admin-api`, kontrolera serwisowego, MariaDB, gniazd Unix, tokenów usługowych ani ogólnego proxy.

Pozostaje model Stage IV: hasło i obowiązkowy ośmiocyfrowy OTP z e-maila, autoryzacja według rzeczywistej roli C++, sesje, CSRF, ograniczenia prób i audyt. Publiczne passkeys/WebAuthn nie należą do zatwierdzonego zakresu. Operacje modyfikujące konfigurację lub działanie usług wymagają osobnej decyzji OWNER.

## Początkowa mapa funkcji (kontrakty i metody do potwierdzenia)

| Obszar WebAdmin | Istniejące API / funkcje | Granica dostępu |
| --- | --- | --- |
| Podsumowanie kont Stage IV | Nowe `/api/v1/stage4/identity-summary` w usłudze kont | Zaimplementowany w gałęzi odczyt sumarycznych liczników; CI do potwierdzenia |
| Dashboard i zdrowie | `/api/v1/health`, `/ready`, `/status` | Hub wymaga osobnego mostu; nie utożsamiać statusu procesu kont z hubem |
| Użytkownicy i sesje | `/api/v1/users`, konta | Odczyt użytkowników huba wymaga osobnego mostu oraz redakcji sesji/SID |
| Konta i hasła | `/api/v1/accounts`, `/api/v1/admin`, konta Stage IV | Weryfikacja e-mail, indywidualne hasła i uprawnienia backendu |
| Role i uprawnienia | `/api/v1/admin/access`, `/api/v1/management` | Ponowna weryfikacja uprawnień dla operacji |
| Bany, moderacja | `/api/v1/bans`, reguły, kick/mute | Jawne trasy i audyt, bez nieobsługiwanych działań |
| Chat, wyszukiwanie, transfery | Statystyki i monitoring | Ograniczone dane odczytu, jeśli wspierane |
| Audyt i diagnostyka | `/api/v1/audit`, metryki | Wgląd tylko dla uprawnionych; bez sekretów |
| Konfiguracja | `/api/v1/management/configuration`, `/write`, `/restore` | Prywatne domyślnie; zmiany wymagają OWNER GO |
| Zarządzanie usługą | `/api/v1/management/authorize`, `/hub/*` | Prywatne domyślnie; brak ogólnego proxy |

To **rodziny tras**, nie potwierdzenie implementacji każdego wariantu. Należy ustalić dokładne `METHOD + PATH + schemat + uprawnienia + efekty uboczne` na podstawie kodu i testów. Zachować statusy `501/not_implemented`, gdzie występują.

## Lista zadań

- [x] Dodać licznik kont w C++, adapter Node z limitem i weryfikacją roli, widok React oraz testy odmowy. To **nie** jest komplet wszystkich API.
- [ ] Zinwentaryzować rzeczywiste endpointy C++ i adaptery WebAdmin oraz oznaczyć odczyt/zapis/brak implementacji.
- [ ] Zdefiniować ścisłą listę tras i metod publicznych, wymagane role, bezpieczne DTO, limity i odmowę dostępu w razie błędu.
- [ ] Wymagać uwierzytelnienia, aktywnej sesji, CSRF przy zapisach, aktualnej roli/stanu konta oraz cofnięcia uprawnień w backendzie.
- [ ] Dodać jawne adaptery Node–C++ bez dynamicznego proxy, tokenu przeglądarkowego i bezpośredniego dostępu do bazy.
- [ ] Podłączyć strony WebAdmin do typowanych adapterów wraz ze stanami odmowy i niedostępności oraz tłumaczeniami EN-US/PL.
- [ ] Sprawdzić role, niedozwolone trasy, metody i nagłówki, Origin, CSRF, OTP, limity, sesje, revocation oraz audyt.
- [ ] Przeprowadzić testy GitHub-hosted Ubuntu 22.04/24.04/26.04 na dokładnym SHA; testy VPS tylko po osobnej zgodzie OWNER.
- [ ] Przeprowadzić przegląd bezpieczeństwa i uzyskać odrębne decyzje OWNER dla ryzykownych operacji oraz późniejszego scalenia.

## Warunek akceptacji

Draft PR zawiera jedną nową publiczną trasę odczytową wyłącznie w kodzie gałęzi; nie aktywowano jej na VPS. Poprzednia akceptacja Stage IV nie jest zgodą na rozszerzenie produkcyjnej powierzchni publicznej. Bez nowego OWNER GO nie scalać, nie wdrażać i nie zmieniać DNS/firewalla ani decyzji RELEASE/PRODUCTION.
