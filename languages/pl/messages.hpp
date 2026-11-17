#pragma once
#include <array>
#include <string_view>

namespace dc24h::i18n::pl {
inline constexpr std::array<std::string_view, 165> messages{{
    "Wyrzucono z huba",
    "Wyrzucono użytkownika",
    "Odmowa wyrzucenia",
    "Odmowa wyrzucenia; użyj +kick <nick> [powód]",

    "Nieprawidłowe polecenie administracyjne",
    "Odmowa polecenia administracyjnego",
    "Nieaktualna rewizja konta administracyjnego",
    "Nie znaleziono konta administracyjnego",
    "Usługa administracyjna jest niedostępna",
    "Nieznany wynik zapisu administracyjnego",

    "Utworzono właściciela huba",
    "Przekazano własność huba",
    "Nieznany wynik przekazania własności; sprawdź bazę przed ponowieniem",
    "Odmowa operacji właściciela",

    "Niepoprawna konfiguracja uprawnień ról",
    "Niepoprawna konfiguracja uwierzytelniania ADC",
    "Niepoprawne dane poświadczenia ADC",
    "Niepoprawne potwierdzenie hasła konta przy zapisie hasła ADC",
    "Poświadczenie ADC zostało już zapisane",
    "Poświadczenie ADC wymaga powiązania tożsamości konta",
    "Uwierzytelnianie ADC wymaga zgodnego aktualnego schematu bazy",
    "Niepoprawne zapisane poświadczenie lub tożsamość ADC",
    "Błąd operacji kryptograficznej poświadczenia ADC",
    "Uwierzytelnianie ADC nie powiodło się",
    "Włączono uwierzytelnianie kont ADC",
    "Uwierzytelnianie ADC wymaga bazy, prywatnego klucza i zgodnych portów nasłuchu",
    "Klucz uwierzytelniania ADC wymaga oddzielnego prywatnego pliku",
    "Niepoprawny katalog migracji bazy danych",
    "Migracja bazy danych wymaga interwencji operatora",
    "Niezgodny schemat metadanych migracji bazy danych",
    "Wersja schematu bazy danych=",
    "Połączenie z bazą danych nie nadaje się do użycia",
    "Naruszenie ograniczenia bazy danych",
    "Konflikt transakcji bazy danych",
    "Przekroczono limit operacji bazy danych",
    "Niepoprawny stan transakcji bazy danych",
    "Połączenie z bazą danych użyte z innego wątku",

    "Przełączniki transferów muszą mieć wartość on lub off",
    "Czat zdarzenie=",
    "Pominięte zdarzenia logu czatu=",
    "Niepoprawna polityka czatu",
    "Niepoprawna wiadomość powitalna czatu",
    "Niepoprawne limity anti-flood czatu",
    "Użytkownicy zakończeni; online=",
    "Uruchamianie dc24h.eu ",
    "Core działa",
    "Zatrzymywanie Core",
    "Core zatrzymany po błędzie sieci",
    "Core zatrzymany",
    "Odebrano SIGINT; żądanie zatrzymania",
    "Odebrano SIGTERM; żądanie zatrzymania",
    "Sieć nasłuchuje na ",
    "ADCS nasłuchuje na ",
    "Sieć zatrzymana; wszystkie gniazda TCP zamknięte; accepted=",
    "ADC zatrzymany; frames=",
    "ADCS zatrzymany; handshakes=",
    "Limit zasobów TCP accept; ponowienie za sekundę",
    "Limit zasobów TCP epoll; ponowienie za sekundę",
    "Błąd nasłuchu TCP",
    "Nieprawidłowe argumenty: ",
    ". Użyj dc24h --language pl --help.\n",
    "Nieznany język (użyj us lub pl)",
    "Nieznana opcja",
    "Brak wartości opcji",
    "Maksymalnie 16 nasłuchów",
    "Wartość liczbowa poza dozwolonym zakresem",
    "Nasłuch ADCS wymaga certyfikatu TLS i klucza prywatnego; łącznie maksymalnie 16 nasłuchów",
    "ADCS wymaga --tls-cert i --tls-key",
    "Nasłuch ADCS wymaga certyfikatu TLS i klucza prywatnego",
    "Nie można utworzyć kontekstu TLS",
    "Nie można ustawić minimalnej wersji TLS",
    "Nie można wczytać łańcucha certyfikatów TLS",
    "Nie można wczytać klucza prywatnego TLS",
    "Certyfikat TLS i klucz prywatny nie pasują do siebie",
    "Nie można utworzyć połączenia TLS",
    "Nie można przypisać gniazda TLS",
    "Oczekiwano IPv4:port lub [IPv6]:port",
    "Port nasłuchu musi być liczbą całkowitą od 0 do 65535",
    "Adres nasłuchu musi być numerycznym adresem IPv4 lub IPv6",
    "Nawiasów używaj wyłącznie dla nasłuchu IPv6",
    "Konfiguracja poprawna (bez zajmowania portów i otwierania logu)\n",
    "Sprawdzenie konfiguracji nie powiodło się: ",
    "Błąd uruchomienia: ",
    "Nieprawidłowa wartość liczbowa konfiguracji",
    "Poziom logowania musi być info, warning, error lub off",
    "Pusta ścieżka konfiguracji",
    "Niedozwolony znak sterujący w ścieżce",
    "Nie można rozwiązać skonfigurowanej ścieżki",
    "none nie można łączyć z adresami nasłuchu",
    "Nieprawidłowa lista nasłuchów",
    "Nieprawidłowy adres nasłuchu",
    "Powtórzony adres i port nasłuchu",
    "Nie można otworzyć konfiguracji (wymagany zwykły plik bez dowiązania symbolicznego)",
    "Konfiguracja musi być zwykłym plikiem o wielkości do 64 KiB",
    "Błąd odczytu konfiguracji",
    "Plik konfiguracji przekracza 64 KiB",
    "Konfiguracja musi być zapisana jako UTF-8 bez znaków sterujących",
    "Nieprawidłowe limity nasłuchów lub połączeń",
    "ADCS wymaga nasłuchu, certyfikatu i klucza prywatnego",
    "Nieprawidłowy poziom logowania",
    "Log nie może wskazywać plików konfiguracji ani danych dostępowych",
    "Nie można rozwiązać ścieżki pliku konfiguracji",
    "Nieprawidłowy wiersz konfiguracji ",
    "Nieprawidłowa sekcja w wierszu ",
    "Nieznana sekcja konfiguracji w wierszu ",
    "Oczekiwano sekcji i klucz=wartość w wierszu ",
    "Powtórzony klucz konfiguracji w wierszu ",
    "Nieprawidłowy lub nieznany klucz/wartość konfiguracji w wierszu ",
    "Podaj --config tylko raz ze ścieżką pliku",
    "Pomocy/wersji nie można łączyć z opcjami operacyjnymi",
    "Nie można otworzyć skonfigurowanego pliku logu",
    "Log musi wskazywać zwykły plik z jednym dowiązaniem",
    "Ustawienia startowe: adc_listeners=",
    "Połączenie id=",
    "Pominięte zdarzenia połączeń: suppressed=",
    "Odrzucono nieprawidłową ramkę ADC; treść pominięta",
    "Osiągnięto limit połączeń; nadmiarowy klient odrzucony",
    "Nie udało się utworzyć sesji ADCS; klient odrzucony",
    "Błąd negocjacji ADCS; połączenie zamknięte",
    "Błąd komunikacji z klientem; połączenie zamknięte",
    "Przekroczono czas negocjacji ADCS; połączenie zamknięte",
    "Zamknięto bezczynne połączenie",
    "Logowanie połączeń musi być on lub off",
    "Nieprawidłowe limity bezpieczeństwa",
    "Security zatrzymany; rejected=",
    "Osiągnięto limit przyjęć lub oczekujących logowań; klient odrzucony",
    "Osiągnięto limit ramek; połączenie zamknięte",
    "Przekroczono czas niedokończonej ramki; połączenie zamknięte",
    "Nieprawidłowe ustawienia bazy danych",
    "Nie można odczytać prywatnego pliku hasła bazy danych",
    "Baza danych jest wyłączona",
    "Nie udało się zainicjować bazy danych",
    "Nie udało się ustawić opcji bazy danych",
    "Połączenie z bazą danych nie powiodło się",
    "Nie udało się ustawić kodowania bazy danych",
    "Sprawdzenie połączenia z bazą danych nie powiodło się",
    "Wykonanie instrukcji bazy danych nie powiodło się",
    "Nieprawidłowy format wyniku bazy danych",
    "Migracja bazy danych nie powiodła się",
    "Blokada migracji bazy danych jest niedostępna",
    "Niezgodna historia migracji bazy danych",
    "Operacje bazy i sprawdzenie konfiguracji wykluczają się",
    "Zastosowane migracje bazy danych=",
    "Połączenie z bazą danych poprawne\n",
    "Połączono z bazą danych",
    "Nieprawidłowe ustawienia lokalnego API",
    "Nie można odczytać prywatnego pliku tokena API",
    "Token API musi zawierać 64 znaki szesnastkowe",
    "Gniazdo API wymaga prywatnego katalogu właściciela",
    "Ścieżka gniazda API już istnieje lub jest niedostępna",
    "Nie można utworzyć lokalnego gniazda API",
    "Nie można przypisać lokalnego gniazda API",
    "Nie można sprawdzić lokalnego gniazda API",
    "Nie można nasłuchiwać na lokalnym gnieździe API",
    "Błąd wątku lokalnego API",
    "Lokalne API nasłuchuje",
    "Lokalne API można uruchomić tylko raz",
    "Gniazdo API nie może wskazywać plików konfiguracji ani danych dostępowych"
    "Upłynął czas logowania ADC; połączenie zamknięte",
    "Wysyłanie ADC zatrzymane; połączenie zamknięte",
    "Limit negocjacji przekroczony; połączenie zamknięte",
    "Limit weryfikacji tożsamości przekroczony; połączenie zamknięte",
    "Wyczerpano identyfikatory sesji; klient odrzucony",
    "Nieprawidłowe limity użytkowników/sesji",
    "Osiągnięto limit sesji użytkowników; nadmiarowy klient odrzucony",
}};
inline constexpr std::string_view help = R"HELP(Użycie: dc24h [--config PLIK] [--check-config | --check-database | --database-status | --migrate-database]
              [--listen IPv4:port | [IPv6]:port | none] ...
              [--adcs-listen IPv4:port | [IPv6]:port | none] ...
              [--tls-cert ŁAŃCUCH.pem --tls-key KLUCZ.pem]
              [--max-connections N] [--max-sessions N] [--max-users N] [--idle-timeout SEKUNDY]
              [--log-level info|warning|error|off] [--log-file ŚCIEŻKA|-] [--language us|pl]
              [--log-connections on|off] [--log-chat on|off]
             [--max-per-ip N] [--accepts-per-minute N]
             [--frames-per-second N] [--max-invalid-frames N]
             [--tls-handshake-timeout SECONDS] [--partial-frame-timeout SECONDS]
        dc24h [--language us|pl] --help | --version
Kolejność: ustawienia domyślne, opcjonalny plik INI, nadpisania z CLI.
Ścieżki INI są względne do katalogu pliku; ścieżki CLI do katalogu roboczego.
Pierwszy jawny nasłuch usuwa domyślny TCP; listy CLI zastępują dany transport.
ADCS wymaga zgodnego łańcucha PEM i klucza; TLS >=1.2. Łącznie do 16 nasłuchów.
--check-config sprawdza ustawienia/TLS bez zajmowania portów i tworzenia logu.
Domyślnie: 127.0.0.1:1511, 1024 połączenia, 120 sekund bezczynności, info na stderr, us.
Limity: 1..65535 połączeń; 1..86400 sekund bezczynności.
Program działa na pierwszym planie; stop przez SIGINT (Ctrl+C) lub SIGTERM.
Limity użytkowników/sesji: 1..65535, domyślnie 65535; obowiązuje też limit połączeń.
Domyślne limity: 128 klientów/IP, 256 przyjęć/IP/minutę, 1024 ramki/s/klienta,
512 błędnych ramek/klienta, 10 sekund dla TLS i niedokończonej ramki.
Działa logowanie gości ADC, publiczna lista oraz czat publiczny i prywatny.
Flagi MSG: PM określa SID odpowiedzi prywatnej; ME1 oznacza akcję w trzeciej osobie.
Hub wysyła systemowe powitanie IMSG po własnym INF klienta; klienci nie mogą nadawać IMSG.
Czat klientów sprawdza tekst UTF-8/NFC, własność SID źródła i gotowość sesji.
Anti-flood [chat]: 5 wiadomości/s (seria 10), 32768 bajtów tekstu/s (seria 131072).
Nadmiarowe B/D/E są odrzucane; odwracalny komunikat pojawia się najwyżej raz na sekundę.
Czat [chat]: max_text_bytes=16384; public_enabled=on, private_enabled=on,
actions_enabled=on, welcome_enabled=on. Przełączniki przyjmują tylko on/off.
welcome_message to literalny tekst INI UTF-8/NFC (do 4096 bajtów); ustawienia wymagają restartu.
Wyszukiwanie [search]: sprawdzana polityka startowa, limity liczby/bajtów i okien tokenów.
Działa routing SCH i DRES przez hub; UDP między klientami jest poza licznikami huba.
Uwierzytelnione API stanu pokazuje zbiorcze statystyki i efektywne ustawienia wyszukiwania.
Zmiana konfiguracji wymaga restartu; strona Search WebAdmin jest tylko do odczytu.
Transfer Core koordynuje sprawdzone DCTM do celu / ECTM do celu i nadawcy.
CTM wymaga aktywnego źródła TCP; protokół/port/token klienta są zachowane.
RCM kieruje żądanie pasywnego klienta do aktywnego TCP: D do celu / E do celu i nadawcy.
Wspólny Connection Router wybiera odbiorców CTM/RCM według życia gotowego SID, nigdy tokenu.
Możliwości klientów rozdzielają TCP4/TCP6; UDP i TLS huba nie nadają aktywności TCP.
Protokoły klientów: ADC/1.0 oznacza zwykły TCP, ADCS/1.0 wymaga TLS między klientami.
Hub zachowuje jawne żądania i tokeny błędów; TLS klientów jest niezależny od TLS huba.
Klienci zachowują protokół/token odpowiedzi CTM; hub nie podaje za nich portu.
Tokeny połączeń: 1..128 zdekodowanych bajtów NFC UTF-8; zachowane litery, spacje i ukośniki.
Transfery [transfers]: ctm_enabled, rcm_enabled, guest_initiate, guest_receive (on/off, domyślnie on).
allowed_protocols: any (domyślnie), none lub do 16 dokładnych nazw oddzielonych przecinkami.
Zmiany transferów wymagają restartu; API i WebAdmin pokazują efektywną politykę startową.
Limity transferów: request_rate/burst/byte_rate/byte_burst oraz incoming_rate/burst/byte_rate/byte_burst.
CTM/RCM współdzielą budżety nadawcy i odbiorcy; nadmiar jest pomijany, do 1 prywatnego powiadomienia/s.
Uprawnienia inicjowania źródła i odbioru celu są ponownie sprawdzane przed wysyłką.
Błędy STA klientów zachowują protokół/token; kierowany status fatalny nie zamyka sesji huba.
STA używa wspólnych ograniczonych kolejek; błędy pamięci walidacji/kolejki/zamykania są izolowane.
Korelacja oczekujących negocjacji i obsługa danych plików pozostają planowane.
Czat grupowy pozostaje planowany.
--show-configuration pokazuje bezpieczne ustawienia startowe JSON bez odczytu haseł i połączenia SQL.
Ustawienia bazy w sekcji INI [database]; domyślnie wyłączona.
--check-database sprawdza połączenie; --database-status odczytuje wersję/latest/pending/dirty.
--migrate-database stosuje uporządkowany katalog SQL; stan dirty wymaga interwencji operatora.
--check-config nie łączy z MariaDB ani nie odczytuje pliku hasła.
Lokalne API: INI [api], domyślnie wyłączone; gniazdo Unix i prywatny token Bearer.
Tylko GET /api/v1/health i /api/v1/status. Brak operacji zmieniających stan.
Database Core: typowane parametry, ograniczone wyniki i transakcje RAII w wątku właściciela.
Błąd SQL/wyniku wymaga wycofania; utrata połączenia bez automatycznego ponawiania zapisów.
)HELP";
} // namespace dc24h::i18n::pl
