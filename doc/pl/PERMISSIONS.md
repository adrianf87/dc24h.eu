# Uprawnienia i hierarchia ról — alpha-0.8.1

**alpha-0.9.16 Testy przeciążenia:** [Przeciążenie i długotrwała praca](STRESS.md) powtarza istniejące limity ramek, klientów, logowania, czatu i sesji przez stałą liczbę rund. Odmowy zostają. Zajętość wraca do zera. Schemat v9 pozostaje bez zmian.
**alpha-0.9.15 Bezpieczeństwo pamięci:** [AddressSanitizer, UBSan i ThreadSanitizer](MEMORY_SAFETY.md) są osobnymi trybami budowania. Zachowanie huba pozostaje takie same. Schemat v9 pozostaje bez zmian.
**alpha-0.9.14 Optymalizacja:** [Pamięć i routing C++20](OPTIMIZATION.md) publikuje każdą ramkę routingu w jednej alokacji i kolejkuje listę powitalną z już zmierzonego kodowania. Bajty protokołu pozostają takie same. Schemat v9 pozostaje bez zmian.
**alpha-0.9.13 Profilowanie:** [Wykrywanie wąskich gardeł alpha-0.8](PROFILING.md) szereguje stały zestaw decyzji uprawnień, moderacji i ról w procesie. To nie jest profiler próbkujący ani deklaracja pojemności. Schemat v9 pozostaje bez zmian.
**alpha-0.9.12 Pomiary:** [Pomiary wydajności alpha-0.8](BENCHMARK.md) mierzy stały zestaw decyzji uprawnień i moderacji w procesie. To nie jest deklaracja pojemności ani test obciążenia. Schemat v9 pozostaje bez zmian.
**alpha-0.9.11 Monitorowanie:** [CPU, pamięć, sesje i opóźnienia](MONITORING.md) publikuje jedną próbkę procesu i liczniki sesji na istniejącym statusie z bearer. To nie jest monitor hosta ani zapisana historia. Schemat v9 pozostaje bez zmian.
**alpha-0.9.10 Logowanie:** [Bezpieczne logi i rejestrowanie zdarzeń](LOGGING_SECURITY.md) utrzymuje plik logu jako prywatny dla właściciela i zapisuje stałe skutki bezpieczeństwa bez danych klienta. To nie jest audyt produkcyjny. Schemat v9 pozostaje bez zmian.
**alpha-0.9.9 Bezpieczeństwo systemu:** [Utwardzenie usługi Debian i Ubuntu](SYSTEM_SECURITY.md) dodaje jeden przykład systemd, który uruchamia hub jako zablokowanego użytkownika systemowego bez nowych uprawnień i ze ścisłym systemem plików. Budowanie nie instaluje ani nie uruchamia tej usługi. Schemat v9 pozostaje bez zmian.
**alpha-0.9.8 Bezpieczeństwo bazy danych:** [Kopia MariaDB i obsługa awarii](DATABASE_SECURITY.md) zapisuje jedną spójną kopię tylko dla właściciela, gdy operator o to poprosi, i odtwarza ją wyłącznie do pustych tabel tego samego schematu. Utracone potwierdzenie nie jest powtarzane. Kopie poza hostem zostają przy operatorze. Schemat v9 pozostaje bez zmian.
**alpha-0.9.7 Bezpieczeństwo API:** [Uprawnienia, sesje i CSRF](API_SECURITY.md) wymaga identyfikatora sesji i sekretu CSRF przy każdym wywołaniu utrzymaniowym poza dowodem hasła. Sam grant nie czyta już danych. Skopiowanie wszystkich trzech wartości z pamięci strony nadal działa do końca slotu. Schemat v9 pozostaje bez zmian.
**alpha-0.9.6 Bezpieczeństwo konta:** [Hasła, GPA/PAS i ochrona sekretów](ACCOUNT_SECURITY.md) odmawia hasła ADC identycznego z hasłem konta i odszyfrowuje to hasło dopiero po budżecie prób konta i sprawdzeniu bana. Zgadywanie online, przechwycony PAS przy wyłączonym require_tls oraz kubełki czyszczone restartem zostają. Schemat v9 pozostaje bez zmian.
**alpha-0.9.5 Bezpieczeństwo TLS:** [Polityka certyfikatu i szyfrów ADCS](TLS_SECURITY.md) sprawdza certyfikat liścia huba przed nasłuchem i ogranicza ADCS do TLS 1.2 i 1.3 AEAD. Zaufanie, nazwa hosta, HTTP/2, OCSP i automatyczne odnawianie pozostają osobną pracą. Schemat v9 pozostaje bez zmian.
**alpha-0.9.4 Limity ruchu:** [Spam i nadmierny ruch obecności](TRAFFIC_LIMITS.md) ogranicza aktualizacje INF gotowej sesji przed rozesłaniem. Budżety czatu, wyszukiwania i transferów zostają. Schemat v9 pozostaje bez zmian. Limit pasma i publiczna brama pozostają osobną pracą.
**alpha-0.9.3 Bezpieczeństwo sieci:** [Ochrona połączeń TCP, timeouty i limity buforów](NETWORK_SECURITY.md) ogranicza kolejkę nasłuchu, bufory jądra wysyłania i odbioru każdego przyjętego gniazda oraz TCP user timeout. Dotychczasowe timeouty aplikacji i kolejki wyjścia zostają. Schemat v9 pozostaje bez zmian. Limit pasma i publiczna brama pozostają osobną pracą.
**alpha-0.9.2 Bezpieczeństwo ADC:** [Walidacja parsera, fuzzing i kontrola protokołu](ADC_SECURITY.md) sprawdza każdą sparsowaną ramkę klienta względem profilu poleceń huba, zanim zmieni się stan sesji. Deterministyczna kampania fuzzingu w procesie spina parser, dekoder strumienia i ten profil. Schemat v9 pozostaje bez zmian. Fuzzing sterowany pokryciem i publiczna brama pozostają osobną pracą.
**alpha-0.9.1 Model zagrożeń:** [Analiza zagrożeń huba i WebAdmin](THREAT_MODEL.md) zapisuje aktywa, aktorów, granice zaufania i ryzyko szczątkowe obecnego huba oraz lokalnego WebAdmin. Nie dodaje mechanizmu, zmiany schematu, publicznej bramy ani produkcyjnego audytu. Zdalny dostęp administratora pozostaje osobną pracą.
**alpha-0.8.15 Sesje bezpieczeństwa:** [Kontrola dostępu, CSRF, sesje i limity żądań](SECURITY_SESSIONS.md) wiąże każdy grant w pamięci procesu z publicznym identyfikatorem sesji i osobnym sekretem CSRF. Zapisy utrzymaniowe wymagają obu wartości na tym samym slocie. Każda sesja przyjmuje 24 żądania w ciągu 10 sekund. Odczyty nadal wymagają tylko grantu. Wartości zostają w pamięci procesu, nie w SQL, ciasteczkach ani pamięci przeglądarki. Publiczna brama z uwierzytelnieniem pozostaje osobną pracą.
**alpha-0.8.14 Proxy HTTPS Apache2:** [Odwrotne proxy HTTPS i ochrona API](APACHE2_PROXY.md) przekazuje dozwoloną listę API WebAdmin do adaptera na loopbacku. Token bearer klienta, ciasteczka i tożsamość z nagłówków proxy są usuwane. Gniazdo Unix C++ i token usługi pozostają poza Apache. Domyślnie dostęp jest lokalny.
**alpha-0.8.13 Zarządzanie WebAdmin:** [Zarządzanie banami, kontami i rolami](WEBADMIN_MANAGEMENT.md) pokazuje, które obszary utrzymania dopuszcza bieżący grant, i prowadzi do istniejących stron kont, ról i banów. Strona nie wykonuje tych zapisów. Zapis konfiguracji, bieżąca administracja huba oraz kick i mute w przeglądarce pozostają poza tą stroną.
**alpha-0.8.12 Użytkownicy WebAdmin:** [Lista użytkowników i działania moderacyjne](WEBADMIN_USERS.md) publikuje katalog gotowych sesji oraz dwujęzyczną stronę decyzji kick, mute i unmute. Strona nie wysyła tych poleceń, nie tworzy banów i nie ujawnia CID, IP, konta ani pól profilu. Polecenia kick pozostają w uwierzytelnionym kliencie Direct Connect.
**alpha-0.8.11 Panel WebAdmin:** [Panel główny i statystyki](WEBADMIN_DASHBOARD.md) dodaje stałe podsumowanie istniejących liczników procesu do uwierzytelnionej migawki statusu oraz dwujęzyczną stronę WebAdmin. Listy kont, rekordy banów, wiersze audytu, zapisy konfiguracji i bieżąca administracja huba pozostają poza tym panelem.
**alpha-0.8.10 API administracyjne:** [Udowodniony dostęp administracyjny](ADMIN_API.md) wymaga hasła aktywnego konta przed odczytem i zapisem konserwacyjnym. Token procesu pozostaje poświadczeniem transportu. Zapisy konfiguracji i bieżąca administracja huba pozostają w planach.
**alpha-0.8.9 Dziennik audytu:** [Historia udanych działań konserwacyjnych](AUDIT_LOGS.md) jest zapisywana w schemacie v9. Aktywne konto z uprawnieniem audit.read wyświetla tę historię. Kick i wyciszenie na żywo, odmowy, hasła oraz dowolny tekst powodu pozostają poza zapisem. Zapis konfiguracji i administracja huba pozostają w planach.

