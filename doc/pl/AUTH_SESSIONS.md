# Auth Sessions — alpha-0.7.9

**alpha-0.8.4 Kick:** [Wyrzucanie aktywnych sesji](KICK.md) dodaje prywatne polecenie +kick, hierarchię, komunikaty końcowe, natychmiastowe odcięcie routingu i liczniki. Operacje serwisowe i schemat v7 pozostają.

**alpha-0.8.2 Operators:** [Spis zespołu i lokalne tworzenie/przekazanie własności](OPERATORS.md) zastępują wcześniejsze ograniczenia bootstrap/przekazania Ownera. Schemat v7 i zwykła ochrona Ownera pozostają.

**alpha-0.8.1 Permissions:** [model siedmiu ról i uprawnień](PERMISSIONS.md) dodaje nazwane klucze, jawne rangi, ograniczone delegacje VIP, kick gości i transakcyjne zmiany roli. Jawna migracja v7 rozszerza role kont bez zmiany wcześniejszego SQL. Mute, bany, audyt i zapis konfiguracji mają definicje polityki; ich silniki pozostają planowane. Globalne limity obowiązują też Ownera.

**alpha-0.7.12 Anti-Bruteforce:** [wspólne limity prób logowania](ANTI_BRUTEFORCE.md) dodają ograniczone pamięciowo budżety globalne/IP/konta, automatyczne odnawianie i ogólną odmowę dla ADC/ADCS oraz reconnect. Sukces nie zwraca prób; każda rola podlega limitom. Stan pozostaje w pamięci i nie zmienia kont w bazie.

**alpha-0.7.11 Account Management:** [zaufana zmiana hasła, wyłączenie i reset](ACCOUNT_MANAGEMENT.md) łączą atomowy zapis konta/haseł ze świeżym potwierdzeniem, rewizją i usunięciem danych ADC. Wykonuj przy zatrzymanym hubie, następnie jawnie zapisz ADC/aktywuj konto według potrzeby i uruchom hub ponownie. Bez przeładowania na żywo, nowej migracji SQL i publicznego API zarządzania.

**alpha-0.7.10 Roles:** [sprawdzone role kont](ROLES.md) są teraz przypisane do gotowych sesji, nadają uprawnienia Search/Transfers i pozwalają personelowi unieważniać sesje niższych ról. Limity globalne i jawne blokady połączeń nadal obowiązują. Zmiany roli w bazie wymagają restartu; bez nowej migracji SQL i publicznego API administracji.

Etap powstaje bezpośrednio z main `f39d552c174941effd4b9e18ff83f8515059b8f8`.
Dodaje sesje uwierzytelnionych kont związane z połączeniem do istniejącego modułu
Users i adaptera GPA/PAS. Nie scala automatycznie gałęzi z main.

## Uwierzytelnienie i gotowość

Wyzwanie hasła wybiera oczekujące konto, ale nie tworzy sesji uwierzytelnionej.
Poprawny jednorazowy PAS zapisuje tymczasowo zweryfikowane konto w prywatnym
obiekcie połączenia ADC. Dopiero wysłanie końcowego LF własnego BINF powoduje,
że `UserLogin::complete_welcome` atomowo ustawia gotowość zarządzanego użytkownika
i zapisuje `users::AuthSession`. Tymczasowe potwierdzenie jest wtedy usuwane.
Częściowe wyjście, błędny/wczesny/niepoprawny PAS, błędy alokacji powitania oraz
rozłączenie przed gotowością nie tworzą sesji uwierzytelnionej. Gotowe sesje gości
nie otrzymują powiązania z kontem. Pola INF klienta nie nadają konta ani roli.

