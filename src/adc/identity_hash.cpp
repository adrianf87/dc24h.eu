#include "dc24h/adc/identity_hash.hpp"

#include <gcrypt.h>
#include <algorithm>
#include <memory>
#include <type_traits>

namespace dc24h::adc {
namespace { constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"; }

std::optional<TigerDigest> parse_tiger_base32(std::string_view text) noexcept {
    if (text.size() != 39) { return std::nullopt; }
    TigerDigest digest{};
    unsigned bits = 0;
    std::uint32_t buffer = 0;
    std::size_t index = 0;
    for (const char character : text) {
        const auto digit = alphabet.find(character);
        if (digit == std::string_view::npos) { return std::nullopt; }
        buffer = (buffer << 5U) | static_cast<std::uint32_t>(digit);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            digest[index++] = static_cast<std::uint8_t>(buffer >> bits);
            buffer &= (1U << bits) - 1U;
        }
    }
    if (buffer != 0) { return std::nullopt; }
    return digest;
}

std::string tiger_base32(const TigerDigest& digest) {
    std::string text;
    text.reserve(39);
    unsigned bits = 0;
    std::uint32_t buffer = 0;
    for (const auto byte : digest) {
        buffer = (buffer << 8U) | byte;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            text += alphabet[(buffer >> bits) & 31U];
        }
        buffer &= (1U << bits) - 1U;
    }
    if (bits != 0) { text += alphabet[(buffer << (5U - bits)) & 31U]; }
    return text;
}

std::optional<TigerDigest> tiger192(std::span<const std::uint8_t> bytes) {
    static const bool initialized = gcry_check_version("1.6.0") != nullptr;
    if (!initialized || gcry_md_test_algo(GCRY_MD_TIGER1) != 0) { return std::nullopt; }
    gcry_md_hd_t raw = nullptr;
    if (gcry_md_open(&raw, GCRY_MD_TIGER1, 0) != 0) { return std::nullopt; }
    const std::unique_ptr<std::remove_pointer_t<gcry_md_hd_t>, decltype(&gcry_md_close)> context(raw, &gcry_md_close);
    if (!bytes.empty()) { gcry_md_write(raw, bytes.data(), bytes.size()); }
    gcry_md_final(raw);
    const auto* result = gcry_md_read(raw, GCRY_MD_TIGER1);
    if (!result) { return std::nullopt; }
    TigerDigest digest{};
    std::copy_n(result, digest.size(), digest.begin());
    return digest;
}
} // namespace dc24h::adc
