#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace dc24h::adc {
using TigerDigest = std::array<std::uint8_t, 24>;
// Canonical unpadded uppercase base32: 39 symbols, three unused bits zero.
// Direct destination is cleared on failure; it must not overlap encoded input.
[[nodiscard]] bool decode_tiger_base32(std::string_view text, TigerDigest& destination) noexcept;
[[nodiscard]] std::optional<TigerDigest> parse_tiger_base32(std::string_view text) noexcept;
[[nodiscard]] std::string tiger_base32(const TigerDigest& digest);
// Standard Tiger/192 (TIGER1/NESSIE byte order), not Tiger2 or a Merkle tree.
// Does not alter global crypto modes. Direct form clears destination on failure
// and supports input/output aliasing; false means no digest was produced.
[[nodiscard]] bool tiger192_into(std::span<const std::uint8_t> bytes, TigerDigest& destination) noexcept;
// Compatibility value form: empty on unavailable algorithm/context/result.
[[nodiscard]] std::optional<TigerDigest> tiger192(std::span<const std::uint8_t> bytes);
} // namespace dc24h::adc
