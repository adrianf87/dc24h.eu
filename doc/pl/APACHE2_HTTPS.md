# Przykład HTTPS Apache2 — alpha-0.1.11

[Przykład HTTPS](../../config/apache2-webadmin-https.conf.example) serwuje statyczny build WebAdmin przez Apache2. Dodaje przekierowanie HTTP→HTTPS, TLS 1.2/1.3, osobne listy szyfrów, ścieżki certyfikatu/klucza i nagłówki ochronne. Jest szablonem konfiguracji: wydanie nie aktywuje witryny, nie uzyskuje/odnawia certyfikatu, nie zmienia DNS/firewalla i nie przeładowuje usługi hosta.

Listenery C++ ADC/ADCS i prywatne Unix API pozostają niezależne. Przykład nie przekazuje żądań API i nie zapewnia logowania/ról WWW. Domyślnie statyczny panel jest dostępny lokalnie (`Require local`), a `/admin-api` i `/api` są blokowane. Produkcyjna brama i polityka zdalnego dostępu administratorów pozostają osobnymi zadaniami. Adapter deweloperski Vite nie jest częścią Apache ani builda produkcyjnego.

## Pliki i wymagania

- `config/apache2-webadmin-https.conf.example`: przekierowanie HTTP na porcie 80 i HTTPS na 443, z przykładową nazwą `dc24h.eu`.
- `config/apache2-webadmin.conf.example`: dotychczasowy lokalny podgląd HTTP na 8080, osobny przykład.
- `webadmin/dist/`: gotowe zasoby; buduj wspieranym Node/npm zgodnie z [WEBADMIN.md](WEBADMIN.md).
- Apache2 >=2.4.42 z OpenSSL >=1.1.1, mod_ssl, mod_headers, mod_alias, mod_dir, mod_mime i modułami autoryzacji. Standardowe pakiety Debian/Ubuntu zawierają te moduły.

Dyrektywy Listen należą do Debian/Ubuntu `ports.conf`. Nowy plik celowo nie powtarza Listen 80/443. Włączenie mod_ssl zwykle udostępnia warunkowy nasłuch HTTPS dystrybucji; sprawdź rzeczywistą konfigurację portów. Nie kopiuj efemerycznych nasłuchów harnessu testowego do witryny produkcyjnej.

Domyślny katalog zasobów: `/usr/local/share/dc24h/webadmin`. Certyfikat/klucz: `/etc/dc24h/webadmin-tls/fullchain.pem` i `/etc/dc24h/webadmin-tls/privkey.pem`. Dostosuj wszystkie wystąpienia domeny i ścieżek. Pełny łańcuch powinien zawierać certyfikat serwera, potem certyfikaty pośrednie; nazwy muszą obejmować wybranego hosta. Klucz trzymaj poza DocumentRoot/Git, czytelny wyłącznie dla odpowiedniej tożsamości startowej Apache (zwykle root, tryb 0600); workery nie potrzebują bezpośredniego odczytu pliku klucza. Użyj certyfikatu od swojego dostawcy lub istniejącego systemu odnawiania. Nie dodano klienta CA, obsługi challenge ani zadania odnawiania.

## Budowanie i instalacja

```sh
npm --prefix webadmin ci
npm --prefix webadmin run build
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DDC24H_INSTALL_WEBADMIN=ON
cmake --build build-release --parallel 2
sudo cmake --install build-release --prefix /usr/local
```

CMake zawsze instaluje przykład HTTPS w `etc/dc24h`, obok INI. `DC24H_INSTALL_WEBADMIN=ON` dodatkowo instaluje gotowe zasoby i dotychczasowy przykład loopback. Domyślne budowanie C++ nadal nie wymaga Node ani Apache. Instalacja jedynie kopiuje pliki; niczego nie aktywuje.

## Dostosowanie i włączenie na Debian/Ubuntu

Najpierw przygotuj właściwy certyfikat/klucz i zasoby, następnie skopiuj/edytuj szablon. To polecenia operatora, nie czynności wykonane przez to wydanie:

```sh
sudo a2enmod ssl headers alias
sudo install -m 0644 /usr/local/etc/dc24h/apache2-webadmin-https.conf.example \
  /etc/apache2/sites-available/dc24h-webadmin-https.conf
sudoedit /etc/apache2/sites-available/dc24h-webadmin-https.conf
```

Zachowaj `Require local` do wybrania jawnej polityki dostępu administratorów. Późniejsze dopuszczenie zaufanej sieci wymaga świadomej zmiany Directory; nie dodaje to sesji/ról ani bramy API. Sama statyczna aplikacja nie zawiera tokena ani danych na żywo.

