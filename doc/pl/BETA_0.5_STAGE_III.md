# Beta 0.5.9–0.5.12 — Etap III, niezawodność i obsługa WebAdmin

Work unit 3 z [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234), na **tej samej** gałęzi `beta-0.5.x-stage-full` i w draft PR #235. To prace rozwojowe: bez scalenia, publicznego listenera, przywracania danych produkcyjnych, zmiany schematu MariaDB ani zdalnego sterowania hubem. Testy tylko na GitHub-hosted Ubuntu 22.04, 24.04 i 26.04.

## 0.5.9 — Spójność stanu prywatnego i MariaDB v11

Jednostka odtwarzania nadal obejmuje zatrzymaną testową bazę MariaDB v11 oraz pięć plików gatewaya z prawami właściciela: `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`. Narzędzie `scripts/stage4_private_recovery_drill.py` sprawdza format, rozmiar, właściciela, uprawnienia, symlinki, łańcuch audytu i hasze manifestu przed kopiowaniem oraz ponownie przy odtwarzaniu. **Poprawka Etapu III:** identyfikatory, uchwyty i klucze publiczne passkey muszą mieć kanoniczny, nieuzupełniany znakami `=` zapis base64url zarówno w Node, jak i walidatorze kopii. Powtórzone transporty są niedozwolone. Przed poprawką kopia mogła przejść kontrolę, a przywrócona brama odmówiłaby startu.

Manifest pary MariaDB/stan prywatny zawiera źródłowy SHA i oba hasze SHA-256. Przy weryfikacji można podać `--commit-sha`, aby odrzucić kopię z innej rewizji. Nie jest to podpis kryptograficzny ani dowód atomowego zatwierdzenia SQL i plików. Kopię należy wykonać przy zatrzymanych usługach, potem sprawdzić tę samą parę przed importem testowym. Nie wolno cofać historii TOTP ani limitów działającej bramy.

**Decyzja o katalogu passkey:** dla beta-0.5.12 pozostaje mały, prywatny, trwale zapisywany plik. Migracja do MariaDB v11 wymagałaby zmiany schematu i nowych ścieżek błędu transakcji bez udowodnionej korzyści dla pojedynczej bramy. Pięcioplikowe odtwarzanie jawnie wiąże dane. Ponownie rozważyć migrację dopiero przy uzasadnionym użyciu wielu procesów piszących lub zmierzonych problemach spójności, wraz z projektem i testem rollbacku. W tym etapie **bez zmiany SQL**.

## 0.5.10 — Granice Apache2 i systemd

- Osobny vhost TLS nadal nasłuchuje tylko na `127.0.0.1:8443`, dopuszcza wyłącznie stałe ścieżki, sprawdza Host/Origin/Fetch Metadata, usuwa nagłówki z uprawnieniami backendu i nie przekazuje poleceń sterujących hubem.
- Dodano ograniczenia długości żądania i nagłówków, liczby keep-alive oraz timeouty. Negatywne testy prawdziwego Apache sprawdzają zakodowane, nietypowe i przechodzące do katalogu nadrzędnego ścieżki — nawet testowy upstream nie może ich otrzymać.
- Przykładowa jednostka gatewaya zachowuje loopback i prywatny katalog danych, ogranicza restarty do pięciu na minutę, czas zatrzymania, deskryptory, zadania i pamięć. Node/V8 nadal może wykonywać pamięć wygenerowaną przez JIT. Jednostka nie jest automatycznie instalowana ani aktywowana.
- CI wymaga prawdziwego menedżera systemd i uprzywilejowanego testu izolacji. Brak środowiska oznacza błąd, nie pominięcie. Limity zasobów są wartościami rozwojowymi, nie obietnicą wydajności serwera docelowego.

## 0.5.11 — Regresje ADC/ADCS, błędy i restart

Testy realnych procesów loopback obejmują dwa kolejne uruchomienia huba po równoległych, fragmentowanych i błędnych ramkach ADC oraz po seriach niepoprawnych uzgodnień TLS. Po awarii/EOF prawidłowy klient musi ponownie połączyć się, wykonać parsowanie i zakończyć sesję bez błędu. Nadal działają wcześniejsze testy kwot połączeń, TLS 1.2/1.3, AEAD, SIGINT/SIGTERM, deadline wysyłania i ruchu równoległego. To testy porządkowania zasobów, nie pomiar produkcyjny Xeona.

## 0.5.12 — Czytelny i bezpieczny WebAdmin

- Interfejs pokazuje orientacyjny pozostały czas sesji, ale nie ujawnia cookie/CSRF. Po zmianie passkey ponownie sprawdza ważność nowej sesji na serwerze. Spóźniona odpowiedź starej sesji nie może nadpisać nowej.
- Lista urządzeń pokazuje datę utworzenia i ostatniego użycia. Unieważnienie passkey wymaga świadomego potwierdzenia z możliwością anulowania, dostępnymi tekstami EN/PL; ponowne kliknięcia są blokowane na czas operacji.
- Błąd pobrania listy usuwa stare kontrolki urządzeń i pokazuje komunikat awarii. Odmowa 401/403 unieważnia widok sesji. Pozostają testy odpowiedzi spóźnionych, rotacji i odmowy enrollmentu.
- Nadal wyłącznie odczyt stanu huba. Logowanie to **hasło + TOTP** lub **hasło + passkey**; przeglądarka nie przechowuje sekretu TOTP, tokenu API ani grantu backendu.

## Procedura operatora i walidacja

1. Zatrzymać gateway i testową MariaDB przed przygotowaniem spójnej kopii; nie inicjalizować brakującego pliku po działającej instalacji. Zachować pięć plików i dump SQL z manifestem.
2. Użyć procedur offline z [Etapu I](BETA_0.5_STAGE_I.md) i [Etapu II](BETA_0.5_STAGE_II.md). Przy weryfikacji podać oczekiwany SHA, sprawdzić łańcuch audytu i passkey, a SQL importować wyłącznie do pustego środowiska testowego.
3. Poczekać na pełne najnowsze CI GitHub-hosted Ubuntu 22.04/24.04/26.04: C++ Debug/Release, WebAdmin, przeglądarka, ASan/UBSan, prawdziwy systemd, bezpieczeństwo Apache, pomiary i odtwarzanie. Błąd wymaga poprawki i pełnego nowego przebiegu.
4. Zapisać finalne SHA, linki i ograniczenia w Issue #234 oraz PR #235. PR pozostaje draftem, issue otwarte, a dalsze jednostki niezaznaczone; bez wdrożenia produkcyjnego i bez dodatkowej akceptacji zewnętrznej.

**Stan walidacji:** aktualne dowody CI i checklista Etapu III znajdują się w Issue #234. Nie deklarujemy niezależnego odbioru bezpieczeństwa ani sprzętu docelowego.