[Specyfikacja ADC](https://dc-protocols.github.io/ADC.html) opisuje negocjację
protokołu i GPA/PAS. AuthSession jest prywatnym modelem huba, a nie nowym
poleceniem ADC ani tokenem przesyłanym przez sieć.

## Dane i własność

`AuthSession` jest samodzielną wartością zawierającą pięć pól (rola została dodana w alpha-0.7.10):

| Pole | Znaczenie |
| --- | --- |
| `session_id` | Niezerowy lokalny identyfikator życia połączenia; różny od fd, SID ADC i ID konta |
| `account_id` | Sprawdzony dodatni `accounts::AccountId` z zakresu signed BIGINT |
| `transport` | Faktyczny transport `adc` albo `adcs` danego połączenia |
| `established_at` | Jawny monotoniczny czas gotowości, równy `Timeline::ready` |
| `role` | Sprawdzona rola konta z katalogu startowego; uprawnienia i hierarchię opisuje [Roles](ROLES.md) |

Jednowątkowy właściciel `SessionManager` przechowuje powiązanie w istniejącym
obiekcie `Session`. Opcjonalne pole ma stały rozmiar; gotowość, odczyt i usunięcie
nie alokują pamięci. Pojemność wyznaczają dotychczasowe limity sesji/użytkowników.
Wartość nie zawiera hasła, wyzwania, odpowiedzi, szyfrogramu, klucza, nazwy konta,
ani publicznego profilu. Czas nie jest znacznikiem UTC bazy i nie aktualizuje
`last_login_at`.

`Session::authentication()`, `Snapshot::authentication`,
`SessionManager::authenticated(id)` i `LoginHub::authenticated_session(id)` udostępniają
dane gotowych sesji zaufanemu kodowi. Istniejący `authenticated_account(id)`
odczytuje zarządzany rekord. Brak ID, zero, oczekiwanie na logowanie, gość,
zamykanie i rozłączenie nie zwracają sesji konta.

`authenticated_page(account, after, limit)` zwraca kopie rekordów danego konta
w rosnącej kolejności ID życia połączenia; kursor jest wyłączny, a limit wynosi
1–256. Skanowanie jest ograniczone pojemnością rejestru. Strony są niezależnymi
odczytami, a nie zamrożoną transakcją. Błąd alokacji strony nie zmienia stanu.
Zachowana kopia pozostaje danymi historycznymi po zamknięciu; przed operacją
uprzywilejowaną należy ponownie sprawdzić ID życia połączenia. Kopie i numery
kont nie są dowodem uwierzytelnienia.

`mark_authenticated_ready` oraz opcjonalne konto w `UserLogin::complete_welcome`
są operacjami zaufanego adaptera, nie publicznym API logowania. Wywołujący musi
wcześniej sprawdzić hasło i politykę konta. Brak ID, niewłaściwa faza oraz cofnięty
czas są odrzucane bez zmiany modelu. Gotowego gościa nie można awansować,
a sesji uwierzytelnionej przepiąć na inne konto. Model odrzuca także zmianę nicka
przed modyfikacją indeksu nazw; aktualizacje profilu zachowujące tożsamość są dozwolone.

## Zamykanie, unieważnienie i ponowne połączenie

Poprawne rozpoczęcie zamykania, wymuszone zamknięcie i bezpośrednia finalizacja
natychmiast usuwają powiązanie konta, także przy błędzie protokołu, EOF, wygaśnięciu
bezczynności, błędzie wyjścia i wyłączeniu huba. Opróżnianie wyjścia nie zachowuje
uwierzytelnienia. Usunięcie zwalnia dotychczasowe rezerwacje tożsamości; monotoniczny
rejestr odrzuca ponowne użycie ID życia połączenia. Reconnect wymaga nowej wymiany
GPA/PAS i tworzy nowy rekord po własnej barierze gotowości. Unikalność nicka/CID
pozostaje obowiązkowa; etap nie dodaje równoległych logowań tej samej tożsamości.

Zaufany kod wątku właściciela może wywołać `LoginHub::revoke_auth_session(id, now)`.
Dla gotowej sesji konta operacja atomowo wymusza zamknięcie z przyczyną
`authentication_revoked`, usuwa uwierzytelnienie i okna wyszukiwania bez alokacji,
a reaktor rozłącza gniazdo w następnym obiegu. Nie dodaje komunikatu sieciowego
i nie pozwala wznowić starego połączenia. Brak sesji, gość, ponowne unieważnienie
oraz cofnięty czas zwracają false. Opcjonalny log połączeń zawiera tylko stały typ
zdarzenia i lokalne ID życia połączenia.

Operacja dotyczy tylko tego połączenia. Nie wyłącza konta, nie zmienia katalogu
startowego i nie blokuje nowego poprawnego logowania. Zmiany statusu, mapowań
i danych uwierzytelniających w bazie nadal wymagają restartu. Przeładowanie
katalogu na żywo, bany kont, role niestandardowe, sesje przeglądarkowe, JWT/cookies,
publiczne API unieważniania, trwały zapis aktywnych sesji i nowy absolutny TTL
pozostają poza etapem. Dotychczasowe limity bezczynności/logowania/zamykania
nadal działają. Nie ma nowej migracji SQL; schemat v6 i zapis haseł są bez zmian.

## Weryfikacja

`users.auth_sessions` sprawdza fazy i czas, powiązanie konta, gotowość/usunięcie
bez alokacji, izolację gości, niezmienność tożsamości, ograniczone strony rekordów,
stare ID i własność kopii. `adc.authentication` sprawdza publikację po ostatnim LF,
zgodność modelu, unieważnienie przy błędzie alokacji, blokadę ruchu po unieważnieniu
i izolację ponownego połączenia. `accounts.adc_authentication.process` dodaje
powtarzane rozłączenie i świeże logowanie przez TCP i TLS do istniejących testów
prawdziwego ncdc: błędne/poprawne hasło, powitanie i czat oraz prywatna MariaDB.

Debug/Release i WebAdmin korzystają z [runnerów GitHub Ubuntu 24.04](GITHUB_CI.md),
z `nproc` dla kompilacji, testów i przeglądarki. CI jest uruchamiane bez czekania
na wynik; changelog oddziela wyniki lokalne od CI.
