#pragma once

#include <array>
#include <cstdint>

namespace dc24h::search {
// Public Tiger Tree Hash root supplied by a file descriptor, not a CID/PID,
// Tiger(file bytes) digest or proof that the caller possesses the file.
// All 192-bit values are representable; absence is a separate optional value.
// ADC's canonical Base32 codec remains in the protocol adapter.
using TthRoot = std::array<std::uint8_t, 24>;
} // namespace dc24h::search