Sprawdź tożsamość/ważność certyfikatu i zgodność kluczy publicznych. Polecenia wypisują wyłącznie metadane certyfikatu i skróty kluczy publicznych:

```sh
openssl x509 -in /etc/dc24h/webadmin-tls/fullchain.pem -noout -subject -issuer -dates
openssl x509 -in /etc/dc24h/webadmin-tls/fullchain.pem -pubkey -noout |
  openssl pkey -pubin -outform DER | openssl dgst -sha256
sudo openssl pkey -in /etc/dc24h/webadmin-tls/privkey.pem -pubout -outform DER |
  openssl dgst -sha256
```

Oba skróty muszą być zgodne. `apache2ctl configtest` sprawdza składnię i brakujące pliki, ale samo nie gwarantuje zgodności certyfikatu z kluczem; regresja sprawdza również rzeczywisty start z błędną parą. Po przeglądzie certyfikatu/klucza i dostosowanej konfiguracji:

```sh
sudo a2ensite dc24h-webadmin-https
sudo apache2ctl configtest && sudo systemctl reload apache2
```

Dla zaufanego certyfikatu CA obejmującego `dc24h.eu` lokalnie można użyć `curl --resolve dc24h.eu:443:127.0.0.1 https://dc24h.eu/`; zachowana jest weryfikacja nazwy/łańcucha, bez wyłączania TLS. Po odnowieniu/przeładowaniu sprawdź faktycznie serwowany certyfikat. Szablon nie weryfikuje automatycznie zewnętrznego DNS, łańcucha CA, ważności ani polityki innych vhostów.

## Zachowanie i zakres

Vhost HTTP daje stałe 301 na `https://dc24h.eu/`, zachowując ścieżkę/query. Cel nie pochodzi z nagłówka Host klienta. Vhost HTTPS wybiera TLS 1.2/1.3 i szyfry AEAD, wyłącza kompresję SSL oraz ustawia `SSLSessionTickets off`. Blokowane są prywatne API, ukryte ścieżki, typowe rozszerzenia sekretów/konfiguracji/logów/SQL, indeksy katalogów i podążanie za symlinkami. Serwuj wyłącznie katalog builda, nie repozytorium.

CSP dopuszcza zasoby/skrypty/style/fonty/połączenia tego samego origin i zabrania ramek/obiektów. Dodatkowe nagłówki to no-sniff, no-referrer i DENY. Odpowiedzi HTTPS, także błędy, mają host-only HSTS `max-age=300`; brak includeSubDomains/preload. Operator może zmienić krótki czas przykładu po weryfikacji własnego wdrożenia. Nagłówki nie implementują uwierzytelniania API i nie są audytem bezpieczeństwa.

Istniejące vhosty 443/default/SNI i globalne ustawienia Apache mogą wpływać na wdrożenie; sprawdź wybraną witrynę zamiast zastępować inne konfiguracje. Przykład nie konfiguruje HTTP/2, OCSP stapling, automatycznego odnawiania, produkcyjnego audytu i reverse proxy.

## Automatyczna weryfikacja

```sh
ctest --test-dir build -R '^apache2.https$' --output-on-failure
```

CMake dodaje test przy dostępnym Apache2/mod_ssl, a ich brak jawnie raportuje. CI instaluje Apache2. Python tworzy osobny tymczasowy ServerRoot, lokalne efemeryczne porty, jednorazowy certyfikat/klucz, zasoby/log/cache/PID i workery z obniżonymi uprawnieniami. Nie ładuje/zmienia witryn hosta, magazynu zaufania ani portów uprzywilejowanych. Podmieniane są jedynie ścieżki/porty szablonu; polityka TLS/dostępu/nagłówków pozostaje ta sama.

Siedem przypadków sprawdza zaufane TLS 1.2/1.3 i zasoby, przekierowania kanoniczne/query, HSTS wyłącznie na HTTPS i nagłówki, blokady API/ukrytych ścieżek/sekretów/indeksów/symlinków, odrzucenie obcej nazwy/niezaufanego certyfikatu, starego TLS, brakujących plików i rzeczywistego startu z błędnym kluczem. Nie jest to zewnętrzny skan ani dowód zachowania wszystkich dystrybucji. Zakres wyników: [TESTING.md](TESTING.md).

Źródła: [Apache mod_ssl](https://httpd.apache.org/docs/2.4/mod/mod_ssl.html), [instrukcja TLS](https://httpd.apache.org/docs/2.4/ssl/ssl_howto.html), [przekierowania kanoniczne](https://httpd.apache.org/docs/2.4/rewrite/remapping.html).
