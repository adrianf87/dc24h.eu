#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace dc24h::accounts::detail {
inline constexpr std::uint64_t scrypt_n = 131072, scrypt_r = 8, scrypt_p = 1;
inline constexpr std::uint64_t scrypt_max_memory = 256ULL * 1024 * 1024;
inline constexpr std::string_view verifier_prefix = "$dc24h-scrypt$v=1,ln=17,r=8,p=1$";
inline constexpr std::size_t salt_bytes = 32, key_bytes = 32;
inline constexpr std::size_t verifier_bytes = verifier_prefix.size() + salt_bytes * 2 + 1 + key_bytes * 2;
// Fixed storage, no implicit copies, wiped on every exit. Internal to the
// registration implementation/tests; not exported by account snapshots/API.
struct EncodedPassword final {
    std::array<unsigned char,verifier_bytes> bytes{};
    EncodedPassword() = default;
    ~EncodedPassword();
    EncodedPassword(const EncodedPassword&) = delete;
    EncodedPassword& operator=(const EncodedPassword&) = delete;
};
void validate_password(std::string_view password);
void hash_password(std::string_view password, EncodedPassword& output);
// Strict fixed-cost v1 envelope only. Malformed/unsupported stored parameters
// fail before any KDF work. Inputs are borrowed; temporary decoded keys are wiped.
[[nodiscard]] bool verify_password(std::string_view password, std::string_view encoded);
} // namespace dc24h::accounts::detail
