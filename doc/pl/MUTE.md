# Mute — alpha-0.8.7

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
