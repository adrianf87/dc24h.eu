# Kick — alpha-0.8.4

**alpha-0.8.5 Ban Core:** [Trwałe blokady wejścia kont/CID/IP](BAN_CORE.md) dodają jawny schemat v8, dodawanie/cofanie z hasłem, egzekwowanie ze startu i wygaśnięcie. Admin Core ma dziesięć poleceń; users.ban/blocks.manage działają. Zastępuje wcześniejsze opisy v7/planowanych banów.

Uwierzytelniony członek zespołu może wyrzucić aktywnego użytkownika z klienta
ADC/ADCS, wysyłając zwykły tekst czatu publicznego:

```text
+kick <nick> [powód]
```

Dokładny mały prefiks musi zaczynać wiadomość. Podaj aktualny nick zgodny z
istniejącym rejestrem. Brak powodu oznacza `Kicked from hub`. Powód ma do 256
bajtów UTF-8, musi być już NFC, bez znaków sterujących, formatowania i separatorów
wierszy/akapitów. Jest to konwencja czatu tego huba, nie nowa komenda protokołu ADC.
DMSG/EMSG nie uruchamia administracji. Dodatkowe flagi, w tym ME/PM, oznaczają
odmowę dla zastrzeżonego publicznego prefiksu. Pozostały czat działa jak wcześniej.
Nie dodano zapisu kick przez przeglądarkę ani HTTP.

## Uprawnienia i Admin Core na żywo

`admin::KickCore` to niezależna od protokołu polityka bez alokacji. Korzysta
z SessionManager reaktora i RolePolicy ze startu. Sprawdza faktycznego gotowego,
uwierzytelnionego wykonawcę, `users.kick`, gotowy cel i ściśle niższą rangę.
Moderator/Operator/Administrator/Owner mają domyślny grant. VIP wymaga jawnego
`[roles] vip_permissions=users.kick` i może wyrzucić tylko Guest/Registered.
Guest i Registered nie mogą wyrzucać. Siebie, równych/wyższych, brakujące,
oczekujące, zamykane i stare ID sesji odrzucamy. Nieznane role/polityki odmawiają.
Czas cofnięty nie zmienia celu.

Zaufane `LoginHub::kick(actor_lifetime, target_lifetime, reason, now)` używa ID
życia sesji, nie numeru fd ani domniemanego konta/SID. Adapter czatu rozwiązuje
nick raz do bieżącej sesji; wykonawcą jest rzeczywiste połączenie odbierające.
Najpierw działają sprawdzenia stanu, źródłowego SID i pól MSG. Podszycie pod
innego nadawcę odpada przed kick. Role pochodzą z GPA/PAS konta, nie flag INF.
Wspólne tokenowe limity MSG obowiązują przed dispatch, również dla błędnych
poleceń i odmów. Wyłączenie zwykłego czatu publicznego nie wyłącza oddzielnej
funkcji administracyjnej. Nie ma awaryjnego rozesłania polecenia jako czatu.

Serwisowy Admin Core zachowuje osiem działań kont/własności i wymaganie zatrzymania
hubów. Kick na żywo używa oddzielnej części `dc24h_moderation` w reaktorze.
Nie wykonuje SQL, KDF, blokad ani modyfikacji sesji z workera API. Zmiany kont/ról
nadal wymagają zatrzymania i restartu, jak przy istniejącym niezmiennym katalogu
uwierzytelniania. Nie dodano zmiany grantów na żywo.

## Zamykanie i prywatność

Adapter sprawdza ograniczony powód i przygotowuje komunikaty przed zmianą celu.
KickCore ponownie sprawdza uprawnienia i rozpoczyna zamknięcie z drenażem przez
`CloseCause::kicked`. Natychmiast usuwa uwierzytelnienie/gotowość, a adapter usuwa
wszystkie okna wyszukiwania celu. Czat, Search i Transfers odrzucają tę sesję
przed fizycznym zamknięciem transportu. Hasła, mapowania i SQL pozostają bez zmian.

