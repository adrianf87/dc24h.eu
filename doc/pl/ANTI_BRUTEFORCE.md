# Anti-Bruteforce — alpha-0.7.12

Etap powstaje z main `420715822970698018dd093d16b2f5486b676da1`. Dodaje ograniczone
pamięciowo budżety prób uwierzytelnienia w dotychczasowym LoginHub ADC/ADCS,
zachowując jednorazowe GPA/PAS, TLS, role i zarządzanie kontami. Bez automatycznego
scalenia, nowej migracji SQL, formatu hasła i publicznego API administracji.

## Trzy współdzielone budżety

Przy skonfigurowanym źródle uwierzytelniania każdy zaakceptowany do sprawdzenia
początkowy BINF przechodzi kontrolę globalną i per IP przed przygotowaniem decyzji
lub wyzwania. Dotyczy to także decyzji gościa i odmowy tożsamości na hubie kont.
Poprawna decyzja wymagająca hasła dodatkowo obciąża budżet zaufanego ID konta,
zanim IGPA trafi do kolejki. Budżet konta jest wspólny dla IP i listenerów ADC/ADCS,
także po reconnect.

| Klucz INI w `[authentication]` | Domyślnie | Zakres | Znaczenie |
| --- | --- | --- | --- |
| `attempts_per_ip` | 20 | 1–100000 | Początkowy zapas i średnia liczba prób IP na okno |
| `attempts_per_account` | 10 | 1–100000 | Wspólny zapas i średnia liczba prób konta na okno |
| `attempts_per_second` | 128 | 1–100000 | Globalny zapas i średnia liczba prób huba na sekundę |
| `attempt_window_seconds` | 60 | 1–3600 | Horyzont odnawiania i przechowywania dla IP/konta |
| `max_tracked_attempt_ips` | 4096 | 1–65536 | Maksymalna liczba adresów w pamięci |
| `max_tracked_attempt_accounts` | 4096 | 1–65536 | Maksymalna liczba zaufanych ID kont w pamięci |

To algorytm token bucket, a nie stałe lub ścisłe przesuwne okno. Budżet początkowo
jest pełny i stale odnawia się ze wskazaną średnią prędkością, do początkowej
pojemności. Domyślnie wyczerpany IP odzyskuje jedną próbę co trzy sekundy,
a konto co sześć sekund. Poprawny PAS, gotowość, rozłączenie, aktualizacje SUP/INF
i nowe gniazdo nie zwracają ani nie resetują zapasu. Każda rola, także administrator,
podlega tym samym regułom.

```ini
[authentication]
# Dotychczasowe enabled/key_file/max_accounts/require_tls nadal obowiązują.
attempts_per_ip = 20
attempts_per_account = 10
attempts_per_second = 128
attempt_window_seconds = 60
max_tracked_attempt_ips = 4096
max_tracked_attempt_accounts = 4096
```

Zero, nieznane/powtórzone klucze i wartości poza zakresem przerywają walidację,
także przy wyłączonym uwierzytelnianiu kont. Ograniczenia działają po zainstalowaniu
zaufanego źródła uwierzytelniania. Hub wyłącznie gościnny bez źródła działa jak
wcześniej. Dopasuj budżety do reconnect i współdzielonych NAT: dołączenia gości
na hubie kont dzielą IP/globalny zapas z próbami hasła. Dotychczasowe limity
połączeń, oczekiwania, tożsamości, ramek i bajtów pozostają aktywne i nadrzędne.

## Odmowa, odzyskanie dostępu i liczniki

Próba obciąża budżet przed poznaniem wyniku. Błędny/wczesny/niepoprawny PAS,
porzucone wyzwanie i sukces nie resetują więc historii obciążenia. Przyjęte
wyzwanie może dokończyć jednorazową odpowiedź, nawet jeśli inne połączenie później
wyczerpie wspólny budżet. Niepoprawny protokół przed tożsamością nadal podlega
wcześniejszym limitom negocjacji i sprawdzania tożsamości.

Pusty budżet lub pełna tablica oznacza zwykłe `ISTA 223 Authentication failed`
i własne IQUI, jak przy odmowie hasła. Ograniczona próba nie otrzymuje GPA, listy
użytkowników, gotowego powiązania konta ani roli. Przy limicie konta mogło już
powstać wewnętrzne, ograniczone wyzwanie; zostaje anulowane, a oczekiwany skrót
wyzerowany przed wysłaniem IGPA. Reaktor nie wykonuje sleep, SQL, KDF hasła ani
blokującego oczekiwania na timer.

