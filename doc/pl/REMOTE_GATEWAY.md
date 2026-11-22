# Zdalny WebAdmin — beta-0.2.12 (dostęp lokalny, tylko odczyt)

**Wersja rozwojowa.** beta-0.2 dodaje, a beta-0.2.2–beta-0.2.12 uszczelniają **osobną** bramę WebAdmin:
/remote.html i pięć ściśle określonych tras /remote/v1. Działa **tylko
lokalnie** na 127.0.0.1 i wymaga HTTPS, aktywnego konta
**administrator** albo **Owner** oraz hasła tego konta. W tej wersji nie
ma **MFA**; udostępnianie bramy w Internecie, przez proxy zewnętrzne albo
tunel **nie jest obsługiwane**. Kompilacja nie uruchamia ani nie wdraża
bramy. Dotychczasowa publiczna konfiguracja Apache pozostaje bez zmian.

## Granice zaufania i uprawnienia

Przeglądarka (lokalny HTTPS) → osobny Apache (127.0.0.1:8443) →
oddzielny proces Node (127.0.0.1:4182) → prywatne gniazdo Unix API C++.
Brama odczytuje token usługi wyłącznie po stronie serwera, z prywatnego
pliku. Nie przyjmuje tokenu procesu, grantu administracyjnego ani
tożsamości z nagłówków przeglądarki. Hasło sprawdza istniejąca trasa
dowodu hasła w C++; brama nie zachowuje hasła w sesji. Grant, sesja
API i CSRF pozostają w pamięci serwera. Przeglądarka dostaje wyłącznie
losowe cookie Secure, HttpOnly, SameSite=Strict dla /remote/v1.

Przed **każdym** odczytem sesji lub statystyk brama ponownie sprawdza
aktywną rolę konta w katalogu zarządzania. Blokada konta, odebranie
uprawnienia, utrata grantu lub awaria API zamyka dostęp; przy błędzie
API zwracane jest 503, bez wcześniejszych danych.
W beta-0.2.3 odpowiedź C++ po sprawdzeniu hasła musi zawierać dokładnie ten
sam identyfikator konta, który podał operator. Inne ID, konto nieaktywne lub
niedozwolona rola skutkują generyczną odmową jak przy błędnym haśle. Odczyt
statusu nie może zakończyć się sukcesem po równoległym unieważnieniu sesji;
spóźniona odpowiedź API nie przywraca uprawnień.

Sesja trwa maksymalnie 10 minut, wygasa po 2 minutach bezczynności. Jednocześnie maksymalnie
4 sesje. Logowanie wymaga jednorazowego wyzwania CSRF; próby są
limitowane. Wylogowanie wymaga osobnego CSRF i zgodnego Origin.
Poprawka `beta-0.2.3-fix` sprawdza czas ważności wyzwania **po** odebraniu
całej treści żądania logowania oraz ponownie sprawdza ważność sesji po
asynchronicznej autoryzacji i po pobraniu statystyk. Żądanie rozpoczęte
przed upływem terminu nie może zakończyć się sukcesem po jego przekroczeniu.
Pięć tras, numer wersji i uprawnienia tylko do odczytu pozostają bez zmian.
Audyt zapisuje jedynie ustalone nazwy zdarzeń, bez haseł, tokenów,
identyfikatorów sesji, adresów IP i treści odpowiedzi.

### beta-0.2.7–beta-0.2.12 — zbiorcze poprawki High Starter

Widoczna nazwa to **dc24h.eu High Starter**; wewnętrzna tożsamość protokołu
prywatnego API pozostaje `dc24h.eu`. To jedna gałąź i jedna końcowa wersja
`beta-0.2.12`, nie sześć osobno opublikowanych aktualizacji.

Odpowiedź prywatnego API przez gniazdo Unix musi mieć dokładny typ JSON
(dopuszczalne parametry Content-Type). Całkowity czas żądania wynosi cztery
sekundy także przy nieprzerwanym strumieniu, a zgromadzone prywatne bufory
są zerowane po sukcesie, przerwaniu, przekroczeniu limitu lub ucięciu.
Błąd odczytu oznacza 503 bez zapamiętanych metryk. Testy bez gniazd obejmują
usunięcie sesji w trakcie statusu oraz 12 równoległych odczytów z cofnięciem
grantu: nie ma wycieku metryk, a ponowne logowanie działa po odzyskaniu
uprawnień.

