# WebAdmin — plan bezpiecznej integracji API

Status: **DRAFT / NIEZAIMPLEMENTOWANE**. Punkt wyjścia: chroniony `main` po PR #29 Stage IV (`bbf240d4f5303209c52614b62a877d4c915ad9aa`). Dokument ustala zakres gałęzi `feature/webadmin-secure-api-integration`.

## Cel i granice bezpieczeństwa

Połączyć istniejące możliwości administracyjnego API C++ z uwierzytelnionym WebAdmin za pomocą jawnych, typowanych adapterów. Uwzględnić wszystkie obszary WebAdmin, dla których istnieje obsługiwany kontrakt backendu; operacje niezaimplementowane pozostać mają oznaczone jako niedostępne. **Nie** publikować automatycznie całego prywatnego API. Nie udostępniać `/remote.html`, lokalnego `/admin-api`, kontrolera serwisowego, MariaDB, gniazd Unix, tokenów usługowych ani ogólnego proxy.

Pozostaje model Stage IV: hasło i obowiązkowy ośmiocyfrowy OTP z e-maila, autoryzacja według rzeczywistej roli C++, sesje, CSRF, ograniczenia prób i audyt. Publiczne passkeys/WebAuthn nie należą do zatwierdzonego zakresu. Operacje modyfikujące konfigurację lub działanie usług wymagają osobnej decyzji OWNER.

## Początkowa mapa funkcji (kontrakty i metody do potwierdzenia)

| Obszar WebAdmin | Istniejące API / funkcje | Granica dostępu |
| --- | --- | --- |
| Dashboard i zdrowie | `/api/v1/health`, `/ready`, `/status` | Uwierzytelniony, ograniczony odczyt |
| Użytkownicy i sesje | `/api/v1/users`, konta | Odczyt zależny od roli, redakcja danych |
| Konta i hasła | `/api/v1/accounts`, `/api/v1/admin`, konta Stage IV | Weryfikacja e-mail, indywidualne hasła i uprawnienia backendu |
| Role i uprawnienia | `/api/v1/admin/access`, `/api/v1/management` | Ponowna weryfikacja uprawnień dla operacji |
| Bany, moderacja | `/api/v1/bans`, reguły, kick/mute | Jawne trasy i audyt, bez nieobsługiwanych działań |
| Chat, wyszukiwanie, transfery | Statystyki i monitoring | Ograniczone dane odczytu, jeśli wspierane |
| Audyt i diagnostyka | `/api/v1/audit`, metryki | Wgląd tylko dla uprawnionych; bez sekretów |
| Konfiguracja | `/api/v1/management/configuration`, `/write`, `/restore` | Prywatne domyślnie; zmiany wymagają OWNER GO |
| Zarządzanie usługą | `/api/v1/management/authorize`, `/hub/*` | Prywatne domyślnie; brak ogólnego proxy |

To **rodziny tras**, nie potwierdzenie implementacji każdego wariantu. Należy ustalić dokładne `METHOD + PATH + schemat + uprawnienia + efekty uboczne` na podstawie kodu i testów. Zachować statusy `501/not_implemented`, gdzie występują.

## Lista zadań

- [ ] Zinwentaryzować rzeczywiste endpointy C++ i adaptery WebAdmin oraz oznaczyć odczyt/zapis/brak implementacji.
- [ ] Zdefiniować ścisłą listę tras i metod publicznych, wymagane role, bezpieczne DTO, limity i odmowę dostępu w razie błędu.
- [ ] Wymagać uwierzytelnienia, aktywnej sesji, CSRF przy zapisach, aktualnej roli/stanu konta oraz cofnięcia uprawnień w backendzie.
- [ ] Dodać jawne adaptery Node–C++ bez dynamicznego proxy, tokenu przeglądarkowego i bezpośredniego dostępu do bazy.
- [ ] Podłączyć strony WebAdmin do typowanych adapterów wraz ze stanami odmowy i niedostępności oraz tłumaczeniami EN-US/PL.
- [ ] Sprawdzić role, niedozwolone trasy, metody i nagłówki, Origin, CSRF, OTP, limity, sesje, revocation oraz audyt.
- [ ] Przeprowadzić testy GitHub-hosted Ubuntu 22.04/24.04/26.04 na dokładnym SHA; testy VPS tylko po osobnej zgodzie OWNER.
- [ ] Przeprowadzić przegląd bezpieczeństwa i uzyskać odrębne decyzje OWNER dla ryzykownych operacji oraz późniejszego scalenia.

## Warunek akceptacji

Utworzenie Draft PR nie dodaje i nie aktywuje żadnego publicznego API. Poprzednia akceptacja Stage IV nie jest zgodą na rozszerzenie powierzchni publicznej. Bez nowego OWNER GO nie scalać, nie wdrażać i nie zmieniać DNS/firewalla ani decyzji RELEASE/PRODUCTION.