**alpha-0.8.8 Reguły moderacji:** [Wspólne sprawdzenie uprawnienia i celu](MODERATION_RULES.md) obejmuje kick, mute, unmute oraz bany konta, CID i adresu IP, razem z cofaniem. Schemat v8 pozostaje bez zmian.

**alpha-0.8.7 Mute:** [Moderacja czatu publicznego i prywatnego](MUTE.md) wdraża users.mute z uwierzytelnioną hierarchią, flagami sesji, +mute/+unmute i prywatnymi agregatami. Schemat v8 pozostaje bez zmian. Zastępuje wcześniejsze opisy planowanego mute.

**alpha-0.8.5 Ban Core:** [Trwałe blokady wejścia kont/CID/IP](BAN_CORE.md) dodają jawny schemat v8, dodawanie/cofanie z hasłem, egzekwowanie ze startu i wygaśnięcie. Admin Core ma dziesięć poleceń; users.ban/blocks.manage działają. Zastępuje wcześniejsze opisy v7/planowanych banów.

**alpha-0.8.4 Kick:** [Wyrzucanie aktywnych sesji](KICK.md) dodaje prywatne polecenie +kick, hierarchię, komunikaty końcowe, natychmiastowe odcięcie routingu i liczniki. Operacje serwisowe i schemat v7 pozostają.

