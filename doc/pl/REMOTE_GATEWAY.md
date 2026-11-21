# Zdalny WebAdmin — beta-0.2.2 (dostęp lokalny, tylko odczyt)

**Wersja rozwojowa.** beta-0.2 dodaje, a beta-0.2.2 uszczelnia **osobną** bramę WebAdmin:
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
API zwracane jest 503, bez wcześniejszych danych. Sesja trwa maksymalnie
10 minut, wygasa po 2 minutach bezczynności. Jednocześnie maksymalnie
4 sesje. Logowanie wymaga jednorazowego wyzwania CSRF; próby są
limitowane. Wylogowanie wymaga osobnego CSRF i zgodnego Origin.
Audyt zapisuje jedynie ustalone nazwy zdarzeń, bez haseł, tokenów,
identyfikatorów sesji, adresów IP i treści odpowiedzi.

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

Testy Node sprawdzają dowód hasła i role, CSRF, wygaśnięcie sesji,
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
