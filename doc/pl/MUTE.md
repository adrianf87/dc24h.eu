# Mute — alpha-0.8.7

**beta-0.1 Linia beta:** [Kanał wydania](BETA.md) zgłasza `beta-0.1` i zachowuje zachowanie alpha-0.9.17. Schemat v9 pozostaje bez zmian.
**alpha-0.9.17 Zależności:** [Kontrola bibliotek i testy bezpieczeństwa](DEPENDENCIES.md) sprawdza pliki wykonywalne huba, lockfile WebAdmin i zarejestrowane testy bezpieczeństwa. Schemat v9 pozostaje bez zmian.
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

Etap dodaje wyciszenie czatu publicznego i prywatnego, rozwijając
[Temporary Bans](TEMPORARY_BANS.md). Powstaje z main
`bcbba253074ad7dbba8aa75752c51b1e68767475`, bez automatycznego scalenia.
Polityka Admin Core niezależna od protokołu, model sesji i silnik czatu egzekwują
zmianę. Adapter ADC udostępnia polecenia uwierzytelnionym klientom. Schemat SQL **v8**,
wszystkie pliki migracji i manifest pozostają bez zmian.

## Polecenia i uprawnienia

Wyślij zwykły publiczny BMSG z uwierzytelnionego klienta Direct Connect:

```text
+mute <nickname> [public|private|all]
+unmute <nickname> [public|private|all]
```

Nazwa jest dokładna i uwzględnia wielkość liter. Składnia tokenowa obsługuje nicki
bez spacji. Domyślny zakres to `all`. Publiczny i prywatny są niezależne: wyciszenie
public, a potem private, ustawia oba; odciszenie public pozostawia private.
Powtórzenia są idempotentne i liczone jako udane operacje. W tym etapie nie ma
argumentu czasu ani powodu. Błędny zakres, dodatkowe tokeny i flagi MSG są odrzucane
prywatnie. Polecenia w DMSG/EMSG są zwykłym prywatnym tekstem, bez administracji.
Prefiksy takie jak `+muted` pozostają zwykłym czatem.

Wykonawcą jest rzeczywista bieżąca sesja połączenia, nie ID podane przez klienta.
Musi być gotowa, uwierzytelniona, mieć `users.mute` i rolę ściśle wyższą od celu.
Moderator, Operator, Administrator i Owner mają to uprawnienie. VIP wymaga jawnego
`roles.vip_permissions=users.mute` i może objąć Guest/Registered. Samo `users.kick`
nie nadaje mute i odwrotnie. Operacja na sobie, równej/wyższej roli, brakującym lub
zamykanym celu i błędny czas/zakres są odrzucane. Nieznana rola i błędna maska polityki
nie nadają praw. Mute i unmute mają te same warunki; uprawniony wykonawca wyższej
roli może odciszyć cel niezależnie od autora wyciszenia. Wyciszony personel zachowuje
możliwość dozwolonej moderacji.

Kontrola stanu, źródła, MSG i istniejący wspólny limiter wiadomości/bajtów poprzedzają
polecenia. Poprawne, błędne i niedozwolone zastrzeżone polecenia publiczne są
konsumowane prywatnie, poza licznikami zwykłego czatu; ograniczone przez flood nie
docierają do polityki moderacji. Odpowiedź wykonawcy nie zawiera nicka celu, treści
wiadomości ani polecenia. Polecenia nie trafiają do publicznego rozsyłania.

## Egzekwowanie i czas życia

| Zakres | Blokowane wysyłanie |
| --- | --- |
| public | BMSG, także warianty akcji ME0/ME1 i echo nadawcy |
| private | DMSG/EMSG, także PM, akcje, cel własny i prywatne echo |
| all | Oba zakresy |

Sesja ma dwie stałe flagi; nie powstaje nieograniczona mapa, timer, zmiana konta ani
zapis SQL. `MuteCore` sprawdza uprawnienia i zatwierdza przez `SessionManager` bez
alokacji. Gotowość celu i monotoniczny czas przejścia są sprawdzane przed zmianą.
Silnik czatu sprawdza flagi przy przygotowaniu i wykonaniu planu: przygotowany
wcześniej plan nie omija późniejszego wyciszenia. Prywatna odmowa poprzedza szukanie
odbiorcy i nie generuje echa ani informacji o jego dostępności. Nadal obowiązują
limity flood i kolejek wyjścia.