Przykładowy vhost Apache TLS odrzuca również Fetch Metadata cross-site
(`Sec-Fetch-Site: cross-site` lub `same-site`) przed Node. Brak nagłówka,
`same-origin` lub `none` pozostają dozwolone dla lokalnych klientów;
Node niezależnie stosuje te same ograniczenia. Testy obejmują odizolowany
serwer Apache HTTPS. Pięć tras pozostaje lokalnych i tylko do odczytu.

### beta-0.2.6 — minimalna projekcja statusu i awarie API

Brama Node korzysta teraz z osobnego parsera `parseRemoteSummary` zamiast
pełnego lokalnego `parseSnapshot`. Przeglądarka dostaje wyłącznie nowy obiekt
z listy zatwierdzonej: wersja/działanie/czas huba, połączenia aktywne i
zaakceptowane, liczba użytkowników online i szczytowa, zbiorcze liczniki
odebranego/odrzuconego czatu oraz opcjonalnie po dwa liczniki wyszukiwania
i transferów. ID kont, nicki, CID/IP, wiadomości, konfiguracja, wiersze
audytu, granty, sekrety, ścieżki, monitoring ani surowy JSON nie trafiają
do projekcji. Nieznane pola i uszkodzone *nieużywane* sekcje API są pomijane.

Schemat `v1`, tożsamość i wersja serwera oraz typy wymaganych pól muszą być
poprawne; publikowane liczniki to nieujemne bezpieczne liczby całkowite.
Braki lub błędy wymaganych agregatów i obecnych opcjonalnych liczników
publicznych skutkują ogólną odmową 503 bez poprzednich danych. Tak samo
działają awarie gniazda/API/MariaDB i timeouty prywatnego API. Testy
deterministyczne obejmują dodane sekrety, zmianę formatu, błędne liczby,
brak pól i powrót po awarii. Brak nowych tras, uprawnień i zmian w
dotychczasowym parserze lokalnego WebAdmin.

Dodatkowa poprawka wyścigu sprawdza aktualne uprawnienia **ponownie po**
zakończeniu pobierania statusu, a przed wysłaniem danych ponownie weryfikuje
tożsamość i czas ważności sesji przeglądarki. Odebranie grantu w trakcie
odczytu daje 403 bez statystyk; awaria ponownej kontroli daje 503 bez
danych z pamięci. Oba scenariusze mają testy regresyjne. Brama czyści
również scalony bufor JSON po parsowaniu logowania, także przy błędnym JSON,
oprócz wcześniej czyszczonych fragmentów strumienia. Ciągi znaków JavaScript
podlegają normalnemu odśmiecaniu pamięci przez środowisko uruchomieniowe.

### beta-0.2.5 — granica Apache2, HTTPS i loopback

Osobny przykładowy vhost nasłuchuje **wyłącznie na 127.0.0.1:8443**,
wykorzystuje TLS 1.2/1.3 oraz lokalnie zaufany certyfikat.
`DC24H_REMOTE_ORIGIN` musi odpowiadać `https://localhost:8443`.
Moduł `rewrite` Apache odrzuca niepoprawny Host przeglądarki, query string
na trasach API oraz niewłaściwą metodę GET/POST przed przekazaniem do Node.
Niepusty Origin musi dokładnie odpowiadać adresowi HTTPS, a POST logowania
i wylogowania musi go zawierać. Node nadal niezależnie sprawdza trasy,
metody, długość i format żądania, CSRF, konto oraz uprawnienia. Jedynie
pięć ścisłych tras `/remote/v1/*` może trafić do lokalnego serwera Node.

Vhost współdzieli katalog z kompilacją Vite, ale blokuje zwykły
`index.html` panelu administracyjnego i jego pliki JS/CSS `local-*`.
Ścieżki `/` i `/remote.html` pokazują osobny portal; zasoby `remote-*`
pozostają dostępne. `LimitRequestBody 2048` uzupełnia jawna reguła rewrite:
żądania POST do bramy wymagają liczbowego `Content-Length` w granicy 2048
bajtów i nie mogą używać `Transfer-Encoding`. Jest to konieczne, ponieważ
strumieniowanie w proxy może ominąć ogólny limit treści. Trzy trasy GET
również odrzucają `Transfer-Encoding` i niezerową długość
treści przed przekazaniem do Node. Testy na jednorazowym Apache HTTPS
obejmują nieprawidłowy Host, metody, query,
Origin, dostęp do plików lokalnych i sekretów, 2048/2049 bajtów,
treść GET, chunked oraz próbę zwykłego HTTP; odmowy nie docierają do backendu.
Testy statyczne sprawdzają nasłuch loopback i izolację usługi systemd.

