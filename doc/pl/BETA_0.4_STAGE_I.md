# Beta 0.4.1–0.4.4 — Etap I: fundamenty bezpieczeństwa

Gałąź `beta-0.4.1-0.4.4-stage1` powstała z `main` po beta-0.3.12.
Docelowy identyfikator Etapu I to `beta-0.4.4`. Ten patch **nie** wdraża
produkcji, WebAuthn ani publicznego WebAdmin. Publiczny Apache i schemat
MariaDB v11 pozostają bez zmian. Scalenie dopiero po CI i zgodzie operatora.

## beta-0.4.1 — CI & Stability

- Poprawki Alpha 0.1 połączono w PR #184; pełne CI #573 przeszło przed
  rozpoczęciem Etapu I. Nowa gałąź wymaga **własnego** zielonego CI.
- Zachowano trzy wersje Ubuntu, C++ Debug/Release, sanitizery, przeglądarkę,
  testy Python/Node i bramki bezpieczeństwa. Dodano gałąź do push CI.
- Testy obejmują teraz rotację TOTP, trwałość anty-replay, uszkodzenie
  audytu, prawa dostępu i odmowę przy awarii dziennika.

## beta-0.4.2 — Security Architecture

Granice zaufania: operator/przeglądarka → zatwierdzony lokalny HTTPS i
opcjonalny tunel SSH → Apache loopback → Node loopback → C++ API przez
prywatne gniazdo Unix. Pięć ścisłych tras `/remote/v1/` pozostaje tylko
do odczytu. Brak dostępu publicznego, zapisu administracyjnego, obniżenia
MFA, wildcard proxy, automatycznego wdrożenia i migracji SQL.

**Projekt przyszłego WebAuthn/passkeys:** stały RP ID i HTTPS origin,
serwerowa weryfikacja challenge, origin, hash RP ID, podpisu, obecności
użytkownika i wymaganej polityką weryfikacji użytkownika. Powiązanie
poświadczenia z kontem, licznikami/flagami kopii, cofnięciem urządzeń,
odzyskiwaniem i ponowną kontrolą ról/grantów przy każdym odczycie. Polityka
attestation i synchronizacji kluczy wymaga osobnej decyzji. Etap I nie dodaje
trasy rejestracji/logowania WebAuthn. TOTP nie chroni przed phishingiem.

Zagrożenia do dalszych testów: phishing, enumeracja kont, replay kodów,
wyścig rotacji, stare sesje, CSRF, spoofing nagłówków proxy, obcy Origin,
kradzież SSH, przejęcie stacji operatora, rollback dysku, brak miejsca,
utrata audytu, przywrócenie starego TOTP i przejęcie hosta. Atakujący z
uprawnieniami root może przepisać cały łańcuch hashy; potrzeba zewnętrznego
punktu kontrolnego.

## beta-0.4.3 — MFA Hardening

TOTP pozostaje związane z kontem i zapisane w prywatnym pliku. Dziennik
anty-replay korzysta z append/fsync. Lokalna komenda operatora
`webadmin/server/remote-mfa-tool.ts` wymienia jeden sekret przez atomową
zmianę pliku w tym samym katalogu oraz fsync pliku i katalogu. **Nigdy nie
resetuje dziennika anty-replay.** Działający wcześniej weryfikator zauważa
zmianę pliku i odmawia logowania do restartu. Zużyty krok TOTP pozostaje
zużyty także po rotacji; można wymagać następnego okresu 30 sekund.

Procedura operatora (bezwzględne ścieżki, katalog 0700, pliki 0600):

1. Niezależnie sprawdź tożsamość użytkownika i jego aktywną rolę. Zatrzymaj
   lokalną bramę i pilotaż SSH; restart cofnie sesje przeglądarki. Zachowaj
   bezpieczną kopię sekretów oraz **całego** dziennika replay. Nie cofaj
   starszej kopii dziennika z tym samym sekretem.
2. Wygeneruj nowy, unikatowy 160-bitowy sekret Base32 w chronionym
   środowisku, dodaj go do aplikacji TOTP i uzyskaj świeży kod. Nie wpisuj
   sekretów, haseł ani kodów w argumenty procesu, historię powłoki,
   GitHub, journald ani plik dostępny dla innych użytkowników.
3. Prywatny plik JSON z prośbą o rotację musi zawierać `schema`, `actor`,
   `new_seed`, `new_code` i `old_code`. Wartości przykładowe:

   ```json
   {"schema":1,"actor":"7","new_seed":"BASE32_SEED","new_code":"000000","old_code":"000000"}
   ```

   `old_code` to aktualny kod **poprzedniego** urządzenia. Przy zatrzymanej
   bramie uruchom polecenie jako konto usługi:

   ```sh
   node --import tsx server/remote-mfa-tool.ts \
     --enrollment /var/lib/dc24h-remote/mfa.json \
     --replay /var/lib/dc24h-remote/replay.jsonl \
     --request /private/operator/rotation.json --offline-ack
   ```

