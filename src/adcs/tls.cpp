#include "dc24h/adcs/tls.hpp"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <climits>
#include <stdexcept>

namespace dc24h::adcs {
namespace {
[[nodiscard]] IoResult classify(SSL* ssl, int result) noexcept {
    switch (SSL_get_error(ssl, result)) {
    case SSL_ERROR_WANT_READ: return {IoStatus::want_read};
    case SSL_ERROR_WANT_WRITE: return {IoStatus::want_write};
    case SSL_ERROR_ZERO_RETURN: return {IoStatus::closed};
    default: return {IoStatus::error};
    }
}
} // namespace

Context::Context(const std::string& certificate_chain, const std::string& private_key) {
    if (certificate_chain.empty() || private_key.empty() ||
        certificate_chain.find('\0') != std::string::npos ||
        private_key.find('\0') != std::string::npos) {
        throw std::invalid_argument("ADCS needs both --tls-cert and --tls-key");
    }
    ERR_clear_error();
    context_ = SSL_CTX_new(TLS_server_method());
    if (context_ == nullptr) { throw std::runtime_error("Cannot initialize TLS context"); }
    try {
        if (SSL_CTX_set_min_proto_version(context_, TLS1_2_VERSION) != 1) {
            throw std::runtime_error("Cannot set minimum TLS version");
        }
        SSL_CTX_set_options(context_, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
        // Never let unattended startup prompt on a terminal for a PEM password.
        SSL_CTX_set_default_passwd_cb(context_, [](char*, int, int, void*) { return 0; });
        SSL_CTX_set_session_cache_mode(context_, SSL_SESS_CACHE_OFF);
        if (SSL_CTX_use_certificate_chain_file(context_, certificate_chain.c_str()) != 1) {
            throw std::runtime_error("Cannot load TLS certificate chain");
        }
        if (SSL_CTX_use_PrivateKey_file(context_, private_key.c_str(), SSL_FILETYPE_PEM) != 1) {
            throw std::runtime_error("Cannot load TLS private key");
        }
        if (SSL_CTX_check_private_key(context_) != 1) {
            throw std::runtime_error("TLS certificate and private key do not match");
        }
    } catch (...) {
        SSL_CTX_free(context_);
        context_ = nullptr;
        throw;
    }
}

Context::~Context() { SSL_CTX_free(context_); }

Connection::Connection(const Context& context, int fd) {
    ERR_clear_error();
    ssl_ = SSL_new(context.context_);
    if (ssl_ == nullptr) { throw std::runtime_error("Cannot initialize TLS connection"); }
    if (SSL_set_fd(ssl_, fd) != 1) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        throw std::runtime_error("Cannot attach TLS socket");
    }
    SSL_set_accept_state(ssl_);
}

Connection::~Connection() { SSL_free(ssl_); }

IoResult Connection::handshake() noexcept {
    ERR_clear_error();
    const int result = SSL_accept(ssl_);
    return result == 1 ? IoResult{IoStatus::complete} : classify(ssl_, result);
}

IoResult Connection::receive(char* buffer, std::size_t capacity) noexcept {
    if (capacity == 0 || capacity > static_cast<std::size_t>(INT_MAX)) {
        return {IoStatus::error};
    }
    std::size_t bytes = 0;
    ERR_clear_error();
    const int result = SSL_read_ex(ssl_, buffer, capacity, &bytes);
    return result == 1 ? IoResult{IoStatus::complete, bytes} : classify(ssl_, result);
}

bool Connection::pending() const noexcept { return SSL_pending(ssl_) > 0; }

IoResult Connection::send(const char* buffer, std::size_t size) noexcept {
    if (size == 0 || size > static_cast<std::size_t>(INT_MAX)) { return {IoStatus::error}; }
    std::size_t bytes = 0;
    ERR_clear_error();
    const int result = SSL_write_ex(ssl_, buffer, size, &bytes);
    return result == 1 ? IoResult{IoStatus::complete, bytes} : classify(ssl_, result);
}

} // namespace dc24h::adcs
