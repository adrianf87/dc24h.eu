#include "dc24h/adcs/tls.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <openssl/ssl.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "expected cert and key paths");
        dc24h::adcs::Context context(argv[1], argv[2]);
        int pair[2];
        require(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, pair) == 0,
                "socketpair failed");
        dc24h::adcs::Connection server(context, pair[0]);
        SSL_CTX* client_context = SSL_CTX_new(TLS_client_method());
        require(client_context != nullptr, "client TLS context failed");
        SSL_CTX_set_verify(client_context, SSL_VERIFY_NONE, nullptr); // test-only self-signed cert
        SSL* client = SSL_new(client_context);
        require(client != nullptr, "client TLS session failed");
        require(SSL_set_fd(client, pair[1]) == 1, "client fd failed");
        SSL_set_connect_state(client);
        bool client_complete = false, server_complete = false;
        for (int i = 0; i < 100 && !(client_complete && server_complete); ++i) {
            if (!client_complete) {
                const auto status = SSL_connect(client);
                if (status == 1) { client_complete = true; }
                else {
                    const auto error = SSL_get_error(client, status);
                    require(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE,
                            "client handshake failed");
                }
            }
            if (!server_complete) {
                const auto result = server.handshake();
                if (result.status == dc24h::adcs::IoStatus::complete) { server_complete = true; }
                else {
                    require(result.status == dc24h::adcs::IoStatus::want_read ||
                            result.status == dc24h::adcs::IoStatus::want_write,
                            "server handshake failed");
                }
            }
        }
        require(client_complete && server_complete, "handshake did not complete");
        require(SSL_version(client) >= TLS1_2_VERSION, "legacy TLS was negotiated");
        const char payload[] = "HSUP ADBASE\n";
        require(SSL_write(client, payload, sizeof(payload) - 1) == sizeof(payload) - 1,
                "client write failed");
        char buffer[100]{};
        const auto result = server.receive(buffer, sizeof(buffer));
        require(result.status == dc24h::adcs::IoStatus::complete, "server read failed");
        require(std::string(buffer, result.bytes) == payload, "decrypted bytes differ");
        SSL_free(client);
        SSL_CTX_free(client_context);
        close(pair[0]);
        close(pair[1]);
        std::cout << "ADCS OpenSSL handshake and application data OK\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
