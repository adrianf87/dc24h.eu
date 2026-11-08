# Sieć i proces

## Alpha-0.1.2 — transport TCP

`network::TcpServer` obsługuje nieblokujące gniazda IPv4/IPv6 w pojedynczym
wątku pętli `epoll` (level-triggered). Nie tworzy wątku na klienta.
Domyślny nasłuch: **127.0.0.1:1511**, limit 1024 aktywnych połączeń,
timeout bezczynności 120 sekund. Zmiana adresu i portu wymaga jawnej opcji CLI.

```sh
./build/dc24h --listen 127.0.0.1:1511 --listen '[::1]:1512' \
  --max-connections 512 --idle-timeout 60
./build/dc24h --listen 127.0.0.1:0
```

Można wskazać do 16 listenerów. Pierwsze `--listen` zastępuje domyślny
nasłuch, kolejne dodają adresy. Port 0 wybiera wolny port systemowy, podany
w logu `Network listening on ...`. Komunikaty gotowości pojawiają się dopiero
po poprawnym uruchomieniu wszystkich listenerów. `Core running` oznacza start
silnika, a nie gotowość portów. Nie są obsługiwane nazwy DNS ani adresy IPv6
z identyfikatorem strefy. IPv6 używa `IPV6_V6ONLY`; IPv4 konfiguruje się osobno.

**To jeszcze nie hub ADC.** Serwer odbiera i zlicza bajty, ale odrzuca ich treść.
Nie odpowiada, nie wykonuje echo, nie parsuje komend, nie uwierzytelnia i nie
przekazuje wiadomości. Nie ma TLS/ADCS ani buforów wyjściowych. Podłączenie
klienta Direct Connect nie zakończy się logowaniem do huba.

## Połączenia, limity i błędy

Każde gniazdo ma właściciela RAII, `SOCK_NONBLOCK` i `SOCK_CLOEXEC`.
Nadmiarowe połączenie jest zamykane, gdy pętla je przyjmie; systemowy handshake
TCP może wcześniej zakończyć się sukcesem. `accepted` liczy klientów dopuszczonych
do pętli, a `rejected` klientów odrzuconych przez limit lub brak zasobów watch.
Limit jest wspólny dla wszystkich portów; nie obejmuje kolejki kernela.

Ostatni poprawny odczyt odświeża timeout klienta. Zamknięcie przez peer, FIN
oraz błędy odczytu/reset zamykają tylko dane połączenie i zwalniają jego slot.
Kontrola bezczynności następuje w pętli z oczekiwaniem do 250 ms, więc timeout
może przekroczyć ustawioną wartość o ten interwał i opóźnienie planisty.

Jedna partia przyjmowania obejmuje do 64 klientów, a odczyt jednego zdarzenia
do 64 KiB. Stały bufor odczytu ma 8 KiB; dane nie są kumulowane w pamięci.
Ograniczenie partii umożliwia obsługę pozostałych klientów oraz stopu przy ciągłym
ruchu. Nie jest to ochrona przed DDoS ani limit przepustowości/per-IP.

Przejściowe błędy `accept4` są ponawiane. Po wyczerpaniu deskryptorów/pamięci
lub limitu watch `epoll` nasłuchy są wyłączane z `epoll` na sekundę, aby uniknąć
aktywnej pętli i zalewu logów; istniejące połączenia i stop pozostają obsługiwane.
Zajęty port i błędny start zwracają kod 1, zwalniając także wcześniej otwarte
listenery. Nieodwracalny błąd listenera/reactora kończy proces z kodem 1.
Limit aplikacji nie podnosi systemowego `RLIMIT_NOFILE` ani limitów kernela.

## Zatrzymanie

SIGINT/SIGTERM wywołują `Core::request_stop()`, a następnie stop Network.
Atomowa flaga jest zapamiętywana również przed startem, a zapis do `eventfd`
budzi `epoll_wait` bez oczekiwania na timeout klienta. Gniazda są zamykane
wyłącznie w wątku pętli. Stop nie oczekuje na peer i nie opróżnia wiadomości
aplikacyjnych — takie kolejki jeszcze nie istnieją. Log zatrzymania zawiera
liczniki przyjętych/odrzuconych połączeń i odebranych bajtów, bez treści danych.

| Sygnał | Obsługa |
| --- | --- |
| SIGINT (Ctrl+C) | Kontrolowany stop, kod 0 |
| SIGTERM | Kontrolowany stop, kod 0 |
| Powtarzane SIGINT/SIGTERM | Brak ponownego sprzątania i eskalacji |
| SIGHUP | Brak własnej obsługi; nie służy do reload |
| SIGKILL/SIGSTOP | Nie są przechwytywane |

Interfejsy systemowe: [epoll(7)](https://man7.org/linux/man-pages/man7/epoll.7.html),
[accept(2)/accept4](https://man7.org/linux/man-pages/man2/accept.2.html),
[eventfd(2)](https://man7.org/linux/man-pages/man2/eventfd.2.html).
