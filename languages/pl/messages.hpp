#pragma once
#include <array>
#include <string_view>

namespace dc24h::i18n::pl {
inline constexpr std::array<std::string_view, 79> messages{{
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
    "Log nie może wskazywać pliku konfiguracji, certyfikatu TLS ani klucza prywatnego",
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
    "Logowanie połączeń musi być on lub off"
}};
inline constexpr std::string_view help = R"HELP(Użycie: dc24h [--config PLIK] [--check-config]
              [--listen IPv4:port | [IPv6]:port | none] ...
              [--adcs-listen IPv4:port | [IPv6]:port | none] ...
              [--tls-cert ŁAŃCUCH.pem --tls-key KLUCZ.pem]
              [--max-connections N] [--idle-timeout SEKUNDY]
              [--log-level info|warning|error|off] [--log-file ŚCIEŻKA|-] [--language us|pl]
              [--log-connections on|off]
        dc24h [--language us|pl] --help | --version
Kolejność: ustawienia domyślne, opcjonalny plik INI, nadpisania z CLI.
Ścieżki INI są względne do katalogu pliku; ścieżki CLI do katalogu roboczego.
Pierwszy jawny nasłuch usuwa domyślny TCP; listy CLI zastępują dany transport.
ADCS wymaga zgodnego łańcucha PEM i klucza; TLS >=1.2. Łącznie do 16 nasłuchów.
--check-config sprawdza ustawienia/TLS bez zajmowania portów i tworzenia logu.
Domyślnie: 127.0.0.1:1511, 1024 połączenia, 120 sekund bezczynności, info na stderr, us.
Limity: 1..65535 połączeń; 1..86400 sekund bezczynności.
Program działa na pierwszym planie; stop przez SIGINT (Ctrl+C) lub SIGTERM.
Tylko parser ADC: brak pełnego logowania ADC, routingu i sesji użytkowników.
)HELP";
} // namespace dc24h::i18n::pl