To dodatkowa ochrona, **nie** konfiguracja do Internetu: tunel/proxy
zewnętrzne dodane przez operatora lub lokalny napastnik z dostępem do
sekretów pozostają zagrożeniem. Publiczny vhost jest niezmieniony, a
przykład nie jest automatycznie instalowany, uruchamiany ani wdrażany.

Dodatkowa ochrona sesji blokuje ponowne logowanie ze starym cookie, dopóki
wylogowanie z prawidłowym CSRF czeka na zakończenie odbierania treści żądania.
Równoległa próba wylogowania z nieprawidłową treścią nie usuwa tej blokady.
Wcześniej opóźnione wylogowanie mogło zgłosić sukces, choć nowa sesja
pozostawała aktywna. Deterministyczny test bez gniazd odtwarza ten wyścig.

### beta-0.2.4 — sesje i weryfikacja CSRF

Czas bezwzględny sesji (10 minut) i bezczynności (2 minuty) jest teraz
liczony dopiero po zakończeniu sprawdzenia hasła i uprawnień w prywatnym API.
Opóźniona autoryzacja nie zwraca poprawnego logowania z już wygasłym cookie.
Udane ponowne logowanie z cookie poprzedniej sesji tej przeglądarki unieważnia
poprzednią sesję po stronie serwera i wydaje nowy losowy identyfikator oraz
CSRF; nieudana próba nie usuwa poprawnej sesji. Inne przeglądarki mają osobne
sesje (maksymalnie cztery, piąta usuwa najstarszą). Restart procesu usuwa
wszystkie sesje i wyzwania z pamięci.

Cookie nadal ma `Secure; HttpOnly; SameSite=Strict; Path=/remote/v1`.
Logowanie wymaga niezależnego, jednorazowego wyzwania ważnego 2 minuty,
powiązanego z cookie inicjalizacyjnym. Wylogowanie wymaga zgodnego Origin
HTTPS i aktualnego CSRF sesji; poprzednie cookie nie może być użyte ponownie.
Testy bez gniazd sprawdzają rotację, izolację przeglądarek, równoległe
wyzwania, limit sesji, oba czasy wygaśnięcia, zły Origin/CSRF,
wylogowanie w trakcie odczytu i restart. Playwright potwierdza usunięcie
wcześniej wyświetlonych metryk po wygaśnięciu. Token usługi, grant, sesja
i CSRF prywatnego API nie trafiają do localStorage, URL ani przeglądarki;
jedynie CSRF przeglądarki potrzebny do wylogowania jest zwracany w
autoryzowanym JSON. Nie dodano tras ani możliwości zapisu.

Dodatkowa ochrona wyścigów wymaga, aby poprzednia sesja przeglądarki nadal
była aktywna w chwili zakończenia równoległego ponownego logowania. Jeśli
wcześniej wylogowano użytkownika albo inna próba zastąpiła sesję, opóźnione
logowanie jest odrzucane bez tworzenia osieroconego cookie. Interfejs
odrzuca również spóźnione odpowiedzi ze starego stanu autoryzacji:
poprzedni odczyt nie może nadpisać nowych statystyk ani unieważnić nowego
logowania. Wylogowanie od razu ukrywa metryki, a błędne pobranie wyzwania
czyści wpisane hasło. Scenariusze te mają testy regresyjne.

### Dozwolone trasy

| Trasa | Metoda | Działanie |
| --- | --- | --- |
| /remote/v1/challenge | GET | Krótkie wyzwanie CSRF, bez danych huba |
| /remote/v1/login | POST | Weryfikacja hasła i roli |
| /remote/v1/session | GET | Powtórna kontrola sesji i uprawnień |
| /remote/v1/summary | GET | Wybrane zbiorcze statystyki, tylko odczyt |
| /remote/v1/logout | POST | Unieważnienie sesji po kontroli CSRF |

### Walidacja żądań w beta-0.2.2

Brama porównuje oryginalną ścieżkę żądania z pięcioma dokładnymi nazwami
i metodami. Dodatkowe segmenty, podwójne ukośniki, alternatywne kodowanie
procentowe, query string, zmiana wielkości liter i niewłaściwe metody są
odrzucane. GET nie przyjmuje body. POST wymaga ograniczonego JSON z poprawnym
Content-Length i bez Transfer-Encoding; niepoprawny JSON i nadmierny rozmiar
oznaczają odmowę. Nagłówki Authorization, Proxy-Authorization, Forwarded,
wszystkie `X-Forwarded-*`, X-Real-IP, X-Original-URL i X-Rewrite-URL
są odrzucane w procesie Node przed połączeniem z prywatnym API. Apache
dodatkowo usuwa popularne sfałszowane nagłówki. Błędy zwracają generyczne
kody, bez tokenów, haseł i treści prywatnego API. Pięć poprawnych tras
pozostaje zgodnych z dotychczasowym kontraktem.