**alpha-0.8.2 Operators:** [Spis zespołu i lokalne tworzenie/przekazanie własności](OPERATORS.md) zastępują wcześniejsze ograniczenia bootstrap/przekazania Ownera. Schemat v7 i zwykła ochrona Ownera pozostają.

Hub rozpoznaje siedem stałych ról i siedemnaście nazwanych uprawnień. Rola pochodzi
ze sprawdzonego konta SQL i poprawnego logowania GPA/PAS, nigdy z pól INF/SUP
klienta. Guest oznacza gotową sesję bez konta; nie może być zapisany jako konto
ani związany z uwierzytelnioną sesją konta. Klucze są dokładnymi nazwami EN/US,
z rozróżnieniem wielkości liter. Nieznane role, uprawnienia i maski VIP odmawiają dostępu.

## Role i domyślne przywileje

| Rola | Ranga | Domyślne dodatki do uprawnień |
| --- | --- | --- |
| Guest | 0 | hub.use, chat.public, chat.private; Search/Transfers według ustawień gości |
| Registered | 1 | accounts.password.change i zwykłe uprawnienia do Search/Transfers, niezależne od ograniczeń gości |
| VIP | 2 | Registered i jawnie skonfigurowane dodatkowe uprawnienia |
| Moderator | 3 | users.kick, users.mute, audit.read |
| Operator | 4 | Moderator oraz users.ban, blocks.manage |
| Administrator | 5 | Operator oraz accounts.manage, roles.manage, configuration.manage |
| Owner | 6 | Wszystkie rozpoznane uprawnienia, także hub.manage |

