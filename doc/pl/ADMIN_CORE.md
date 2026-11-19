# Admin Core — alpha-0.8.3

**alpha-0.8.8 Reguły moderacji:** [Wspólne sprawdzenie uprawnienia i celu](MODERATION_RULES.md) obejmuje kick, mute, unmute oraz bany konta, CID i adresu IP, razem z cofaniem. Schemat v8 pozostaje bez zmian.

**alpha-0.8.7 Mute:** [Moderacja czatu publicznego i prywatnego](MUTE.md) wdraża users.mute z uwierzytelnioną hierarchią, flagami sesji, +mute/+unmute i prywatnymi agregatami. Schemat v8 pozostaje bez zmian. Zastępuje wcześniejsze opisy planowanego mute.

**alpha-0.8.5 Ban Core:** [Trwałe blokady wejścia kont/CID/IP](BAN_CORE.md) dodają jawny schemat v8, dodawanie/cofanie z hasłem, egzekwowanie ze startu i wygaśnięcie. Admin Core ma dziesięć poleceń; users.ban/blocks.manage działają. Zastępuje wcześniejsze opisy v7/planowanych banów.

**alpha-0.8.4 Kick:** [Wyrzucanie aktywnych sesji](KICK.md) dodaje prywatne polecenie +kick, hierarchię, komunikaty końcowe, natychmiastowe odcięcie routingu i liczniki. Operacje serwisowe i schemat v7 pozostają.

Centralna biblioteka `dc24h_admin` obsługuje teraz operacje kont i własności
z prywatnego API serwisowego i lokalnego `dc24h-owner`. Adaptery budują
`admin::Command`; nie wyszukują już celów zapisu, nie porównują rewizji ani nie
wybierają bezpośrednio pojedynczych operacji kont. Silnik wykonuje tę sekwencję,
a istniejący `AccountManagementService` nadal sprawdza hasła, hierarchię i transakcje.

To synchroniczny silnik **serwisowy**. Przed zapisem zatrzymaj wszystkie huby.
Niezmienne katalogi uwierzytelniania i aktywne sesje nie otrzymują zmian na żywo.
Nie powstaje kanał poleceń ADC, publiczne logowanie administratora ani nowy silnik
banów/mute/audytu. Istniejąca zaufana moderacja `LoginHub` pozostaje osobna.
Schemat v7, pliki SQL migracji i manifest nie zmieniają się. Publikacja wydania
nie zmienia kont produkcyjnych, poświadczeń, migracji ani wdrożenia.

## Polecenia i rozdzielenie punktów dostępu

| Dokładny klucz polecenia | Prywatne API serwisowe | Lokalny CLI właściciela |
| --- | --- | --- |
| accounts.create | Dotychczasowa rejestracja wyłączonego konta | Odmowa |
| accounts.activate | Aktywacja z dowodem hasła | Odmowa |
| accounts.disable | Wyłączenie z dowodem hasła | Odmowa |
| accounts.password | Własne aktywne konto, aktualne hasło | Odmowa |
| accounts.reset | Hasło silniejszego wykonawcy, cel wyłączony | Odmowa |
| accounts.role | Silniejszy wykonawca i niższa nowa rola, cel wyłączony | Odmowa |
| ownership.bootstrap | Odmowa | Pierwszy Owner |
| ownership.transfer | Odmowa | Atomowe przekazanie z dwoma hasłami |

Są to nazwy poleceń, nie nowe uprawnienia ról. Dostępność opisuje tylko punkt
wejścia: operacja nadal wymaga uprawnień konta. Token Bearer umożliwia dostęp do
prywatnego API, lecz nie omija dowodu hasła i hierarchii. Zaufany adapter ustala
Origin przy tworzeniu silnika; formularz HTTP i rola podana przez klienta nie
mogą go wybrać. Nieznane polecenia, konteksty, role, brakujące rewizje i zbędne
pola polecenia są odrzucane przed SQL. Nie ma ogólnego endpointu execute ani
trasy HTTP przekazania własności.

Polecenie zawiera pożyczone widoki haseł, liczbowe odwołania do kont i czas UTC.
Silnik nie zachowuje polecenia, sekretów, ID, nazw ani treści wyjątków. Wywołujący
posiada i czyści hasła po synchronicznym wywołaniu. Rewizja zwykłego wykonawcy
jest opcjonalna dla zgodności istniejących formularzy, ale silnik czyta świeże
dane i usługa sprawdza je ponownie z dokładnym dowodem pod blokadą. Przekazanie
własności wymaga obu jawnych rewizji. Rewizja istniejącego celu jest obowiązkowa.

Dla zapisu silnik sprawdza działanie/kontekst i pola, otwiera jedno połączenie
w bieżącym wątku, pobiera świeże konta, porównuje rewizje i wywołuje usługę.
Wynik zawiera wyłącznie bezpieczne metadane jednego konta lub dwóch kont przy
przekazaniu własności. Rejestracja tworzy konto wyłączone; zmiana roli/reset
wyłączają i unieważniają ADC. Przekazanie zachowuje aktywnego następcę i wyłącza
poprzedniego Ownera. Dowody haseł, uporządkowane blokady, ponowne sprawdzanie
weryfikatorów, rewizje, rollback i unieważnienie ADC pozostają w przetestowanej
usłudze kont. Zwykłe zarządzanie rolami nie nadaje ani nie usuwa Ownera.
Pełny cykl opisują [Operatorzy](OPERATORS.md) i [Uprawnienia](PERMISSIONS.md).

