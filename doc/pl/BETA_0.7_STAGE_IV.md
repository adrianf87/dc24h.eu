# Beta 0.7.13–0.7.16 — Etap IV: prywatna obserwowalność WebAdmin

Zakres: [#238](https://github.com/adrianf87/dc24h.eu-high-starter/issues/238), jedna gałąź `beta-0.7.x-stage-full` i jeden roboczy PR #239. Jest to wyłącznie **uwierzytelniony, lokalny widok diagnostyczny tylko do odczytu**. Nie udostępnia start/stop usługi, nadawania uprawnień ani publicznego portu bramki.

## 0.7.13 — pochodzenie i świeżość próbek

- C++ emituje `observation.schema=1` i `observed_unix_ms` w odpowiedzi statusowej. Node przyjmuje próbkę jedynie z prawidłowym znacznikiem i wiekiem `0–10 000 ms` obliczonym względem chwili odbioru. Odrzuca próbki przyszłe, niepoprawne i przeterminowane. Wybór pól danych pozostaje stałą listą liczników zagregowanych.
- Odbiór i odliczanie wieku widoku wykorzystuje zegar monotoniczny. Na ekranie znikają liczniki po 45 sekundach łącznego wieku (wiek u źródła + czas od odbioru). Jeśli odczyt utknie, nie przedłuża okresu ważności.
- Starszy hub v1 bez pola `observation` pokazuje `Nieznane` przy wiarygodności stanu źródła; nie tworzymy fikcyjnej daty pomiaru ani informacji o odtwarzaniu. Prezentowane liczby są jedynie ostatnią odpowiedzią API, a nie poświadczonym stanem bieżącym huba.

## 0.7.14 — historia zmian zagregowanych

- Historia jest tylko w pamięci procesu bramki, mieści **do 12** pozycji, wygasa po **10 minutach** i zawiera wyłącznie `hub_started`, `hub_stopped`, `transport_refusals`, `login_refusals` i `rate_limit_refusals` wraz z wiekiem zdarzenia. Nie importuje surowego audytu, nicków, ID kont, adresów IP, haseł, kodów ani tekstu komend.
- Brak zweryfikowanej historii (`unknown`) różni się od braku zaobserwowanych przejść (`observed`, pusta lista). Po odczycie z huba bez potwierdzenia świeżości, awarii API, przekroczeniu 45 sekund bez nowej próbki lub cofnięciu monotonicznego zegara poprzednia historia zostaje odrzucona.
- Zmiana generacji po resecie liczników transportu albo cofnięciu uptime zeruje poprzednie porównania; incydenty z różnych uruchomień nie są sumowane.

## 0.7.15 — alerty bez danych identyfikujących

- Sygnały odmowy TLS/transportu, logowania i limitowania są tworzone dopiero po **5** incydentach w krótkim oknie; cooldown kolejnego alertu tej klasy wynosi **60 sekund**. Pamięć okien i wyświetlana historia są ograniczone, a wyniki to tylko stałe nazwy zdarzeń i wiek.
- Brak świeżej próbki, błąd odczytu statusu i utrata audytu oznaczają stan nieznany, a nie „zdrowy”. Po restarcie bramki historia i progi nie są odtwarzane z surowych plików. Długotrwała odporność limitów logowania stanowi oddzielne zabezpieczenie; ta historia nie podejmuje decyzji uwierzytelniających.
- Do CI wolno zapisywać jedynie zagregowane, pozbawione identyfikatorów wyniki testów. Nie eksportuj prywatnych JSON, logów lub treści audytu.

## 0.7.16 — dostępność i obsługa

1. Otwórz prywatną stronę WebAdmin na zatwierdzonym lokalnym HTTPS i uwierzytelnij się hasłem oraz TOTP/passkey. Widok nie działa dla anonimowych sesji ani kont bez uprawnień C++.
2. Przełącz język przyciskami `EN`/`PL`; główna część ma ustawione odpowiednie `lang` dla czytnika ekranu. Nawigacja klawiaturą działa w układzie mobilnym, a zmiana stanu ma semantyczne etykiety.
3. `Potwierdź lokalnie` dotyczy **wyłącznie bieżącej zawartości ekranu**. Nie wysyła żądania zapisu do API, a po odświeżeniu ponownie wymaga potwierdzenia. Nie stanowi kwitowania zdarzeń w hubie ani trwałego dziennika operacyjnego.
4. Po wygaśnięciu, cofnięciu sesji, utracie dostępu C++ albo przekroczeniu wieku próbki dane prywatne znikają. Zaloguj się ponownie lub odśwież dopiero po przywróceniu źródła. `Brak zdarzeń` nie jest dowodem braku problemów.

## Weryfikacja i granice

Regresje Node i Playwright obejmują cofnięcie zegara, restart generacji, brak poświadczenia świeżości, utratę audytu i API, ograniczenia alertów, prywatność pól, wygasanie odczytów, EN/PL i klawiaturę. Ostatecznym warunkiem akceptacji jest CI na **dokładnym SHA** w GitHub-hosted Ubuntu 22.04/24.04/26.04. Zgodnie z OWNER **nie czekamy na wyniki CI** po wypchnięciu. `Pending` nie oznacza zaliczenia. Nie wykonujemy merge, instalacji ani wdrożenia bez osobnej zgody.

## Fix Pass 4 — kontrola i poprawki (2026-11-24)

- Po końcowej weryfikacji uprawnień C++ uwzględniamy także czas oczekiwania bramki Node w monotonicznym wieku próbki. Jeśli przekroczy 10 sekund, odczyt zostanie odrzucony jako niedostępny; nie przedstawiamy go jako świeżego. Dodano test opóźnionej kontroli i bezpiecznego wznowienia.
- Cofnięcie zegara między sygnałami audytu a kolejnym odczytem nie może dać ujemnego wieku alertu. Jedna kontrola kolejności czasu obowiązuje odbiór źródła, zdarzenia audytu i odczyt historii. Po wykryciu cofnięcia historia staje się nieznana.
- Wyłączono automatyczne odczytywanie przez czytnik ekranu całego panelu liczników aktualizowanych co sekundę; pojedyncze komunikaty mają nadal semantykę `role=status`. Pozostawiono klawiaturowe potwierdzenie tylko lokalne, bez zmiany uprawnień czy usługi.
- Po wypchnięciu poprawek wymagane jest osobne potwierdzenie pełnej macierzy CI Ubuntu 22.04/24.04/26.04 na dokładnym SHA. Nie oznaczaj zadania jako zaakceptowanego przed sprawdzeniem testów, nie wykonuj merge ani wdrożenia.
