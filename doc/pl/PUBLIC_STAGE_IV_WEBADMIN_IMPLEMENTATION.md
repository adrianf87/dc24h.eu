# Etap IV — publiczny WebAdmin: kandydat implementacji i blokady odbioru

**NIE URUCHOMIONO NA VPS / BRAK PUBLICZNEJ AKTYWACJI WEBADMIN.**
Gałąź robocza i draft PR #25 realizują część Issue #10. Publiczny hub
`adcs://server-747585786.dc24h.eu:1512` działa niezależnie i pozostaje
bez zmian. Planowany panel: `https://server-747585786.dc24h.eu:7278`.

## Przygotowany kod

- Oddzielny gateway `webadmin/server/public-gateway.ts` na wewnętrznym
  `127.0.0.1:4278`, z zamkniętą listą tras, bez publicznego `/remote.html`.
- Weryfikacja hasła i uprawnień przez C++ na prywatnym gnieździe Unix oraz
  **obowiązkowy passkey** z kontrolą Origin i RP ID.
- Sesje po stronie serwera, ciasteczka `Secure`, `HttpOnly`, `SameSite=Strict`,
  ochrona CSRF, limity prób, wygasanie, wylogowanie i unieważnianie.
- Zarządzanie własnymi passkeys z ponowną kontrolą autoryzacji; dziennik
  zdarzeń i trwałe limity prób z istniejących bezpiecznych modułów.
- Osobny minimalny interfejs przeglądarkowy oraz **niewłączony** przykład
  Apache HTTPS na TCP 7278, korzystający ze wskazanych plików Let's Encrypt.
- Regresje HTTP, TypeScript, testy ścieżek i Playwright na runnerach GitHub
  Ubuntu; bez self-hosted runnera, bez dostępu do VPS w CI.

## Co wciąż blokuje włączenie do Internetu?

1. **Pierwszy passkey OWNER.** Wymagane jest bezpieczne, nadzorowane
   zarejestrowanie passkey dla produkcyjnej domeny; istniejący passkey
   `localhost` nie pasuje. Nie ma publicznego sposobu ominięcia drugiego
   składnika. Zatwierdzona metoda pierwszego zapisania klucza wciąż wymaga
   projektu i testu na izolowanym środowisku.
2. **Odzyskiwanie.** Obecne narzędzie offline unieważnia urządzenia, lecz
   potrzebna jest dodatkowa niezależna weryfikacja tożsamości, bezpieczne
   ponowne zapisanie passkey, zapasowe kopie i test odtwarzania.
3. **Rzeczywisty backend.** Należy potwierdzić role OWNER i administratorów,
   cofnięcie uprawnień oraz zachowanie restartu/usługi C++ na Debianie 13.
4. **Ciasteczka między portami.** HTTPS na tej samej domenie nie izoluje
   cookies według portu. Wszystkie inne usługi HTTPS hosta muszą zostać
   sprawdzone pod kątem przekazywania lub zapisywania cookies panelu.
5. **Apache/TLS.** Trzeba przetestować prawdziwy vhost, certyfikat i łańcuch,
   SNI, listę tras, otwarte porty, odnowienie certyfikatu i rollback.
6. **CI i test rzeczywistego klienta.** Do odbioru wymagane są zielone wyniki
   testów przeglądarkowych, serwera oraz niezależnych testów integracyjnych,
   w tym testów odmowy i restartu.
7. **Osobna zgoda aktywacyjna.** Dopiero po zamknięciu blokad i zatwierdzeniu
   przez OWNER poleceniem `OWNER WEBADMIN PUBLIC ACTIVATION GO` można
   przygotować uruchomienie portu 7278 na VPS.

Rozwinięcie techniczne znajduje się w
[wersji US](../us/PUBLIC_STAGE_IV_WEBADMIN_IMPLEMENTATION.md).

**Etap IV: implementacja w toku — kwalifikacja i aktywacja nieukończone.**
