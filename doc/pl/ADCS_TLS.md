# ADCS / TLS — alpha-0.1.4

Serwer używa OpenSSL, oddzielnego kontekstu TLS oraz sesji dla każdego połączenia. Handshake i odbiór są nieblokujące, zintegrowane z `epoll`. Można uruchomić jednocześnie port zwykłego ADC i oddzielny port ADCS. Odszyfrowane ramki przechodzą przez dotychczasowy parser ADC.

Minimalna wersja to TLS 1.2; TLS 1.3 działa, jeśli obsługuje ją OpenSSL. Kompresja TLS i renegocjacja są wyłączone. Certyfikat serwera powinien być prawidłowy i sprawdzany przez klienta. Serwer na tym etapie nie wymaga certyfikatu klienta i nie obsługuje logowania użytkowników.

```sh
./build/dc24h --listen 127.0.0.1:1511 \\
  --adcs-listen 0.0.0.0:1512 \\
  --tls-cert /etc/dc24h/tls/fullchain.pem \\
  --tls-key /etc/dc24h/tls/privkey.pem
```

Klucz prywatny musi znajdować się poza GitHub i być chroniony uprawnieniami systemowymi. Certyfikat i klucz muszą pasować do siebie, inaczej start zakończy się błędem. Serwer nie przeładowuje jeszcze certyfikatów bez restartu. Niepoprawny klient TLS nie zatrzymuje serwera. Limit handshake'u to maksymalnie 10 sekund. Brak odpowiedzi aplikacyjnych ADC, SID, sesji, routingu i zamknięcia `close_notify` po stronie serwera. To wydanie rozwojowe, nieprodukcyjne.
