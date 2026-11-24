# Beta 0.5.5–0.5.8 — Etap II: bezpieczeństwo i testy

Jednostka pracy 2 z [Issue #234](https://github.com/adrianf87/dc24h.eu-high-starter/issues/234). Całość pozostaje na gałęzi `beta-0.5.x-stage-full` i w jednym roboczym PR #235. Nie uruchamiamy produkcji, publicznego gatewaya, testów Debiana ani prywatnych runnerów.

## Granice zaufania

1. **Przeglądarka → Apache HTTPS → lokalny Node:** chronione cookies HttpOnly/SameSite, CSRF, kontrola Origin/Host/SNI, odmowa obcych nagłówków proxy i nieautoryzowanych tras. Node nasłuchuje tylko na loopback.
2. **Node → prywatne gniazdo C++:** C++ jako jedyny zarządza hasłami, kontami, rolami i grantami. Logowanie do WebAdmin wymaga hasła + TOTP albo hasła + passkey. Każdy chroniony odczyt ponownie sprawdza uprawnienia; awaria API oznacza odmowę.
3. **WebAuthn/TOTP → prywatny stan:** klucze publiczne passkeys, identyfikatory, liczniki, flagi kopii, seedy TOTP i historia wykorzystanych kodów muszą zachowywać powiązanie z kontem i reguły ponownego użycia. Materiał prywatny nie trafia do przeglądarki.
4. **Dysk → odzyskiwanie:** pięć plików `mfa.json`, `replay.jsonl`, `audit.jsonl`, `passkeys.json`, `rate-limits.json` i baza MariaDB v11 tworzą spójną jednostkę odtworzenia. Nie wolno automatycznie zastępować brakujących dzienników pustymi.
5. **Audyt → journal:** wpisy i alerty zawierają tylko stałe nazwy zdarzeń; bez IP, kont, haseł, sesji, tokenów czy odpowiedzi prywatnego API. Łańcuch SHA-256 wykrywa uszkodzenie, ale nie jest podpisem chroniącym przed uprzywilejowanym napastnikiem.

## Ustalenia przeglądu

| Kod | Ryzyko | Poprawka |
| --- | --- | --- |
| S2-01 | Niejednoznaczne kodowanie identyfikatorów i dodatkowe pola katalogu passkeys | 0.5.6: format base64url kanoniczny, ścisła walidacja i przypadki negatywne. |
| S2-02 | Brak pliku replay TOTP powoduje utworzenie pustej historii | 0.5.7: odmowa uruchomienia bez dziennika, jawna inicjalizacja offline, test odtwarzania. |
| S2-03 | Utrata audytu, katalogu passkeys albo limitów może uruchomić pusty stan | 0.5.8: osobny krok inicjalizacji i odmowa startu bez istniejących plików. |
| S2-04 | Cofnięcie zegara może nadmiernie wstrzymać ostrzeżenia | 0.5.8: kontrolowane wznowienie alertów i testy prywatności. |

Nie zakładamy hasłowego logowania bez MFA ani publicznych operacji administracyjnych. WebAuthn wymaga stałego Origin/RP ID, obecności i weryfikacji użytkownika, jednorazowego wyzwania, przypisania poświadczenia do konta i kontroli licznika. `attestation: none` nie potwierdza producenta urządzenia.

## Inicjalizacja i odzyskiwanie

Pierwszą inicjalizację pustych katalogów wykonuje się **wyłącznie offline**, po niezależnym przygotowaniu prywatnego `mfa.json` i zatrzymaniu gatewaya. Utrata pliku na wcześniej działającym systemie wymaga odtworzenia **zgodnej migawki MariaDB + pięciu plików**, a nie stworzenia nowego pustego dziennika. Rejestr TOTP zachowuje najwyższe zużyte kroki. Pęknięty łańcuch audytu, błędne uprawnienia plików lub uszkodzenie limitów logowania blokują dostęp.

Dla **pierwszej instalacji** po utworzeniu poprawnego `mfa.json` (właściciel, `0600`, prywatny katalog `0700`) i zatrzymaniu usługi można z katalogu `webadmin/` uruchomić:

```sh
node --import tsx server/remote-private-init.ts \
  --directory /var/lib/dc24h-remote --rp-id YOUR_FIXED_RP_ID \
  --offline-ack --first-install-ack
```

Podmień `YOUR_FIXED_RP_ID` na rzeczywisty RP ID. Narzędzie tworzy wyłącznie cztery pozostałe pliki, bez sekretów w logach, i odmawia nadpisania choćby jednego istniejącego pliku. Jeśli próba pierwszej instalacji pozostawi stan częściowy, zatrzymaj i sprawdź pliki. **Nigdy** nie używaj tej procedury do zerowania utraconych limitów, sesji, historii TOTP lub audytu — wtedy odtwórz zgodną kopię. Zwykły start usługi i odzyskiwanie passkeys nie tworzą już brakujących plików.

## Kryterium zakończenia

Po czterech patchach wykonujemy pełne CI na **GitHub-hosted Ubuntu 22.04/24.04/26.04**: C++ Debug/Release, WebAdmin, ASan/UBSan, negatywne WebAuthn/CSRF/sesje, browser E2E, testy audytu, systemd oraz odzyskiwanie. Dokumentujemy wynik i SHA w issue #234. Brak wymogu niezależnej akceptacji ani uruchomienia produkcji. Etap III rozpocznie się dopiero przy następnym poleceniu.

**Stan:** Etap II zakończony na `0010814682a84c24b1ddcb6205af207028263dab`; [CI Check status](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45288419207) zakończyło się 12/12, a [izolowane Ubuntu VM](https://github.com/adrianf87/dc24h.eu-high-starter/actions/runs/45288419166) — 3/3. Work unit 2 jest zaznaczone w Issue #234; dalsze etapy pozostają osobnymi zadaniami.

## Fix Pass 2 — ponowny przegląd etapu II (24.11.2026)

Jednostka pracy 6 ponownie analizuje uwierzytelnianie, sesje, audyt i limity logowania z patchy 0.5.5–0.5.8. Wykryto i naprawiono trzy dodatkowe usterki:

1. **Cofnięcie zegara wydłużało ważność sesji i wyzwań logowania.** Sesja pozostawała aktywna mimo czasu wcześniejszego niż jej utworzenie, a wyzwania hasło/TOTP i passkey mogły działać znacznie dłużej od limitu. Dodano dolną granicę czasu dla sesji i wyzwań, sprawdzaną ponownie po asynchronicznej kontroli C++ i WebAuthn. Nowa sesja po długiej autoryzacji powstaje tylko wtedy, gdy wyzwanie nie wygasło.
2. **Weryfikację integralności audytu można było odsunąć w czasie.** Dotychczasowy termin ponownego skanowania łańcucha audytu opierał się tylko na zegarze ściennym. Po cofnięciu zegara kontrola jest wykonywana natychmiast i po wykryciu uszkodzenia nie dopuszcza kolejnych sesji.
3. **Katalog budżetów logowania akceptował nieznane pola.** Warstwa Node była mniej rygorystyczna niż walidator prywatnych kopii. Teraz zarówno nieoczekiwane pola główne, jak i pola pojedynczego limitu powodują odmowę wczytania danych.

Testy regresyjne obejmują cofnięcie zegara w działającej sesji, wyzwanie podczas długiej weryfikacji C++, oczekujące potwierdzenie passkey, ponowny skan audytu oraz uszkodzone pliki limitów. Zachowano wcześniejsze kontrole grantów C++, TOTP replay, powiązania passkey z kontem i odzyskiwania offline. Nie wykonano migracji schematu, operacji na produkcji ani wdrożenia. Pełne wyniki CI dla ostatniego SHA zostaną zapisane w Issue #234 dopiero po pomyślnym zakończeniu wszystkich testów Ubuntu.