Poprawka na gałęzi `beta-0.2.2-fix` zużywa wydane wyzwanie logowania także
wtedy, gdy POST zawiera błędny JSON, długość lub typ body, oraz czyści cookie
inicjalizacyjne. Bufory odebranej treści są zerowane również po przekroczeniu
limitu i błędzie/przerwaniu strumienia. Odrzuconego dowodu nie można ponownie
użyć z tym samym wyzwaniem. Kontrakt pięciu tras pozostaje bez zmian.

Osobny vhost Apache ustawia `MergeSlashes Off` (Apache 2.4.39+), aby podwójne
ukośniki nie były po cichu normalizowane do dozwolonej ścieżki proxy.
Izolowany test integracyjny Apache sprawdza tę granicę niezależnie
od walidacji żądań w Node.

Statystyki obejmują tylko wersję, stan działania huba, czas pracy
oraz zbiorcze liczniki połączeń, użytkowników i czatu; opcjonalnie
liczniki wyszukiwania i transferów. Nie ma list kont, nazw użytkowników,
IP/CID, danych poufnych, INI, audytu, surowych odpowiedzi API ani
poleceń administracyjnych. **Brak operacji zapisu**: konta, role, bany,
konfiguracja, Start/Stop/Restart, systemd, SQL i migracje pozostają
poza bramą. Logowanie tutaj nie otwiera starego lokalnego WebAdmin.

## Instalacja opcjonalna (bez automatycznego uruchamiania)

1. W katalogu webadmin/ wykonaj npm ci oraz npm run build (Node.js
   zgodny z webadmin/README.md). Wynik zawiera oddzielny remote.html.
2. Ustaw prywatne zmienne serwerowe DC24H_API_SOCKET i
   DC24H_API_TOKEN_FILE dla istniejącego API C++. Użytkownik bramy
   musi mieć dostęp do gniazda i pliku tokenu. Nie umieszczaj tych
   danych w katalogu WWW, nagłówkach przeglądarki, Vite ani Git.
3. Ustaw DC24H_REMOTE_ORIGIN=https://localhost:8443 i uruchom
   npm run remote:local w webadmin/. Proces nasłuchuje wyłącznie
   127.0.0.1:4182; próby ustawienia adresu publicznego są odrzucane.
   Przykład config/dc24h-remote-gateway.service.example nie jest
   instalowany jako działająca usługa.
4. Skonfiguruj **lokalny** certyfikat HTTPS i osobny vhost z pliku
   config/apache2-webadmin-remote.conf.example. Potrzebne są moduły
   ssl, headers, proxy i proxy_http. Sprawdź zgodność Origin oraz
   konfigurację Apache przed uruchomieniem.
5. Na tym samym komputerze otwórz https://localhost:8443/remote.html,
   zaloguj się numerycznym ID i hasłem aktywnego administratora albo
   Ownera. Dostępne są języki EN/PL. Wylogowanie unieważnia sesję.

Nie wystawiaj portu przez publiczny interfejs lub tunel jako obejście
błędu połączenia. Błędna konfiguracja HTTPS lub utrata lokalnego API
powinna skutkować odmową dostępu.

## Testy i ryzyka

Testy Node sprawdzają dowód hasła i role, w beta-0.2.3 również różne ID,
cofnięcie grantu/roli, awarie DB/API, wyścigi odczytów i limity prób
(deterministycznie bez otwierania gniazd sieciowych). Sprawdzają też CSRF, wygaśnięcie sesji,
błędne metody, kodowane ścieżki, długości i nagłówki oraz brak odczytu API
przy odmowie. Playwright sprawdza EN/PL,
logowanie, wylogowanie i stany błędów. Test integracyjny Apache
uruchamia tymczasowy lokalny serwer HTTPS oraz weryfikuje listę tras,
ukrycie plików i nagłówków poświadczeń. CTest sprawdza też
konfigurację i niezmienioną listę migracji.

Pozostałe ryzyka: lokalne procesy z dostępem do gniazda lub tokenu,
przejęcie serwera albo przeglądarki, kradzież hasła oraz zarządzanie
lokalnym zaufaniem TLS. Limity logowania są **lokalne dla procesu**
i zerują się po jego restarcie. **Brak MFA i publicznego trybu dostępu.**
To nie jest wdrożenie produkcyjne. Schemat v11 i numer pakietu 0.1.0
nie ulegają zmianie.
