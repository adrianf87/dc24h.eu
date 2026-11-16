#include "registration_internal.hpp"
#include "dc24h/accounts/registration.hpp"
#include "dc24h/accounts/password_security.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>

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
bool verify_password(std::string_view password, std::string_view encoded) {
    validate_password(password);
    constexpr auto salt_offset = verifier_prefix.size();
    constexpr auto separator = salt_offset+salt_bytes*2;
    if (encoded.size() != verifier_bytes || !encoded.starts_with(verifier_prefix) || encoded[separator] != '$') {
        throw PasswordSecurityError(PasswordSecurityErrorKind::invalid_record);
    }
    Buffer<salt_bytes> salt;
    Buffer<key_bytes> expected;
    Buffer<key_bytes> actual;
    const auto nibble = [](char value) -> unsigned char {
        if (value >= '0' && value <= '9') { return static_cast<unsigned char>(value-'0'); }
        if (value >= 'a' && value <= 'f') { return static_cast<unsigned char>(value-'a'+10); }
        throw PasswordSecurityError(PasswordSecurityErrorKind::invalid_record);
    };
    const auto decode = [&](std::size_t offset, auto& output) {
        for (std::size_t i = 0; i < output.size(); ++i) {
            output[i] = static_cast<unsigned char>((nibble(encoded[offset+i*2]) << 4) | nibble(encoded[offset+i*2+1]));
        }
    };
    decode(salt_offset,salt.bytes);
    decode(separator+1,expected.bytes);
    // No cost, algorithm or output length is taken from the stored record.
    if (EVP_PBE_scrypt(password.data(),password.size(),salt.bytes.data(),salt.bytes.size(),
        scrypt_n,scrypt_r,scrypt_p,scrypt_max_memory,actual.bytes.data(),actual.bytes.size()) != 1) {
        throw RegistrationError(RegistrationErrorKind::crypto);
    }
    return CRYPTO_memcmp(actual.bytes.data(),expected.bytes.data(),actual.bytes.size()) == 0;
}
} // namespace dc24h::accounts::detail