4. **Odzyskiwanie zgubionego urządzenia:** wymagane są niezależna
   weryfikacja tożsamości i zgoda dwóch osób. W pliku pomiń `old_code`,
   dodaj `"approvers":["operator-one","operator-two"]` i uruchom z
   `--recovery`. Obie nazwy muszą być różne, ale są jedynie oświadczeniem
   operatora, **nie kryptograficznym dowodem zatwierdzenia**. Nie ma
   odzyskiwania przez WWW ani obejścia OTP. Udokumentuj incydent poza
   systemem i przejrzyj hasła/granty konta.
5. Uruchom ponownie bramę, sprawdź odmowę starego kodu, działanie nowego
   po ewentualnym odczekaniu 30 sekund, prywatny dostęp i cofnięcie sesji.
   Usuń plik żądania zgodnie z polityką usuwania danych.

Dziennik replay ma limit 1 MiB. Przy **zatrzymanej** usłudze, po zachowaniu
bezpiecznej kopii, można ograniczyć go do najwyższego zużytego kroku każdego
konta, **bez cofania ochrony anty-replay**:

```sh
node --import tsx server/remote-mfa-tool.ts \
  --replay /var/lib/dc24h-remote/replay.jsonl --compact-replay --offline-ack
```

Kompaktowanie atomowo zmienia plik, a wcześniejsze procesy odmawiają
uwierzytelniania do restartu. Nie uruchamiaj go na działającej usłudze.

Nie rotuj plików podczas pracy usługi. Ten kod nie zastępuje rzeczywistej
weryfikacji osoby przez operatorów ani nie udostępnia publicznego resetu.
Ścieżka pliku żądania musi być kanoniczna i bezwzględna, a wszystkie jej
składowe muszą być rzeczywistymi katalogami (bez symlinków). Narzędzie
odmawia otwarcia podmienionego, dowiązanego na twardo lub zbyt szeroko
dostępnego pliku żądania.

## beta-0.4.4 — Audit Foundation

- Opcjonalna brama wymaga teraz `DC24H_REMOTE_AUDIT_FILE` w prywatnym
  katalogu. Plik 0600, jeden hardlink, bez symlinków. Uszkodzony, niepełny,
  zmodyfikowany, zbyt duży (8 MiB) lub niebezpieczny dziennik blokuje start.
- Każde zdarzenie o stałym typie jest synchronicznie dopisywane i fsyncowane
  jako JSONL z numerem oraz poprzednim hashem SHA-256. Restart sprawdza
  ograniczony łańcuch. Nie ma identyfikatorów kont, IP, haseł, OTP, cookie,
  surowych żądań ani sesji.
- Awaria zapisu zwiększa licznik pominiętych wpisów w pamięci i zgłasza
  `AUDIT_LOSS` na stderr. Brama odmawia nowego logowania bez trwałych
  rekordów, a kolejne żądanie unieważnia istniejące sesje. Journald jest
  pomocniczą, pozbawioną sekretów kopią zdarzeń.
- To **fundament integralności i wykrywania utraty**, nie pełny audyt
  zgodności. Wyrwanie końca pliku, root na serwerze, zanik zasilania czy
  utrata alarmu wymagają dodatkowego niezależnego monitoringu.

**Retencja:** operator archiwizuje cały łańcuch poza hostem, porównuje
numery i hashe, zanim zastąpi plik po zatrzymaniu usługi. Proponowany punkt
startowy do decyzji prawnej: 90 dni dostępności i 12 miesięcy archiwum z
ograniczonym dostępem; brak automatycznego kasowania. Obserwuj rozmiar przed
8 MiB, alarm `AUDIT_LOSS` i stan usługi. Po awarii zatrzymaj pilotaż,
zachowaj ślady, sprawdź dysk/prawa i przywróć zweryfikowane archiwum lub
zatwierdź nową epokę dziennika. Nie obcinaj wpisów po cichu.

## Brama wydania i wycofanie

Dokładny końcowy commit musi przejść C++ CTest/ASan/UBSan, testy Python,
TypeScript, Node, Chromium, integrację i audyt zależności. Sprawdź systemd
i HTTPS na serwerze testowym. Nie scalać ani nie wdrażać automatycznie.
Przy wycofaniu kodu zachowaj dziennik anty-replay i archiwa audytu.

Odniesienia: NIST SP 800-63B (wiązanie/odzyskiwanie uwierzytelniaczy i
jednorazowe OTP), W3C WebAuthn Level 3 (origin, RP ID, podpisy).