Cel otrzymuje fatalne `ISTA 230`, a potem własne końcowe `IQUI`: inicjator `ID`,
prywatny powód `MS` i `TL0`. Po tym IQUI hub nie wysyła dalszych danych.
Wykorzystano [definicje STA/QUI protokołu ADC](https://adc.sourceforge.io/ADC.html#_qui).
Kick nie banuje: TL0 nie narzuca opóźnienia powrotu, a nowe GPA/PAS nadal działa
z tymi samymi danymi. Nie ma flagi przerywania transferów, przekierowania,
trwałej blokady ani automatycznego wyłączenia konta.

Częściowo wysłana/przypięta przez TLS pierwsza ramka zachowuje adres i pozostałą
długość. Późniejsze zwykłe ramki są usuwane, a komunikaty końcowe podlegają
istniejącym limitom kolejki klienta/globalnej i terminom zamknięcia. Błąd pamięci
przygotowania pozostawia cel bez zmian. Po zatwierdzeniu zamknięcia awaria kolejki
lub pamięci wymusza szybkie zamknięcie; kick pozostaje sukcesem, a licznik
notice_failures odnotowuje brak powiadomienia. Nie ma ponowienia. Wolny klient
nie może dalej korzystać z routingu podczas opróżniania wyjścia.

Wykonawca dostaje tylko prywatny status sukcesu/odmowy. Polecenia i powody nie
trafiają do pokoju, również przy błędnym lub odrzuconym zastrzeżonym prefiksie.
Pozostali dostają dokładnie jedno zwykłe `IQUI <sid>` przy fizycznym rozłączeniu,
bez inicjatora i powodu. Log połączeń używa stałego `event=kicked`, bez tekstu,
nicku, powodu i haseł. Istniejące sprzątanie zwalnia indeksy, SID, wyjście i limity.
Powtórne żądanie nie wyrzuca zamkniętej sesji i nie publikuje drugiego odejścia.

## API statusu i WebAdmin

Prywatne `GET /api/v1/status` dodaje zbiorczy obiekt `kick`. Pięć pól to ciągi
dziesiętne uint64 nasycane zamiast przepełniane:

| Pole | Znaczenie |
| --- | --- |
| attempts | Próby przyjęte do dispatch, w tym odmowy składni/uprawnień |
| succeeded | Sesje pozbawione gotowości przez kick |
| refused | Odmowy składni, uprawnień, celu lub czasu |
| errors | Błędy zasobów przed zatwierdzeniem kick |
| notice_failures | Udane kick bez możliwości zakolejkowania powiadomienia |

Pierwsze cztery opisują wyniki; notice_failures to dodatkowy podzbiór sukcesów.
Błędny protokół i wspólny flood są odrzucane wcześniej i nie wchodzą do dispatch.
Zastrzeżone polecenia nie podnoszą zwykłych liczników rozsyłania czatu. Liczniki
są lokalne dla procesu, zerowane po restarcie i próbkowane niezależnie między
wątkami; jedna próbka nie gwarantuje transakcyjnej sumy. DTO nie ma ID, nicków,
powodów, historii ani treści poleceń. Serwis pokazuje zerowe liczniki na żywo,
a nie działający endpoint kick.

Dwujęzyczny widok Kick pokazuje instrukcję i liczniki tylko do odczytu. Opcjonalne
pola zachowują zgodność starszych API; adapter/przeglądarka sprawdzają zakresy,
zachowują precyzję uint64 i usuwają dodatkowe pola. Awaria/błędny snapshot czyści
wartości przez istniejący polling. Przeglądarka nie może podać cudzego ID wykonawcy.
Rzeczywistą operację wykonuj z uwierzytelnionego klienta Direct Connect.

## Weryfikacja i dalszy zakres

Testy natywne obejmują hierarchię, delegację VIP, odmowę dla siebie/równych/wyższych/
Guest, oczekujące/cofnięte/stare sesje, spoofing, flood, prywatność poleceń,
wyłączony czat, czyszczenie Search, przypięte wyjście, błędy alokacji i limit kolejki.
Rzeczywiste TCP/TLS sprawdzają pola końcowe, jedno publiczne odejście, brak sekretów
w logach, liczniki i świeże GPA/PAS. Rzeczywisty ncdc wyświetla powód w ADC i ADCS.
Node/Chromium sprawdzają DTO, redakcję, precyzję i polski widok mobilny; integracja
C++/adapter/React dodatkowo sprawdza licznik odmowy dla gościa.

Trwałe bany, mute, historia audytu, zdalna sesja administratora i kick w przeglądarce
pozostają kolejnymi etapami. Schemat v7, migracje i manifest nie zmieniają się.
Lokalnie używamy ośmiu wątków. CI GitHub zachowuje Ubuntu 24.04 i cztery logiczne
CPU na runner z równoległością z nproc. Nie czekamy na zdalny wynik CI.
Nie wykonujemy produkcyjnego wdrożenia ani zmiany kont.
