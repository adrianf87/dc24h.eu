# Testing alpha-0.1.4

Dependencies: a C++20 compiler, CMake >= 3.22, OpenSSL development headers and command-line tool, utf8proc development library and Python 3.8+. ncdc is optional for its existing ADC HSUP smoke test.

```sh
sudo apt-get install build-essential cmake libssl-dev openssl libutf8proc-dev python3 ncdc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

`adcs.crypto` uses a disposable, locally generated self-signed certificate to verify a nonblocking OpenSSL handshake and decrypted ADC payload with an in-process client. `adcs.process` exercises real TLS over TCP, verifies the TLS version and ADC parser counters, mixed listener operation, invalid TLS peers, and missing/bad credentials. Its test client deliberately disables certificate verification **only for the temporary test certificate**; production clients must not do so. The established `adc.ncdc` smoke test still covers initial plaintext HSUP. A full `adcs://` login test with ncdc is deferred until outbound ADC handshake and session state exist.

Verify additionally with a trusted public certificate:

```sh
openssl s_client -connect localhost:1512 -servername your.hub.example \\
  -verify_hostname your.hub.example -verify_return_error
```

An `openssl s_client` handshake checks TLS transport only. Do not interpret it as a successful Direct Connect login.
