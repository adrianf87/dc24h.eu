# Beta 0.7.5–0.7.8 — Etap II: WebAuthn, MFA i sesje

Zakres: Etap II zadania [#238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), wspólna gałąź `beta-0.7.x-stage-full` i jeden roboczy PR #239. Kod przygotowany w poprzednim planie podlega w tym etapie kontroli i dodatkowym testom. Nie jest to zgoda na instalację produkcyjną ani na połączenie z chronioną gałęzią `main`.

## Zasady dla operatora

1. Logowanie WebAdmin nadal wymaga hasła zweryfikowanego przez C++ oraz TOTP **albo** zarejestrowanego passkey. Sam passkey bez hasła nie nadaje uprawnień.
2. Przed dodaniem lub zmianą passkey zaloguj się ponownie: operacja wymaga świeżej sesji (maksymalnie 60 sekund). Sprawdź właściwy adres HTTPS i RP ID; nie akceptuj innej domeny lub certyfikatu.
3. Unieważnienie passkey kończy wszystkie aktywne sesje tego konta. Działający drugi klucz można wykorzystać dopiero do nowego logowania z hasłem. Zmiana nazwy klucza obraca identyfikator sesji i CSRF.
4. Rotację TOTP wykonuj jedynie w zatwierdzonym trybie offline po zatrzymaniu zapisujących procesów. Zachowaj dowód starego i nowego kodu, a przed uruchomieniem usług sprawdź prawidłowość pliku `replay.jsonl`. Stare kody i wcześniejsze kroki czasu muszą pozostać odrzucone.
5. Kopia/odtworzenie musi zawierać zgodny eksport MariaDB v11 i pięć prywatnych plików: `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json`. Nie inicjalizuj pustych plików w celu pominięcia błędu odtworzenia. Nie wykonuj odtworzenia na działających usługach.
6. Hasła, kody TOTP, cookies, klucze prywatne, dane kont i nieoczyszczone logi nie mogą trafić do GitHub Issues, PR ani artefaktów CI. Testy używają syntetycznych kont i izolowanych usług.

## Co sprawdzają testy

- **0.7.5:** rzeczywista ceremonia Chromium z wirtualnym uwierzytelniaczem, odtworzenie zgodnego stanu i odmowa błędnego Origin/RP ID, starej sesji, replay oraz unieważnionego klucza.
- **0.7.6:** dwa klucze, osobne konta, liczniki podpisów i niezmienna kwalifikacja backupu, zmiana nazwy, unieważnianie oraz zakończenie równoległych sesji konta.
- **0.7.7:** cofnięcie zegara, częściowy zapis, rotacja TOTP, zachowanie historii replay, audytu i limitów prób po restarcie.
- **0.7.8:** współbieżne logowania i karty, niezależne CSRF, stare odpowiedzi HTTP, wylogowanie i cofnięcie przyznanych przez C++ uprawnień.

CI obejmuje wyłącznie GitHub-hosted Ubuntu 22.04/24.04/26.04. Zgodnie z instrukcją OWNER po uruchomieniu CI **kończymy działanie bez czekania na wyniki**. Stan `CI pending` nie jest potwierdzeniem zaliczenia etapu. PR pozostaje szkicem; bez osobnej zgody nie wykonujemy merge ani wdrożenia.
