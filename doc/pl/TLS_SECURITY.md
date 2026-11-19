# Bezpieczeństwo TLS — alpha-0.9.5

Polityka certyfikatu i szyfrów ADCS. Gałąź powstaje z main `9d66f47`. Nazwa gałęzi to `alpha-0.9.5`.

Hub już wymagał TLS 1.2 lub nowszego, odrzucał kompresję i renegocjację oraz sprawdzał, że łańcuch PEM pasuje do niezaszyfrowanego klucza prywatnego. Ten etap dodaje stałą politykę liścia i listę szyfrów AEAD, zanim otworzy się jakikolwiek nasłuch. Nie jest to opcja ani klucz INI.

## Certyfikat liścia

Uruchomienie raz czyta zegar systemu. Liść musi być już ważny: notBefore jest równy teraz albo wcześniejszy, a notAfter jest późniejszy niż teraz. Nie ma okresu karencji. Certyfikat, który wygaśnie w trakcie pracy procesu, zostaje do restartu. To nie jest automatyczne odnawianie.

Podpis musi być RSA albo ECDSA z SHA-256, SHA-384 lub SHA-512, albo Ed25519 lub Ed448. Klucz publiczny musi mieć co najmniej 2048 bitów RSA, krzywą NIST P-256, P-384 lub P-521, albo Ed25519 lub Ed448. Mniejszy klucz RSA, inna krzywa albo podpis SHA-1 są odrzucane.

Jeśli liść ma rozszerzenie key usage, musi dopuszczać digitalSignature, keyEncipherment albo keyAgreement. Jeśli ma rozszerzenie extended key usage, musi ono zawierać serverAuth. Brak rozszerzenia nie jest dowodem żadnej z tych cech. Hub nie sprawdza nazwy hosta, kotwicy zaufania ani unieważnienia. To nadal należy do klienta. Certyfikat klienta nie jest żądany.

Liść wygasły, jeszcze nieważny, słaby albo o złym przeznaczeniu przerywa start i `--check-config`, zanim otworzy się port. Komunikat nie zawiera certyfikatu ani klucza prywatnego. Katalogi US i PL mają te same zdania. Tekst pomocy pozostaje bez zmian. Po polsku diagnostyka ważności brzmi: `Certyfikat TLS nie jest ważny w chwili uruchomienia`.

## Szyfry

TLS 1.2 oferuje tylko ECDHE z AES-GCM albo ChaCha20-Poly1305. TLS 1.3 oferuje AES-128-GCM, AES-256-GCM i ChaCha20-Poly1305. Hub preferuje tę kolejność. Bilety sesji są wyłączone, a pamięć sesji serwera pozostaje wyłączona, więc późniejsze połączenie nie wznawia sesji. Zestawy CBC, anonimowe i inne niż ECDHE nie są oferowane.

## Bez zmian

Schemat v9 pozostaje bez zmian. Nie dodano licznika statusu, strony WebAdmin ani ustawienia Apache. Przykłady Apache zachowują własne listy szyfrów. Check status zostaje na GitHub-hosted Ubuntu 24.04. HTTP/2, OCSP i automatyczne odnawianie certyfikatu pozostają niezaimplementowane. To nie zamyka zagrożenia szczątkowego T15. Publiczna uwierzytelniona brama pozostaje osobną pracą.

`adcs.tls_security` przyjmuje aktualny liść RSA, P-256 i Ed25519, także liść bez rozszerzenia key usage. Odrzuca liść wygasły, liść z przyszłości, SHA-1, RSA-1024, secp256k1, extended key usage tylko dla klienta i key usage, które nie obsługuje TLS. `adcs.tls_security.process` sprawdza polską diagnostykę, odmowę przed nasłuchem, aktualne `--check-config`, odrzucenie CBC w TLS 1.2, przyjęcie GCM w TLS 1.2 i TLS 1.3 oraz brak wznowienia kolejnego połączenia.
