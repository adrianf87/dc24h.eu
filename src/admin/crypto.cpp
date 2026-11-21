#include "dc24h/admin/crypto.hpp"

#include <argon2.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace dc24h::admin {
namespace {

constexpr unsigned memory_kib = 65536;
constexpr unsigned passes = 3;
constexpr unsigned lanes = 1;
constexpr auto hash_prefix = "$argon2id$v=19$m=65536,t=3,p=1$";

template <std::size_t Size> std::array<unsigned char, Size> random_bytes() {
    std::array<unsigned char, Size> bytes{};
    if (RAND_priv_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw std::runtime_error("Could not generate administrator credentials");
    }
    return bytes;
}

std::string hex(const std::array<unsigned char, 32> &bytes) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        encoded += digits[byte >> 4U];
        encoded += digits[byte & 15U];
    }
    return encoded;
}

std::string digest(std::string_view value) {
    std::array<unsigned char, 32> bytes{};
    if (SHA256(reinterpret_cast<const unsigned char *>(value.data()), value.size(), bytes.data()) ==
        nullptr) {
        throw std::runtime_error("Could not hash administrator session");
    }
    return hex(bytes);
}

} // namespace

bool valid_password(std::string_view password) noexcept {
    return !password.empty() && password.size() <= 128 &&
           password.find_first_of(std::string_view("\r\n\0", 3)) == password.npos;
}

std::string hash_password(std::string_view password) {
    if (password.size() < 12 || !valid_password(password)) {
        throw std::invalid_argument(
            "Administrator passwords must contain 12 to 128 bytes on one line");
    }
    const auto salt = random_bytes<16>();
    std::array<char, 160> encoded{};
    const auto result =
        argon2id_hash_encoded(passes, memory_kib, lanes, password.data(), password.size(),
                              salt.data(), salt.size(), 32, encoded.data(), encoded.size());
    if (result != ARGON2_OK) {
        throw std::runtime_error("Could not hash administrator password");
    }
    return encoded.data();
}

bool verify_password(std::string_view password, const std::string &hash) {
    // Only the parameters used by provisioning may determine verification cost.
    if (!hash.starts_with(hash_prefix)) {
        throw std::runtime_error("Unsupported administrator password hash");
    }
    const auto result = argon2id_verify(hash.c_str(), password.data(), password.size());
    if (result == ARGON2_VERIFY_MISMATCH) {
        return false;
    }
    if (result != ARGON2_OK) {
        throw std::runtime_error("Could not verify administrator password");
    }
    return true;
}

std::string random_token() { return hex(random_bytes<32>()); }

bool valid_token(std::string_view token) noexcept {
    return token.size() == 64 && std::ranges::all_of(token, [](char byte) {
               return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
           });
}

std::string session_digest(std::string_view token) { return digest(token); }

std::string csrf_token(std::string_view token) {
    return digest("dc24h-csrf-v1:" + std::string(token));
}

bool tokens_match(std::string_view offered, std::string_view expected) noexcept {
    return offered.size() == expected.size() &&
           CRYPTO_memcmp(offered.data(), expected.data(), expected.size()) == 0;
}

} // namespace dc24h::admin