Ranga jest jawna, niezależna od liczbowej wartości enum C++. Dziedziczenie nie
omija globalnych przełączników, odpowiednich reguł gości, uprawnień sesji,
sprawdzenia źródła/tożsamości, wymagań ADCS ani limitów ruchu i zasobów.
Owner ma pełny katalog uprawnień, ale nie przywileje systemu operacyjnego ani
obejście zabezpieczeń. Katalog kont przy starcie jest niezmienny; zapis roli
wymaga prac przy zatrzymanym hubie i restartu.

| Klucz uprawnienia | Dostępny mechanizm w tym etapie |
| --- | --- |
| hub.use | Istniejący silnik gotowych sesji i przyjmowania połączeń |
| chat.public / chat.private | Czat i skonfigurowane reguły |
| search.queries / search.results | Uprawnienia i routing wyszukiwania |
| transfers.initiate / transfers.receive | Uprawnienia i routing połączeń |
| users.kick | Zaufane odbieranie sesji LoginHub, także gotowych gości |
| accounts.password.change | Zmiana własnego hasła po potwierdzeniu obecnego |
| accounts.manage | Usługa kont z hasłem Administratora/Ownera |
| roles.manage | Zmiana roli z hasłem Administratora/Ownera |
| users.mute | Definicja polityki; stan mute i polecenia są planowane |
| users.ban / blocks.manage | Definicje polityki; silniki banów i blokad są planowane |
| audit.read | Definicja polityki; przeglądanie zdarzeń jest planowane |
| configuration.manage | Definicja polityki; Konfiguracja nadal tylko do odczytu |
| hub.manage | Definicja polityki; administracyjne sterowanie cyklem życia jest planowane |

Przydział uprawnienia nie tworzy endpointu, gotowego polecenia ani kompletnej
decyzji dostępu. Katalog zawiera flagę implemented. Panel osobno pokazuje przydział
i dostępność mechanizmu; nie udaje działającego mute, bana, audytu czy edytora
konfiguracji. Natywne API sesji nadal wymaga zaufanego wykonawcy z transportu
i wywołania w wątku właściciela.

## Przywileje VIP

```ini
[roles]
vip_permissions = none
# Opcjonalnie:
# vip_permissions = users.kick,users.mute,audit.read
```

Domyślnie VIP nie otrzymuje dodatków. Dozwolone są tylko te trzy jawne delegacje;
nie można tą opcją nadać administracji kontami/rolami/konfiguracją ani banowania.
Puste wartości, wildcard, nieznane lub powtórzone klucze, duplikaty uprawnień,
zła wielkość liter i końcowy przecinek są odrzucane. Białe znaki wokół elementów
listy są dozwolone. Zastosowanie zmiany wymaga restartu.

VIP z users.kick nadal może działać tylko wobec niższych rang: Guest i Registered.
Delegacja mute/audit opisuje przyszłą politykę, nie dodaje tych silników.
Polityka jest sprawdzana przy INI, Network i zaufanym Core, po czym kopiowana do
niezmiennych ustawień startowych. Nie ma przeładowania na żywo.

## Hierarchia i bezpieczeństwo kont

Moderacja wymaga konkretnego uprawnienia oraz ściśle niższej roli celu. Wykonawca
kick musi być gotową uwierzytelnioną sesją tego huba; gotowy cel może być gościem.
Brakujące, oczekujące, zamykane i wygasłe sesje, własna/równa/wyższa rola i czas
wsteczny powodują odmowę. Kick usuwa powiązanie konta i okna wyszukiwania przed
kolejnym routingiem. Sprawdzanie uprawnień i sprzątanie sesji nie wymaga alokacji.

Operacje na kontach wymagają accounts.manage, aktywnego konta wykonawcy,
aktualnego hasła i niższej roli celu. Administrator zarządza Registered/VIP/
Moderator/Operator. Owner może dodatkowo zarządzać Administratorem. Zmiana własnego
hasła aktywnego konta zachowuje osobną ścieżkę z potwierdzeniem obecnego hasła.
Guest nie jest kontem trwałym.

Zmiana roli wymaga także roles.manage i nowej roli ściśle niższej od roli
wykonawcy. Odmawia dla własnego/równego/wyższego celu, Guest oraz awansu do własnej
lub wyższej rangi. Utworzenie Ownera i przekazanie własności pozostają zaufaną
operacją przygotowania offline. Żadne API nie degraduje, nie wyłącza i nie resetuje
Ownera, co chroni przed przypadkową utratą właściciela. Etap nie tworzy właściciela
produkcyjnego ani nie zmienia istniejących kont.

