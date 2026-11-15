#include "registration_internal.hpp"
#include "dc24h/accounts/registration.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <utf8proc.h>

#include <algorithm>

namespace dc24h::accounts::detail {
namespace {
template<std::size_t N> struct Buffer {
    std::array<unsigned char,N> bytes{};
    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer() { OPENSSL_cleanse(bytes.data(),bytes.size()); }
};
}
EncodedPassword::~EncodedPassword() { OPENSSL_cleanse(bytes.data(),bytes.size()); }
void validate_password(std::string_view password) {
    if (password.size() > max_registration_password_bytes) {
        throw RegistrationError(RegistrationErrorKind::invalid_password);
    }
    std::size_t offset = 0, characters = 0;
    while (offset < password.size()) {
        utf8proc_int32_t codepoint{};
        const auto count = utf8proc_iterate(
            reinterpret_cast<const utf8proc_uint8_t*>(password.data()+offset),
            static_cast<utf8proc_ssize_t>(password.size()-offset),&codepoint);
        // Preserve accepted UTF-8 exactly: no trimming, normalization or casefold.
        if (count <= 0 || codepoint < 32 || codepoint == 127) {
            throw RegistrationError(RegistrationErrorKind::invalid_password);
        }
        offset += static_cast<std::size_t>(count); ++characters;
    }
    if (characters < min_password_characters) { throw RegistrationError(RegistrationErrorKind::invalid_password); }
}
void hash_password(std::string_view password, EncodedPassword& output) {
    // Do not retain a previous result when a new derivation fails.
    OPENSSL_cleanse(output.bytes.data(),output.bytes.size());
    validate_password(password);
    Buffer<salt_bytes> salt;
    Buffer<key_bytes> key;
    if (RAND_bytes(salt.bytes.data(),static_cast<int>(salt.bytes.size())) != 1 ||
        EVP_PBE_scrypt(password.data(),password.size(),salt.bytes.data(),salt.bytes.size(),
            scrypt_n,scrypt_r,scrypt_p,scrypt_max_memory,key.bytes.data(),key.bytes.size()) != 1) {
        throw RegistrationError(RegistrationErrorKind::crypto);
    }
    auto next = std::copy(verifier_prefix.begin(),verifier_prefix.end(),output.bytes.begin());
    constexpr char hex[] = "0123456789abcdef";
    for (auto byte : salt.bytes) { *next++ = static_cast<unsigned char>(hex[byte >> 4]); *next++ = static_cast<unsigned char>(hex[byte & 15]); }
    *next++ = '$';
    for (auto byte : key.bytes) { *next++ = static_cast<unsigned char>(hex[byte >> 4]); *next++ = static_cast<unsigned char>(hex[byte & 15]); }
}
} // namespace dc24h::accounts::detail
