# ADC Authentication — alpha-0.7.8

**alpha-0.7.10 Roles:** [sprawdzone role kont](ROLES.md) są teraz przypisane do gotowych sesji, nadają uprawnienia Search/Transfers i pozwalają personelowi unieważniać sesje niższych ról. Limity globalne i jawne blokady połączeń nadal obowiązują. Zmiany roli w bazie wymagają restartu; bez nowej migracji SQL i publicznego API administracji.

**alpha-0.7.9 Auth Sessions:** [sesje kont związane z połączeniem](AUTH_SESSIONS.md) dodają atomowe powiązanie przy gotowości, zaufane odczyty według konta i unieważnianie połączenia. Zamykanie natychmiast usuwa uwierzytelnienie. Bez nowej migracji SQL, automatycznych ról i sesji przeglądarkowych. CI korzysta z [runnerów GitHub](GITHUB_CI.md) bez czekania na wynik.

Logowanie kont przez GPA/PAS działa w istniejącym reactorze TCP/TLS. Branch powstał z main `a20abc38e731e9cb94b3fbf73aa080e2ccaf273d`; scalenie wymaga osobnej dyspozycji. Tożsamości nieobecne w katalogu kont nadal mogą logować się jako goście.

## Oddzielne hasło i klucz

Dotychczasowy weryfikator hasła konta scrypt zachowuje format i koszt. [Protokół ADC GPA/PAS](https://dc-protocols.github.io/ADC.html) wymaga oryginalnych bajtów hasła do obliczenia Tiger(hasło || wyzwanie); sam weryfikator scrypt nie wystarcza. Zgodnie z wybraną przez użytkownika opcją osobne hasło ADC jest więc szyfrowane w MariaDB, w nowej tabeli schematu v6 `dc24h_account_adc_credentials`. Zalecane jest inne hasło niż hasło konta.

Zapis zawiera `DC24HADC1` (9 bajtów), losowy nonce 12 bajtów, szyfrogram AES-256-GCM i tag 16 bajtów. Dane uwierzytelniane wiążą wersję, ID konta i czas utworzenia. Modyfikacja zapisu, przeniesienie do innego konta lub niewłaściwy klucz powodują odmowę. Polityka hasła odpowiada Registration: co najmniej 15 skalarów Unicode, maksymalnie 1024 bajty UTF-8, bez C0/DEL; spacje, wielkość liter i postać Unicode pozostają dokładne.

Klucz 32 bajtów utwórz jako użytkownik usługi, w prywatnym katalogu przygotowanym przez administratora:

```sh
umask 077
openssl rand -hex 32 > /etc/dc24h/adc.key
chmod 600 /etc/dc24h/adc.key
```

Plik zawiera dokładnie 64 małe cyfry szesnastkowe i opcjonalnie końcowy znak nowej linii. Musi być zwykłym plikiem z jednym dowiązaniem, należeć do efektywnego UID procesu, bez dostępu grupy/innych i bez końcowego dowiązania symbolicznego. Przechowuj go poza bazą i repozytorium, z osobną kopią zapasową. Nie może wskazywać pliku konfiguracji, hasła SQL, tokenu API ani plików TLS; log/API także nie mogą go nadpisać. Utrata klucza uniemożliwia użycie istniejących haseł ADC; dostęp do bazy i klucza umożliwia odszyfrowanie. Automatyczna rotacja, odzyskiwanie i reset nie są zaimplementowane. Własne bufory hasła/klucza/wyzwania są wymazywane; bufory wywołującego, biblioteki, zrzuty systemowe i kopie zapasowe są poza tą gwarancją.

## Przygotowanie i konfiguracja

1. Jawnie wykonaj `dc24h --config /etc/dc24h/dc24h.ini --migrate-database` z uprawnieniami migracji. SQL 1–5 pozostaje bez zmian; v6 dodaje wyłącznie tabelę szyfrowanych haseł ADC. Start huba nie migruje bazy automatycznie.
2. Użyj istniejących zaufanych API C++ Registration, AccountRepository i IdentityRepository: utwórz konto, jawnie je aktywuj i powiąż dokładny nick ADC oraz Tiger CID. Zachowaj PID/CID klienta. Nazwa konta może różnić się od nicka ADC.
3. Wczytaj `AdcSecretKey::load(path)` i wywołaj `AdcCredentialService(connection, *key).enroll(account_id, account_password, adc_password, created_at)`. Wywołujący autoryzuje operację administracyjną; serwis dodatkowo sprawdza oryginalne hasło scrypt. Wymaga powiązania tożsamości, sprawdza dane/czas i prowadzi własną transakcję INSERT. Odmawia ponownego zapisu, bez nadpisania/resetu. Konto może być wyłączone podczas przygotowania, lecz logowanie wymaga statusu active. Hasła są pożyczone; za ich wymazanie odpowiada wywołujący. SQL/scrypt wykonuj poza reactorem.
4. Włącz poniższą konfigurację i uruchom hub ponownie. Schemat musi być aktualny i czysty. Ograniczony katalog startowy sprawdza szyfrogramy przed otwarciem portów; błędna baza, klucz lub zapis zatrzymują start z błędem bez danych poufnych. `--check-config` sprawdza plik klucza i TLS bez połączenia z MariaDB, więc nie potwierdza zgodności szyfrogramów. Polecenia statusu/migracji nie wczytują sekretów kont.

```ini
[authentication]
enabled = on
key_file = /etc/dc24h/adc.key
max_accounts = 1024
require_tls = on
```

Skonfiguruj również bazę i działający nasłuch ADCS z certyfikatami (zobacz [Konfiguracja](CONFIGURATION.md)). Uwierzytelnianie domyślnie jest wyłączone, bez domyślnego klucza. Ścieżka względna jest liczona od katalogu INI. `max_accounts` przyjmuje 1–4096 i obejmuje wszystkie powiązane konta, także wyłączone i bez hasła ADC; przekroczenie limitu zatrzymuje start, zamiast pomijać zastrzeżone tożsamości. Odczyt katalogu wymaga SELECT metadanych migracji, kont, powiązań i haseł ADC, bez tabeli oryginalnych weryfikatorów scrypt. Przygotowanie hasła dodatkowo wymaga uprawnień weryfikacji i zapisu. Nie ma haseł w argumentach CLI, HTTP ani WebAdmin; w tym etapie administrację zapewnia zaufane API C++.

**Statusy, powiązania i hasła są migawką startową. Po zmianie lub odebraniu dostępu zrestartuj hub, aby zastąpić katalog i rozłączyć stare sesje.** Nie ma przeładowania, rotacji ani unieważniania na żywo. Reactor nie wykonuje SQL ani scrypt; odszyfrowuje ograniczony zapis przy tworzeniu wyzwania, natychmiast wymazuje hasło i przechowuje wyłącznie oczekiwany skrót do obsługi PAS.

## Protokół i dostęp

Po SUP/SID i walidacji BINF dokładna para nick/CID aktywnego konta otrzymuje IGPA z 24 losowymi bajtami w kanonicznym base32 bez paddingu. Dopiero po wysłaniu końcowego LF przyjmowana jest jedna odpowiedź HPAS: base32(Tiger-192(bajty UTF-8 hasła || zdekodowane wyzwanie)). Porównanie używa CRYPTO_memcmp. Każde połączenie dostaje nowe wyzwanie, a próba zużywa je niezależnie od wyniku. Przedwczesny, błędny, niekanoniczny lub powtórzony PAS jest odrzucany. Całkowicie nieznane nick i CID pozostają gościem. Jeśli jedno jest zastrzeżone, błędna para, wyłączone konto lub brak hasła ADC powodują odmowę bez przejścia na gościa i publikacji na liście użytkowników.

Domyślnie konta wymagają ADCS. `require_tls=off` to jawna decyzja administratora dla izolowanych testów ADC: przechwycona odpowiedź szybkiego Tiger umożliwia zgadywanie hasła offline. Dopiero poprawny PAS dopuszcza sesję konta. Poprawny PAS poprzedza publikację na liście użytkowników. Istniejąca bariera końcowego LF własnego BINF nadal poprzedza gotowość, czat, wyszukiwanie i transfery; powitanie jest kolejkowane po własnym BINF. Zaufany accessor LoginHub ujawnia ID konta dopiero po gotowości. Role nie są automatycznie nadawane, a czas ostatniego logowania nie jest zmieniany. Konto nie może zmienić nicka/CID na inne niż powiązane, a gość nie może przyjąć zastrzeżonej tożsamości. Odmowa jest ogólna: `ISTA 223 Authentication failed`. Obowiązują istniejące limity wyjścia i czasu. Logi nie zawierają haseł, treści GPA/PAS ani szyfrogramów.

## Weryfikacja

`adc.authentication` sprawdza niezależne wektory Tiger/base32, kolejność wysyłania, błędny/powtórzony PAS, awarie kryptografii/alokacji, wymazywanie i dopuszczenie sesji. `accounts.adc_authentication.process` używa prywatnej MariaDB, rzeczywistych AES-GCM/scrypt, użytkownika SQL bez dostępu do scrypt, testów modyfikacji szyfrogramu/błędnego klucza/duplikatów oraz prawdziwego TCP/TLS i ncdc po ADC/ADCS. ncdc sprawdza odmowę błędnego hasła, reconnect z nowym wyzwaniem, poprawne logowanie, powitanie i czat. Migracje sprawdzają zachowanie danych v5→v6 i odmowę brudnej historii. Zachowano testy gości. Pełne CI Debug/Release/WebAdmin uruchamia jeden Contabo Ubuntu 24.04 z sześcioma workerami; nie czekamy na zakończenie zdalnego przebiegu.
