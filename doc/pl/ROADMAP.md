# Plan rozwoju

| Etap | Zaimplementowany zakres |
| --- | --- |
| alpha-0.1.1 Core | Cykl życia, start/stop, sygnały i logger |
| alpha-0.1.2 Network | TCP IPv4/IPv6, wiele portów i klientów, limity zasobów |
| alpha-0.1.3 ADC | Ograniczony parser strumienia, komendy i walidacja danych |
| alpha-0.1.4 ADCS | Certyfikaty, nieblokujący TLS, testy, dokumentacja i komunikaty US/PL |
| alpha-0.1.5 Configuration | INI, nadpisania CLI, walidacja ścieżek/limitów i logowanie append |
| alpha-0.1.6 Logging | Podsumowania startu, WARN, zdarzenia połączeń, ograniczone UTF-8 i budżety |
| alpha-0.1.7 Security | Ograniczone przyjęcia IP, ramki i terminy bezwzględne |
| alpha-0.1.8 Database | Opcjonalne połączenie lokalne MariaDB, prepared statements i jawny rejestr migracji |
| alpha-0.1.9 API | Lokalne uwierzytelnione HTTP/JSON: stan i health, tylko odczyt |
| alpha-0.1.10 WebAdmin | Układ React/TypeScript/Vite tylko do odczytu, EN/PL, lokalny adapter i statyczny przykład Apache |
| alpha-0.1.11 Apache2 | Przykład statycznego HTTPS, przekierowanie, TLS/nagłówki i testy tymczasowego Apache |
| alpha-0.2.1 Project | Audyt alpha-0.1, poprawki spójności, wymagane integracje w CI i podstawa alpha-0.2 |
| alpha-0.2.2 ADC Parser | Składnia argumentów BASE, pola nazwane, liczby i odzyskiwanie TCP/TLS |
| alpha-0.2.3 ADC State | Niezależny automat huba, jawne zatwierdzanie i kontrola komend/kierunków/SID |
| alpha-0.2.4 SUP | Funkcje AD/RM, BASE/wspólny TIGR, limity i połączenie z automatem |
| alpha-0.2.5 SID | Koder 20-bitowy, ograniczona wspólna pula, rezerwacje RAII i zarządzane sesje SID |

## Proponowane etapy alpha-0.2

alpha-0.2.2 realizuje zamówiony pełniejszy parser. Wcześniejsza propozycja serializera pozostaje planowana; numery dalszych etapów zostaną przypisane po określeniu zakresu.

To plan kryteriów odbioru, nie zaimplementowane funkcje. Ostateczny zakres należy ustalić przed rozpoczęciem każdego numerowanego etapu.

| Proponowany etap | Zakres | Kryterium odbioru |
| --- | --- | --- |
| Plan: ADC output | Serializer, escape, walidacja nagłówków i limity zakodowanych bajtów | Dokładne bajty zgodne z gramatyką; parse/serialize, Unicode, granice i błędy; jeszcze bez zapisu do sieci |
| Plan: Network output | Ograniczone kolejki klienta i łączne, częściowy zapis TCP/TLS, WANT_READ/WANT_WRITE i polityka wolnego odbiorcy | Fragmentowany zapis TCP/ADCS, limity kolejek, sprawiedliwa obsługa przy zatrzymanym odbiorcy, błędy klienta i ograniczony stop; bez nieograniczonych alokacji |
| Plan: Sessions | Integracja stanów/SUP/SID z transportem, rejestr użytkowników, obliczanie Tiger i walidacja INF/tożsamości | Dozwolone/niedozwolone przejścia, unikalność/zwalnianie SID, odrzucanie podszytego źródła i duplikatów; bez wycieku PID |
| Plan: Client integration | Pełne logowanie gościa przez ADC i ADCS, rozłączenia i znaczenie gotowości | Prawdziwy ncdc osiąga normalny stan zalogowania na obu transportach; reconnect, wielu użytkowników, błędna negocjacja i dotychczasowe regresje przechodzą |

Pełny chat, wyszukiwanie i koordynacja transferów pozostają osobnymi etapami po poprawności sesji. Konta wymagają przeglądu uwierzytelniania/uprawnień/haseł oraz migracji aplikacji. Produkcyjna administracja WWW wymaga uwierzytelnionej bramy Apache, sesji/ról przeglądarki, CSRF i audytu przed udostępnieniem. Izolacja/rotacja logów, close_notify/reload TLS, pomiary obciążenia/fuzzing/fault injection i pozostałe systemy to jawne dalsze zadania.

## Gałęzie i ukończenie

alpha-0.2.2 powstaje z main w `ca464e02e714c1e81ed4c37279496168a6c47e23`, po scaleniu obu gałęzi audytu. Nie zmienia samoczynnie main ani znacznika alpha-0.2.

alpha-0.2.1 powstaje z main po scaleniu alpha-0.1.11. alpha-0.2 wskazuje ten sam commit audytu jako znacznik linii; nie zastępuje wymaganych numerowanych gałęzi z main. Scalanie sprawdzonego etapu do main wymaga osobnego polecenia. Zachować historię gałęzi i niezmienne migracje SQL.

Każdy etap wymaga CTest z obowiązkowymi integracjami, testów frontendu gdy zmieniany, kontroli metadanych/dokumentacji i changeloga. Zielony ncdc z początkowym HSUP potwierdza transport, dopóki pełne logowanie nie zostanie zaimplementowane. Zobacz [audyt i priorytety](AUDIT_ALPHA_0.1.md).

alpha-0.2.3 powstaje z main w `4c98e10` po scaleniu alpha-0.2.2. [Moduł stanów](ADC_STATE.md) realizuje samodzielną kontrolę kolejności; wyjście transportu, walidacja funkcji/tożsamości/haseł, unikalność SID i pełne logowanie ncdc pozostają planowane. Etap nie scala się samoczynnie do main.

alpha-0.2.4 powstaje z main w `1dd0b9f1a94626789d1ac9addad991a6eae78910`, po jawnym scaleniu alpha-0.2.3. [Moduł SUP](ADC_SUP.md) implementuje wybór funkcji/hash i łączy go z zatwierdzeniem stanu. Ogólne wyjście, podłączenie transportu, obliczanie hash i pełne logowanie pozostają planowane. Gałąź nie scala się samoczynnie do main.

alpha-0.2.5 powstaje z main w `350a43f69b5134bd049923739cd606f798f846ec` po scaleniu SUP. [SID](ADC_SID.md) realizuje własność alokatora/rezerwacji i połączenie modułów; wyjście/podłączenie sesji i tożsamość pozostają planowane. Gałąź nie scala się samoczynnie do main.
