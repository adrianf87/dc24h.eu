#include "dc24h/adcs/tls.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <memory>

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
        struct ClosePair { int* fds; ~ClosePair() { close(fds[0]); close(fds[1]); } } cleanup{pair};
        dc24h::adcs::Connection server(context, pair[0]);
        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> client_context(SSL_CTX_new(TLS_client_method()), SSL_CTX_free);
        require(client_context != nullptr, "client TLS context failed");
        SSL_CTX_set_verify(client_context.get(), SSL_VERIFY_NONE, nullptr); // test-only self-signed cert
        std::unique_ptr<SSL, decltype(&SSL_free)> client(SSL_new(client_context.get()), SSL_free);
        require(client != nullptr, "client TLS session failed");
        require(SSL_set_fd(client.get(), pair[1]) == 1, "client fd failed");
        SSL_set_connect_state(client.get());
        bool client_complete = false, server_complete = false;
        for (int i = 0; i < 100 && !(client_complete && server_complete); ++i) {
            if (!client_complete) {
                const auto status = SSL_connect(client.get());
                if (status == 1) { client_complete = true; }
                else {
                    const auto error = SSL_get_error(client.get(), status);
                    require(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE,
                            "client handshake failed");
                }
            }
            if (!server_complete) {
                // An unrelated operation on this thread may have left errors.
                ERR_put_error(ERR_LIB_USER, 0, 1, __FILE__, __LINE__);
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
        require(SSL_version(client.get()) >= TLS1_2_VERSION, "legacy TLS was negotiated");
        const char payload[] = "HSUP ADBASE\n";
        require(SSL_write(client.get(), payload, sizeof(payload) - 1) == sizeof(payload) - 1,
                "client write failed");
        char buffer[100]{};
        ERR_put_error(ERR_LIB_USER, 0, 1, __FILE__, __LINE__);
        const auto result = server.receive(buffer, sizeof(buffer));
        require(result.status == dc24h::adcs::IoStatus::complete, "server read failed");
        require(std::string(buffer, result.bytes) == payload, "decrypted bytes differ");
        std::cout << "ADCS OpenSSL handshake and application data OK\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