`AccountManagementService::change_role` wykorzystuje istniejący mechanizm
transakcyjny: sprawdza dane i hasło przed blokadami, blokuje konta według ID,
ponownie sprawdza pełne migawki i rekord hasła, zwiększa rewizję, zmienia rolę,
wyłącza cel i usuwa poświadczenie ADC w jednej transakcji. Hasło konta i mapowania
tożsamości pozostają. Nieaktualne migawki, błędy hasła/uprawnień/SQL i niepewny
COMMIT nie powodują automatycznego powtórzenia zapisu. Nadal trzeba zatrzymać
wszystkie instancje huba korzystające z bazy.

Lokalna trasa administracyjna:

```text
POST /api/v1/accounts/{id}/role
Content-Type: application/x-www-form-urlencoded
revision=...&actor_id=...&proof=...&role=vip
```

Obowiązują prywatne gniazdo Unix i zgodność UID, token Bearer po stronie serwera,
Origin i nagłówek przeglądarki, dokładna ograniczona treść formularza, limit
zapisów i bezpieczne komunikaty. GET metadanych kont wymaga prywatnego tokenu
usługi; nie jest nową sesją logowania użytkownika w przeglądarce. Zapis wymaga
świeżo potwierdzonego konta wykonawcy, nie roli przesłanej przez klienta.

Po zmianie sprawdź rewizję i stan, jawnie aktywuj konto, jeżeli ma działać,
następnie zapisz świeże osobne hasło ADC przez istniejącą zaufaną usługę.
Zatrzymaj administrację i uruchom hub, aby wczytać nowe role. Bieżąca sesja
sieciowa nie jest po cichu awansowana.

## Schemat, API i panel

Jawna migracja **v7**, `0007_roles.sql`, zmienia tylko CHECK kolumny role,
dopuszczając Registered/VIP/Moderator/Operator/Administrator/Owner. Zachowuje
wiersze, ID, rewizje, czas, hasła i tożsamości. Guest i nieznane role pozostają
zabronione w SQL i modelu C++. Pliki migracji 1–6 nie zmieniają bajtów.
Przed --migrate-database wykonaj kopię i zatrzymaj hub. Start nie migruje sam,
a nieczysta/przerwana historia wymaga interwencji. Starsza wersja programu
odmawia nowszej historii; powrót wymaga zgodnej odtworzonej kopii bazy.

Status API i --show-configuration zawierają bezpieczne opcjonalne
configuration.roles: kanoniczne delegacje VIP, siedem wpisów rola/ranga/
uprawnienia i katalog dostępności mechanizmów. Bez kont, haseł i ścieżek.
Strona Role EN/PL pozwala wybrać dowolną rolę i sprawdzić przydziały; starsze API
pokazuje brak polityki. Strona Konta rozpoznaje sześć ról trwałych i pozwala
potwierdzić zmianę roli w trybie administracyjnym. Nie oferuje przydziału Guest/Owner.

## Weryfikacja

Testy obejmują pełną macierz siedmiu ról i siedemnastu uprawnień, hierarchię celów
i przydziału, nieznane wartości, konfigurację VIP, odmowę konta/sesji Guest,
brak alokacji, granicę gotowości, kick VIP i gości, nowe role sesji, limity
routingu, transakcyjne zmiany ról, wyścigi hasła/roli i usuwanie danych ADC.
Tymczasowa MariaDB sprawdza zachowanie v6→v7, nowe wartości, odmowę Guest
i historyczne prefiksy migracji. Rzeczywiste ADC/ADCS loguje VIP/Operator/Owner,
a ncdc pozostaje w testach uwierzytelniania i regresji. Node/Chromium sprawdzają
walidację, pomijanie poświadczeń, etykiety planowanych działań, zmianę roli
i polski widok mobilny.

Lokalny Codex używa ośmiu wątków CPU. CI zachowuje GitHub Ubuntu 24.04,
sprawdza cztery logiczne CPU na runner i wyznacza równoległość z nproc bez stałego
limitu workerów testowych. Nie czekamy na zakończenie zdalnego CI.
