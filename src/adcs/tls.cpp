#include "dc24h/adcs/tls.hpp"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string_view>

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

void require_current(const X509* certificate) {
    const ASN1_TIME* before = X509_get0_notBefore(certificate);
    const ASN1_TIME* after = X509_get0_notAfter(certificate);
    // -1 means the ASN.1 time is at or before now. 1 means it is later. 0 is an error.
    if (before == nullptr || after == nullptr || X509_cmp_current_time(before) != -1 ||
        X509_cmp_current_time(after) != 1) {
        throw std::runtime_error("TLS certificate is not valid at startup");
    }
}

void require_leaf(X509* leaf) {
    if (leaf == nullptr) { throw std::runtime_error("TLS certificate is not valid at startup"); }
    require_current(leaf);
    switch (X509_get_signature_nid(leaf)) {
    case NID_sha256WithRSAEncryption:
    case NID_sha384WithRSAEncryption:
    case NID_sha512WithRSAEncryption:
    case NID_ecdsa_with_SHA256:
    case NID_ecdsa_with_SHA384:
    case NID_ecdsa_with_SHA512:
    case NID_ED25519:
    case NID_ED448:
        break;
    default:
        throw std::runtime_error("TLS certificate signature is too weak");
    }
    EVP_PKEY* key = X509_get0_pubkey(leaf);
    if (key == nullptr) { throw std::runtime_error("TLS certificate public key is too weak"); }
    bool strong = false;
    switch (EVP_PKEY_get_base_id(key)) {
    case EVP_PKEY_RSA:
        strong = EVP_PKEY_get_bits(key) >= 2048;
        break;
    case EVP_PKEY_EC: {
        char name[64];
        std::size_t length = 0;
        if (EVP_PKEY_get_group_name(key, name, sizeof name, &length) == 1 && length < sizeof name) {
            const std::string_view group(name, length);
            strong = group == "prime256v1" || group == "secp384r1" || group == "secp521r1";
        }
        break;
    }
    case EVP_PKEY_ED25519:
    case EVP_PKEY_ED448:
        strong = true;
        break;
    default:
        break;
    }
    if (!strong) { throw std::runtime_error("TLS certificate public key is too weak"); }
    constexpr std::uint32_t absent = 0xffffffffu;
    constexpr std::uint32_t listener_usage = static_cast<std::uint32_t>(KU_DIGITAL_SIGNATURE) |
                                             static_cast<std::uint32_t>(KU_KEY_ENCIPHERMENT) |
                                             static_cast<std::uint32_t>(KU_KEY_AGREEMENT);
    const std::uint32_t usage = X509_get_key_usage(leaf);
    if (usage != absent && (usage & listener_usage) == 0) {
        throw std::runtime_error("TLS certificate key usage does not allow a hub listener");
    }
    const std::uint32_t extended = X509_get_extended_key_usage(leaf);
    if (extended != absent && (extended & static_cast<std::uint32_t>(XKU_SSL_SERVER)) == 0) {
        throw std::runtime_error("TLS certificate is not authorized for server authentication");
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
        SSL_CTX_set_options(context_, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION |
                                          SSL_OP_NO_TICKET | SSL_OP_CIPHER_SERVER_PREFERENCE);
        // Never let unattended startup prompt on a terminal for a PEM password.
        SSL_CTX_set_default_passwd_cb(context_, [](char*, int, int, void*) { return 0; });
        SSL_CTX_set_session_cache_mode(context_, SSL_SESS_CACHE_OFF);
        if (SSL_CTX_set_cipher_list(context_,
                "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
                "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
                "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305") != 1 ||
            SSL_CTX_set_ciphersuites(context_,
                "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256") != 1) {
            throw std::runtime_error("Cannot set TLS cipher policy");
        }
        if (SSL_CTX_use_certificate_chain_file(context_, certificate_chain.c_str()) != 1) {
            throw std::runtime_error("Cannot load TLS certificate chain");
        }
        if (SSL_CTX_use_PrivateKey_file(context_, private_key.c_str(), SSL_FILETYPE_PEM) != 1) {
            throw std::runtime_error("Cannot load TLS private key");
        }
        if (SSL_CTX_check_private_key(context_) != 1) {
            throw std::runtime_error("TLS certificate and private key do not match");
        }
        require_leaf(SSL_CTX_get0_certificate(context_));
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
