# Testowanie ADCS

```sh
sudo apt-get install build-essential cmake libssl-dev openssl libutf8proc-dev python3 ncdc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

`adcs.crypto` sprawdza handshake i odszyfrowane dane przy pomocy tymczasowego certyfikatu. `adcs.process` sprawdza prawdziwe gniazda TLS, równoległy TCP, błędny handshake oraz walidację certyfikatów i klucza. Wyłączenie weryfikacji certyfikatu dotyczy wyłącznie testowego klienta używającego certyfikatu samopodpisanego. Klienci produkcyjni muszą weryfikować tożsamość serwera.

Test `adc.ncdc` sprawdza wyłącznie pierwsze `HSUP` przez TCP. Pełny test logowania `adcs://` przy użyciu ncdc wymaga kolejnych etapów obsługi sesji ADC.