Odzyskanie dostępu wymaga nowego połączenia po odnowieniu odpowiedniej liczby
prób; stare pozostaje terminalne. Sukces nie czyści budżetu. Ograniczanie nie
zmienia statusu, roli, haseł i rewizji konta w bazie. Gotowe połączenia działają
dalej. Jest to tymczasowa kontrola przyjęcia, a nie trwały ban lub reset hasła.

Zwykłe odmowy limitera zwiększają dotychczasowy zbiorczy licznik sieciowy
`security_closed`. Opcjonalny log połączeń używa stałego zdarzenia
`authentication_limit` i lokalnego ID życia połączenia; ostrzeżenie pojawia się
raz na uruchomienie reaktora. Bez kluczy IP, kont/nicków/CID, wyzwań, odpowiedzi,
haseł i pozostałego budżetu konta w logu. HTTP/WebAdmin nie publikuje nowych
danych poszczególnych kont.

## Ograniczona pamięć i własność

`security::AuthenticationLimiter` przechowuje osobne mapy IP/kont w jednym wątku
reaktora. Adres numeryczny jest raz zamieniany na klucz binarny o stałym rozmiarze:
warianty zapisu IPv6 są równe, a IPv4-mapped IPv6 dzieli klucz z IPv4. Port i pola
INF klienta nie wybierają budżetu. Bez DNS i nagłówków proxy. ID konta pochodzi
z zaufanej decyzji wymagającej hasła, nigdy z dowolnego nicka lub numeru klienta.

Każda mapa ma niezależny limit. Odmowa globalna nie tworzy wpisu IP; niepoprawne
ID nie tworzy wpisu konta. Sprawdzanie istniejących kluczy nie alokuje. Błąd
alokacji nie obciąża niezapisanego budżetu; hub bezpiecznie zamyka połączenie
bez przyjęcia użytkownika i ujawnienia wyzwania. Obciążenia wcześniejszych etapów
IP/globalnego pozostają zachowane przy błędzie późniejszego etapu.

Wpis można usunąć po całym oknie od ostatniej przyjętej próby, kiedy jego zapas
może być już pełny. Odmowy nie przedłużają tego wieku. Sprzątanie skanuje
ograniczone mapy najwyżej raz na sekundę aktywności limitera; bez ruchu nie ma
tła sprzątającego. Wyczerpane wpisy nie są przedwcześnie usuwane, żeby zrobić
miejsce. Pełna mapa odmawia nowym kluczom do odzyskania miejsca. Cofnięty czas
jest ograniczany do ostatniego; rachunki odnowienia nie przepełniają się nawet
na skrajach obsługiwanego zegara.

Stan jest lokalny dla procesu i przeżywa rozłączenie, ale nie restart ani nowy
LoginHub. Liczniki klastra, trwałe bany, agregacja prefiksów IP, adaptacyjne opóźnienia,
MFA/CAPTCHA, zewnętrzne rozpoznawanie proxy i limity przyszłego publicznego API
zarządzania pozostają poza etapem. Wspólny limit konta może opóźnić poprawne
logowanie podczas ukierunkowanego ataku rozproszonego; nie wyłącza konta w bazie.
Konfiguracja wdrożenia powinna uwzględnić ruch i ewentualną zewnętrzną kontrolę dostępu.

Ogólne kompromisy ograniczania logowania opisuje
[OWASP Authentication](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html#login-throttling).
Tutaj obowiązuje konkretna powyższa polityka token bucket i limitu pamięci,
bez deklaracji wyeliminowania ataków offline na przechwycone skróty ADC.

## Weryfikacja

`security.authentication_limits` sprawdza walidację, kanoniczne adresy, niezależność
IP/konta/globalną, dokładne granice odnowienia, wspólne budżety ADC/ADCS/reconnect
oraz administratora, brak zwrotu po sukcesie, cofnięty/skrajny czas, pojemność,
sprzątanie i błędy alokacji bez utraty niezapisanego zapasu. Testy LoginHub
potwierdzają terminalną odmowę bez wyzwania/gotowości i odnowienie dostępu.

`accounts.adc_authentication.process` dodaje prawdziwą MariaDB, TCP/TLS z różnych
IP pętli zwrotnej, wspólne limity IP/konta, działanie innych kont, odzyskanie dostępu
ze świeżym wyzwaniem, granice konfiguracji i prywatność logów. Nadal działają testy
prawdziwego ncdc: błędne/poprawne hasło, reconnect, powitanie i czat.
[GitHub CI](GITHUB_CI.md) uruchamia Ubuntu 24.04 Debug/Release i WebAdmin z pełnym
`nproc`, bez czekania na wynik. Wyniki opisują [Testowanie](TESTING.md)
i [Changelog](../../CHANGELOG.md).