Silnik nie migruje, nie łączy się ponownie i nie ponawia zapisów. Niepewny wynik
SQL pozostaje `outcome_unknown`, również przy utracie potwierdzenia COMMIT;
nie staje się zwykłą odmową ani sukcesem. `std::bad_alloc` jest przekazywany dalej
po zliczeniu niedostępności. Inne błędy zapisów otrzymują stałe rodzaje
`admin::Error`: invalid_input, forbidden, stale, not_found, unavailable lub
outcome_unknown. Konstrukcja błędu i stała diagnostyka nie wymagają alokacji. Treść wyjątków backendu nie jest ujawniana. CLI zachowuje
prywatne pliki, potwierdzenie zatrzymania i kod wyjścia 3 przy nieznanym wyniku.
Trasy HTTP kont i ich formularze pozostają zgodne.

## Bezpieczny katalog i liczniki procesu

Aktualny prywatny serwis udostępnia uwierzytelnione:

```text
GET /api/v1/admin
```

Nadal obowiązują ten sam UID gniazda Unix, prywatny token Bearer po stronie
serwera i ograniczenia HTTP. Działający `dc24h` nie ma handlera administracyjnego
i zwraca 404. Odczyt katalogu nie wykonuje SQL, KDF, odczytu pliku sekretu ani
wyszukiwania kont; nie jest testem zdrowia bazy. Działa również przy awarii SQL.

```json
{
  "maintenance": true,
  "actions": [
    {"key": "accounts.create", "available": true, "local_owner_only": false},
    {"key": "ownership.transfer", "available": false, "local_owner_only": true}
  ],
  "statistics": {
    "attempts": "9", "succeeded": "5", "refused": "2",
    "unavailable": "1", "outcome_unknown": "1"
  }
}
```

Przykład skraca kanoniczny katalog ośmiu poleceń. Liczniki są dokładnymi ciągami
dziesiętnymi uint64 i nasycają się zamiast przepełniać. Próba oznacza wywołanie
`execute` tej instancji z dokładnie jednym końcowym wynikiem. Refused obejmuje
odmowy pól, uprawnień, rewizji i brak konta; unavailable znane błędy usług.
Nieznany zapis ma osobny licznik i nie podnosi succeeded. Uwierzytelnianie HTTP,
błędne formularze, limitowanie żądań i zapytania tylko do odczytu pozostają poza
licznikami poleceń. Liczniki zerują się po restarcie i nie zachowują historii.
Nie są trwałym audytem ani mechanizmem ponawiania/idempotencji. Instancja silnika
ma jednego wywołującego/wątek naraz; serwis ma jeden worker HTTP. Oddzielne
instancje mają oddzielne liczniki.

Dwujęzyczny widok Admin Core używa dozwolonej trasy adaptera
`GET /admin-api/v1/admin`. Adapter i przeglądarka sprawdzają dokładny katalog,
lokalność własności, typy/zakresy liczników i znacznik serwisu oraz usuwają
nieznane pola. Duże liczby nie tracą precyzji JavaScript. Ręczne odświeżanie
czyści stare wartości po odmowie, błędnych danych lub awarii. Starsze API
i działający hub pokazują niedostępność. Nie dodano formularza zapisu: Konta
i Operatorzy zachowują swoje dotychczasowe operacje z hasłem.

## Weryfikacja i dalszy zakres

Testy obejmują parsowanie/pola poleceń, nieznane konteksty/działania, izolację
punktów wejścia, osiem operacji przez centralny silnik, własne hasło, hierarchię,
stare rewizje, brak kont, prywatny katalog HTTP, liczniki, awarię usługi i rollback
przy braku uprawnień SQL. Prywatny proxy MariaDB odrzuca rzeczywiste udane
potwierdzenie COMMIT: silnik zgłasza jeden nieznany wynik, a niezależne połączenie
potwierdza dokładnie jedno atomowe utworzenie konta i poświadczenia. Dotychczasowe
testy rzeczywistego CLI Ownera i cyklu HTTP używają teraz wspólnego dispatchera.
Pozostają regresje ncdc ADC/ADCS. Node/Chromium sprawdzają katalog, precyzję,
redakcję, lokalne etykiety własności, czyszczenie stanu i polski widok mobilny.

Moderacja na żywo przez Admin Core, trwały audyt, silniki banów/blokad/mute,
zdalne sesje administratorów i zapisy konfiguracji na żywo pozostają planowane.
Lokalne kompilacje/testy używają ośmiu wątków. CI GitHub zachowuje Ubuntu 24.04,
cztery logiczne CPU na runner i równoległość z nproc. Nie oczekujemy na jego koniec.