Mute dotyczy wysyłania. Odbiór czatu i komunikatów huba, rola, obecność, wyszukiwanie
i uprawnienia transferów nadal działają. Ogólny ISTA 125 zgłasza wyciszenie bez
rozłączania. Cel otrzymuje stałe powiadomienie zakresu (ISTA 000), a wykonawca prywatne
potwierdzenie lub odmowę. Powiadomienie jest przygotowane przed zatwierdzeniem;
błąd alokacji pozostawia stan bez zmian. Błąd powiadomienia po zatwierdzeniu nie
cofa udanej operacji i nie pozwala na automatyczne ponowienie. Istniejący limit
kolejki może zamknąć przepełnioną sesję. Nick, polecenie i treść nie trafiają do
nowych logów ani API statusu.

Stan pozostaje przy sesji po zmianie nicka gościa. Jest czyszczony na początku
zamykania i nie przechodzi na nowe połączenie, SID, CID ani sesję konta. Rozłączenie,
ponowne połączenie i restart kończą wyciszenie. Trwałe wyciszenie kont i wyciszenie
czasowe są planowane. Mute nie anuluje ramek już zakolejkowanych odbiorcom przed
zatwierdzeniem: mogą jeszcze dotrzeć. Nie dodano aktywnej operacji HTTP ani
uprzywilejowanego wykonawcy wybieranego w przeglądarce.

## Status API i WebAdmin

Prywatne GET `/api/v1/status` dodaje agregaty `mute`, opcjonalne przy odczycie starego
API. Każdy licznik to kanoniczny dziesiętny string do UINT64_MAX, nasycany zamiast
przepełnienia i zerowany przy restarcie:

| Licznik | Znaczenie |
| --- | --- |
| attempts | Operacje docierające do dyspozytora, także błędna składnia |
| applied | Udane wyciszenia, także idempotentne powtórzenia |
| cleared | Udane odciszenia, także idempotentne powtórzenia |
| refused | Odmowa uprawnień, celu, czasu, zakresu lub składni |
| errors | Błąd alokacji powiadomienia przed zatwierdzeniem |
| notice_failures | Błąd powiadomienia celu po udanej zmianie |
| public_blocked | Publiczny MSG zablokowany po kontroli i limiterze |
| private_blocked | Prywatny MSG zablokowany po kontroli i limiterze |

Nie ma licznika aktualnie wyciszonych, listy celów ani audytu. Zablokowany zwykły MSG
zwiększa też dotychczasowe received/rejected czatu, bez dostarczeń. Zaufane użycie
samego silnika czatu nie zmienia liczników adaptera ADC. Network publikuje atomowe
agregaty; każdy licznik może być obserwowany osobno podczas aktualizacji reaktora.

Strona Wyciszenie w EN/US i PL opisuje składnię, zakres i czas życia, pokazując
liczniki z dokładnością BigInt. Normalizacja DTO odrzuca nieznane pola. Stary serwer
bez `mute` pokazuje brak danych; błąd odczytu lub błędny status usuwa stare wartości.
Panel nie wykonuje moderacji. Obowiązuje prywatne gniazdo, Bearer, ten sam UID oraz
adapter przeglądarki tego samego origin. Dziesięć trwałych akcji administracji
pozostaje bez zmian; mute jest osobną uwierzytelnioną usługą sesji, tak jak kick.

## Weryfikacja

Testy natywne obejmują hierarchię i niezależną delegację VIP, gości/niegotowe/zamykane
sesje, błędny zakres/czas/źródło, osobne flagi, błędne i prywatne polecenia, limiter,
przygotowany plan, zatwierdzanie bez alokacji, zmianę nicka/reconnect, odbiór komunikatów
i wyszukiwanie oraz awarie alokacji przed/po zatwierdzeniu. Rzeczywiste TCP/TLS obejmuje
odmowę i przywrócenie obu zakresów, brak echa/wycieku, liczniki i prywatność logów.
Ncdc jest wyciszany i odciszany przez ADC i ADCS; obserwator potwierdza brak
zablokowanej treści i odbiór przywróconego czatu. Node/Chromium obejmują precyzję,
redakcję, stare/błędne API, odświeżenie i mobilny PL, a rzeczywiste C++ API → adapter
→ React nowe pola statusu. Lokalnie używamy ośmiu wątków CPU. GitHub CI zachowuje
Ubuntu 24.04, cztery logiczne CPU na runner i równoległość `nproc`. Nie czekamy na
zakończenie zdalnego CI.
