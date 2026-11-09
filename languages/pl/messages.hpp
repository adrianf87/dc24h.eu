#pragma once
#include <array>
#include <string_view>

namespace dc24h::i18n::pl {
inline constexpr std::array<std::string_view, 36> messages{{
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
    "Nawiasów używaj wyłącznie dla nasłuchu IPv6"
}};
inline constexpr std::string_view help = R"HELP(Użycie: dc24h [--listen IPv4:port | [IPv6]:port] ...
              [--adcs-listen IPv4:port | [IPv6]:port] ...
              [--tls-cert ŁAŃCUCH.pem --tls-key KLUCZ.pem]
              [--max-connections N] [--idle-timeout SEKUNDY] [--language us|pl]
        dc24h [--language us|pl] --help | --version
Uruchom dc24h.eu na pierwszym planie. Zatrzymaj przez SIGINT (Ctrl+C) lub SIGTERM.
Domyślnie: --listen 127.0.0.1:1511 --max-connections 1024 --idle-timeout 120
Powtarzaj --listen lub --adcs-listen, łącznie do 16 portów; port 0 wybiera system.
Jawne nasłuchy zastępują domyślny; samo --adcs-listen włącza wyłącznie TLS.
ADCS wymaga łańcucha PEM i pasującego klucza prywatnego PEM. TLS >= 1.2.
Limity: 1..65535 połączeń; 1..86400 sekund bezczynności. Język domyślny: us.
Tylko parsowanie składni ADC: brak negocjacji sesji ADC, routingu i logowania.
)HELP";
} // namespace dc24h::i18n::pl
